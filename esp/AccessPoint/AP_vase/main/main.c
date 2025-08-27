#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <inttypes.h> // заголовок для макросов формата

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "cJSON.h"

#include "spi_flash_mmap.h"
#include "esp_spiffs.h"

#include "nvs_flash.h"

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include <esp_http_server.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/gpio.h"


#include "WS2812.h"

#define BTN_1 GPIO_NUM_6  // TTP223 SIG на GPIO6

#define LED_PIN 8
#define led_on  0
#define led_off 1

static bool LAMP_on = false;

typedef enum {
    white   = 0,
    rainbow = 1,
    custom  = 2
} LAMP_state_t;

static const char *lamp_state_to_str(LAMP_state_t s)
{
    switch (s) {
    case white:   return "white";
    case rainbow: return "rainbow";
    case custom:  return "custom";
    default:      return "unknown";
    }
}

static LAMP_state_t lamp_state = white;
static rgb_t custom_color;
static int brightness = 4;


static QueueHandle_t button_queue;  // +++ Очередь для прерываний

static TaskHandle_t rainbow_task_handle = NULL;
static bool rainbow_active = false;

#define EXAMPLE_ESP_WIFI_SSID      "vase" // CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      "vase23456" // CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   5 // CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       3 // CONFIG_ESP_MAX_STA_CONN

/*
#define DEFAULT_MAX_TIME 60     // max_time (сек), если не в файле

static uint32_t max_time    = DEFAULT_MAX_TIME;  // Максимальное время свечения (сек)
static uint32_t remain_time = DEFAULT_MAX_TIME;  // Оставшееся время (сек)
*/

httpd_handle_t server = NULL;
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static const char *TAG = "AP vase";

static int led_state = 0;

static int total_seconds;
static const int tick = 10;
static int current_duration = 5*60;


#define INDEX_HTML_PATH "/spiffs/start.html"
#define STYLE_CSS_PATH  "/spiffs/styles.css"
#define SCRIPT_JS_PATH  "/spiffs/script.js"
#define CONFIG_FILE     "/spiffs/config.txt"

static char index_html[8192+4096];
static char style_css[4096];
static char script_js[4096]; 

//static char response_data[8192+4096];
/***********************/
// - - - -  -

static void rainbow_task(void *arg) {
    float hue = 0.0f;
    float step = 360.0f / 32.0f; // 32 steps for full rainbow
    int direction = 1; // 1: forward, -1: backward

    while (rainbow_active) {
        // Calculate RGB from HSV (S=1, V=1 max brightness)
        rgb_t color = hsv2rgb(hue, 1.0f, 1.0f);
        setAllLED(color );

        hue += direction * step;
        if (hue >= 360.0f) {
            hue = 360.0f;
            direction = -1;
        } else if (hue <= 0.0f) {
            hue = 0.0f;
            direction = 1;
        }

        vTaskDelay(pdMS_TO_TICKS( 400));
    }
    vTaskDelete(NULL);
} // rainbow_task


void start_rainbow(void) {
    if (rainbow_active) return; // Already running
    rainbow_active = true;
    xTaskCreate(rainbow_task, "rainbow_task", 2048, NULL, 5, &rainbow_task_handle);
} // start_rainbow

void stop_rainbow(void) {
    if (!rainbow_active) return;
    rainbow_active = false;
    if (rainbow_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait a bit for task to exit loop
        rainbow_task_handle = NULL;
    }
} // stop_rainbow
/*==================*/
//-----------------
static void LAMP_turn_On(void){
	total_seconds = current_duration;
	if ( white == lamp_state ) {
	  ESP_LOGI(TAG, "white");
	  fade_in_warm_white( brightness );
	} else if ( custom == lamp_state ) {
	  ESP_LOGI(TAG, "custom\nred=%d",custom_color.red);
      setAllLED( custom_color );
	} else if ( rainbow == lamp_state ) {
	  ESP_LOGI(TAG, "rainbow");
	  start_rainbow();
	} else {
	  ESP_LOGE(TAG, "uncnown lamp_state");
	};
	LAMP_on = true;
}; // LAMPon

static void LAMP_turn_Off( void ){
  total_seconds = 0;
  stop_rainbow();
  offAllLED();
  LAMP_on = false;
} // LAMP_turn_Off
/* * * * * */
// +++ Обработчик прерывания (касание)
static void IRAM_ATTR touch_isr_handler(void* arg) {
    BaseType_t higher_priority_task_woken = pdFALSE;
	uint32_t btn = BTN_1;
    xQueueSendFromISR( button_queue, &btn, &higher_priority_task_woken);
}

// Задача для обработки касаний
static void touch_task(void* arg) {
    uint32_t btn;
//    static uint64_t last_time_on = 0;   // Для debounce TTP223
//    static uint64_t last_time_off = 0;  // Для debounce кнопки OFF

    while (1) {
//        if (xQueueReceive(button_queue, NULL, portMAX_DELAY)) {
        if (xQueueReceive(button_queue, &btn, portMAX_DELAY)) {
//            uint64_t current_time = esp_timer_get_time();

//            if (btn == BTN_1) {
            ESP_LOGI(TAG, "Touch detected");
//            start_rainbow();
            if (!LAMP_on) {
              LAMP_turn_On();
            } else {
			  offAllLED();
              LAMP_on = false;
            } // if LAMP_on
        } // if
    } // while
}

/* * * * * * */
static void init_led(){
    gpio_reset_pin(LED_PIN);
    gpio_set_direction( LED_PIN, GPIO_MODE_OUTPUT);
	led_state = 0;
} // init_led

void task_blink_led(void *arg) {
    while (1) {
        gpio_set_level(LED_PIN, led_off);
        vTaskDelay(  500 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, led_on);
        vTaskDelay( 1000 / portTICK_PERIOD_MS);
    }
} // task_blink_led
// * * * * * * * * 

// * * * *  *
static void write_config_file(void) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "brightness=%d\nduration=%d\n", brightness, current_duration ); //    fprintf(f, "max_time=%" PRIu32 "\nremain_time=%" PRIu32 "\n", max_time, remain_time);
    fprintf(f, "red=%d\ngreen=%d\nblue=%d\n", custom_color.red, custom_color.green, custom_color.blue);
    ESP_LOGI(TAG, "Config written:\n brightness=%d\nduration=%d\n", brightness, current_duration);
	fprintf(f, "lamp_state=%d\n",(int)lamp_state);
    fclose(f);
} // write_config_file

static void read_config_file(void) {
	ESP_LOGI(TAG, "read_config_file");
    FILE *f = fopen(CONFIG_FILE, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "File not found, using defaults");
        current_duration = 5;
        brightness = 3;
        write_config_file();  // Создаём файл с дефолтами
        return;
    }

    char line[64];

    while (fgets(line, sizeof(line), f)) {
		int tmp;
        if (sscanf(line, "brightness=%d", &brightness) == 1) {                       // if (sscanf(line, "max_time=%" SCNu32, &max_time) == 1) {
            ESP_LOGI(TAG, "Read brightness=%d", brightness);                         //     ESP_LOGI(TAG, "Read max_time=%" PRIu32, max_time);
        } else if (sscanf(line, "duration=%d", &current_duration) == 1) {
            ESP_LOGI(TAG, "Read current_duration=%d", current_duration);
        } else if (sscanf(line, "lamp_state=%d", &tmp) == 1) {
			lamp_state = (LAMP_state_t)tmp;
            ESP_LOGI(TAG, "Read lamp_state=%d", lamp_state);
        }
    } // while
    fclose(f);
} // read_config_file

//  / * / * / *
void task_counter(void *arg) {
    while (1) {
		if        ( total_seconds   > tick ) { total_seconds -= tick;
		} else if ( total_seconds  == tick ) { LAMP_turn_Off();
		}
		vTaskDelay( 10000 / portTICK_PERIOD_MS);
    }
} // task_counter

void init_spiffs() {
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_conf));
/*

    // Инициализация SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }
*/
}; // init_spiffs

static void initi_web_page_buffer(void)
{
    memset((void *)index_html, 0, sizeof(index_html));
    struct stat st;
    if (stat(INDEX_HTML_PATH, &st))
    {
        ESP_LOGE(TAG, "not found : start.html");
        return;
    }

    FILE *fp = fopen(INDEX_HTML_PATH, "r");
    if (fread(index_html, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);
// CSS ----
    memset((void *)style_css, 0, sizeof(style_css));
    if (stat(STYLE_CSS_PATH, &st))
    {
        ESP_LOGE(TAG, "Failed to open %s", STYLE_CSS_PATH);
        return;
    }

    if (st.st_size >= sizeof(style_css)) {
        ESP_LOGE(TAG, "styles.css too large (%ld bytes), buffer is %d", st.st_size, sizeof(style_css));
        return;
    }

    fp = fopen(STYLE_CSS_PATH, "r");
    size_t css_size = fread(style_css, 1, st.st_size, fp);
//    if (fread(style_css, st.st_size, 1, fp) == 0) {
	if (css_size == 0) {
        ESP_LOGE(TAG, "fread failed for styles.css");
    } else {
        style_css[css_size] = '\0';  // Нулевой терминатор (не обязательно для CSS, но полезно для strlen)
        ESP_LOGI(TAG, "Loaded styles.css, size: %d", css_size);
    } // if
    fclose(fp);
// JS
    memset((void *)script_js, 0, sizeof(script_js));
    if (stat(SCRIPT_JS_PATH, &st)) {
        ESP_LOGE(TAG, "not found: %s", SCRIPT_JS_PATH);
        return;
    }
    if (st.st_size >= sizeof(script_js)) {
        ESP_LOGE(TAG, "script.js too large (%ld bytes)", st.st_size);
        return;
    }
    fp = fopen(SCRIPT_JS_PATH, "r");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", SCRIPT_JS_PATH);
        return;
    }
    size_t js_size = fread(script_js, 1, st.st_size, fp);
    if (js_size == 0) {
        ESP_LOGE(TAG, "fread failed for script.js");
    } else {
        script_js[js_size] = '\0';
        ESP_LOGI(TAG, "Loaded script.js, size: %d", js_size);
    }
    fclose(fp);

} // initi_web_page_buffer


esp_err_t style_handler(httpd_req_t *req) {
    // Проверяем, загружен ли CSS в память
    ESP_LOGI(TAG, "---------style_handler ---------styles.css");
    if (style_css[0] == '\0') {  // Если буфер пустой (не загружен)
        ESP_LOGE(TAG, "styles.css not loaded in memory");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    size_t css_len = strlen(style_css);  // Длина из буфера (благодаря \0)
    ESP_LOGI(TAG, "Serving styles.css from memory, size: %d", css_len);

     httpd_resp_set_type(req, "text/css");    // Устанавливаем тип контента (важно для браузера!)

    // Опционально: заголовки для кэша (чтобы браузер не запрашивал каждый раз)
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");  // Кэш на 1 час

    esp_err_t ret = httpd_resp_send(req, style_css, css_len);     // Отправляем данные одним куском (поскольку файл маленький)
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send styles.css: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
} // style_handler


esp_err_t js_handler(httpd_req_t *req) {
    if (script_js[0] == '\0') {
        ESP_LOGE(TAG, "script.js not loaded");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    size_t js_len = strlen(script_js);
    ESP_LOGI(TAG, "Serving script.js from memory, size: %d", js_len);
    httpd_resp_set_type(req, "application/javascript");  // Тип для JS
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
    return httpd_resp_send(req, script_js, js_len);
} // js_handler

static esp_err_t get_req_handler(httpd_req_t *req) {
    // Обработка GET-запроса для корневого пути
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/*
esp_err_t get_req_handler(httpd_req_t *req)
{
    int response;
    if(led_state)
    {
        sprintf(response_data, index_html, "ON");
    }
    else
    {
        sprintf(response_data, index_html, "OFF");
    }
    response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);
    return response;
}
*/

static void ws_async_send(void *arg)
{
    httpd_ws_frame_t ws_pkt;
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
//    int fd = resp_arg->fd;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "{\"timer\": %d, \"red\": %d }", total_seconds, custom_color.red );
    ESP_LOGI(TAG, "msg: %s", buffer);
    
    
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)buffer;
    ws_pkt.len = strlen(buffer);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);

    if (ret != ESP_OK) {
        return;
    }

    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(hd, client_fds[i], &ws_pkt);
        }
    }
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}

static esp_err_t handle_ws_req(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    if (ws_pkt.len)
    {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }

//??    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
		if ( strcmp((char *)ws_pkt.payload, "toggle") == 0) {
          free(buf);
          return trigger_async_send(req->handle, req);
		} else {
			cJSON *json = cJSON_Parse((char *)ws_pkt.payload);
			free(buf);
			if (json) {
				bool cmdStart = false;
				bool cmdStop  = false;
				const char *action = cJSON_GetObjectItem(json, "act")->valuestring;
				if (strcmp(action, "start") == 0) {
					cmdStart = true;
				} else if (strcmp(action, "stop") == 0) {
					cmdStop = true;
				} else if (strcmp(action, "getState") == 0) {
					return trigger_async_send(req->handle, req);
				}

				if ( cmdStart  ) {
					cJSON *duration_item = cJSON_GetObjectItem(json, "duration");
					int duration = 5;

					if (duration_item != NULL) {
						if (cJSON_IsNumber(duration_item)) {
							int duration = duration_item->valueint * 60; // minutes to seconds
							current_duration = duration;
						} else {
							const char *dstr = duration_item->valuestring;
							ESP_LOGE("JSON", "Duration is not a number> %s", dstr);
						}
					} else {
						ESP_LOGE("JSON", "Duration item not found in JSON");
					}

					const cJSON *mode_item = cJSON_GetObjectItem(json, "mode");
					if (mode_item != NULL && mode_item->type == cJSON_String) {
						const char *mode = mode_item->valuestring;
						if (strcmp(mode, "white") == 0) {
							lamp_state = white;
						} else if (strcmp(mode, "custom") == 0) {
							lamp_state = custom;
						}
					} else {
					};

					const cJSON *red_item = cJSON_GetObjectItem(json, "red");
					if (red_item != NULL ) {
						ESP_LOGI("red item", "+");
						if (cJSON_IsNumber(red_item)) {
						  ESP_LOGI("red item", "+++");
						  custom_color.red = red_item->valueint;
						} else {
							const char *r_str = red_item->valuestring;
							ESP_LOGE("JSON", "red is not a number> %s", r_str);
							custom_color.red = 0;
						};
					} else {
					};

					const cJSON *green_item = cJSON_GetObjectItem(json, "green");
					if (green_item != NULL ) {
						if (cJSON_IsNumber(green_item)) {
						  custom_color.green = green_item->valueint;
						} else {
							custom_color.green = 0;
						};
					} else {
					};

					const cJSON *blue_item = cJSON_GetObjectItem(json, "blue");
					if (blue_item != NULL ) {
						if (cJSON_IsNumber(blue_item)) {
						  custom_color.blue = blue_item->valueint;
						} else {
							custom_color.green = 0;
						};
					} else {
					};

					const char *dsBR = cJSON_GetObjectItem(json, "brightness")->valuestring;
                    brightness = atoi(dsBR);
					write_config_file();
					LAMP_turn_On();

					ESP_LOGI(TAG, "start brightness: %d <duration> %d", brightness, duration);
				}; // cmdStart
				if ( cmdStop ) {
					LAMP_turn_Off();
				}
			} // if (json)
			cJSON_Delete(json);
		}
    }
    return ESP_OK;
}

esp_err_t not_found_handler(httpd_req_t *req) {
    ESP_LOGE(TAG, "404: URI not found: %s", req->uri);
    httpd_resp_send_404(req);
    return ESP_OK;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

httpd_handle_t setup_websocket_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_req_handler,
        .user_ctx = NULL};

    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handle_ws_req,
        .user_ctx = NULL,
        .is_websocket = true};
		
	httpd_uri_t style_uri = {
		.uri = "/styles.css",
		.method = HTTP_GET,
		.handler = style_handler,
		.user_ctx = NULL
	};

	httpd_uri_t js_uri = {
		.uri = "/script.js",
		.method = HTTP_GET,
		.handler = js_handler,
		.user_ctx = NULL
	};

	httpd_uri_t not_found_uri = {
		.uri = "/*",  // Для всех URI
		.method = HTTP_GET,
		.handler = not_found_handler,
		.user_ctx = NULL
	};

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &ws);
		httpd_register_uri_handler(server, &style_uri);
		httpd_register_uri_handler(server, &js_uri);
		httpd_register_uri_handler(server, &not_found_uri);
    }

    return server;
} // setup_websocket_server
//- - - - - -
void set_wifi_tx_power() {
    int8_t max_tx_power = 1; // 80 -> 20 dBm  ; 1 -> 0.25 dBm
    esp_wifi_set_max_tx_power(max_tx_power);
}
// --
void wifi_init_softap(void)
{
	set_wifi_tx_power();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,

#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
//            .authmode = WIFI_AUTH_WPA3_PSK,
//            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
//            .authmode = WIFI_AUTH_WPA2_PSK,
#endif

            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20)); // ??
    ESP_ERROR_CHECK(esp_wifi_start());


    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}
// ***************
void app_main()
{
	ESP_LOGI(TAG, "... ... ... ... MAIN ... ... ... ...\n");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    init_spiffs();

    total_seconds = 0;

    init_led();
	initWS2812();

    xTaskCreate(&task_blink_led, "BlinkLed",  4096, NULL, 5, NULL);
	xTaskCreate(&task_counter,   "countdown", 4096, NULL, 5, NULL);


    gpio_config_t io_conf = {
//        .mode = GPIO_MODE_OUTPUT,
//        .pin_bit_mask = (1ULL << LED_GPIO)
    };


    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BTN_1);
    io_conf.intr_type = GPIO_INTR_POSEDGE;  // Rising edge
    io_conf.pull_down_en = 1;               // Pull-down для стабильности
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_1, touch_isr_handler, NULL);

    button_queue = xQueueCreate(10, 0);
    xTaskCreate(touch_task, "touch_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

	ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
	initi_web_page_buffer();
	setup_websocket_server();
	read_config_file();
	
	fade_in_warm_white( brightness );
    vTaskDelay( 200 );
	offAllLED();
} // main