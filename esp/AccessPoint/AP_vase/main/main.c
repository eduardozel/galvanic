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

#include "lamp.h"

#define BTN_1 GPIO_NUM_6  //  SIG на GPIO6

#define LED_PIN 8
#define led_on  0
#define led_off 1

static QueueHandle_t button_queue;  // +++ Очередь для прерываний
typedef enum {
    EVENT_BUTTON_1,   // Событие от TTP223_1
    EVENT_BUTTON_2    // Событие от TTP223_2
} button_event_t;


#define EXAMPLE_ESP_WIFI_SSID      "vase_1" // CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      "vase23456" // CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   6 // CONFIG_ESP_WIFI_CHANNEL 1 6 11
#define EXAMPLE_MAX_STA_CONN       3 // CONFIG_ESP_MAX_STA_CONN

httpd_handle_t server = NULL;
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static const char *TAG = "AP vase";


static char wifi_ssid[32]     = {0};
static char wifi_password[64] = {0};

typedef struct {
    uint8_t ssid[32];
    uint8_t password[64];
} wifi_ap_cfg_t;

static int led_state = 0;

static const int tick = 10;

#define INDEX_HTML_PATH "/spiffs/start.html"
#define STYLE_CSS_PATH  "/spiffs/styles.css"
#define SCRIPT_JS_PATH  "/spiffs/script.js"

static char index_html[4096];
static char style_css[4096];
static char script_js[4096+4096]; 

//static char response_data[8192+4096];
/***********************/

void monitor_wifi_status() {
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "RSSI: %d dBm", ap_info.rssi);
    }
} // monitor_wifi_status

// - - - -  -

/* * * * * */
// +++ Обработчик прерывания (касание)
static void IRAM_ATTR touch_isr_handler(void* arg) {
    BaseType_t higher_priority_task_woken = pdFALSE;
	uint32_t btn = BTN_1;
    xQueueSendFromISR( button_queue, &btn, &higher_priority_task_woken);
}

// Задача для обработки касаний
static void touch_task(void* arg) {
//    button_event_t event;
    uint32_t btn;
//    static uint64_t last_time_on = 0;   // Для debounce TTP223
//    static uint64_t last_time_off = 0;  // Для debounce кнопки OFF

    while (1) {
//        if (xQueueReceive(button_queue, NULL, portMAX_DELAY)) {
        if (xQueueReceive(button_queue, &btn, portMAX_DELAY)) {
//            uint64_t current_time = esp_timer_get_time();

            if (btn == BTN_1) {
//            switch (event) {
//                case EVENT_BUTTON_1:
//                    break;
//                default:
//                    ESP_LOGW(TAG, "Неизвестное событие: %d", event);
//                    break;
//            }; // switch 
              ESP_LOGI(TAG, "Touch detected");
//            start_rainbow();
              if (!LAMP_on) {
                LAMP_turn_On();
              } else {
				LAMP_turn_Off();
              } // if LAMP_on
            };
        } // if
    } // while
}

/* * * * * * */
void task_blink_led(void *arg) {
    while (1) {
        gpio_set_level(LED_PIN, led_off);
        vTaskDelay(  500 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, led_on);
        vTaskDelay( 1000 / portTICK_PERIOD_MS);
    }
} // task_blink_led

static void init_led(){
    gpio_reset_pin(LED_PIN);
    gpio_set_direction( LED_PIN, GPIO_MODE_OUTPUT);
	led_state = 0;
    xTaskCreate(&task_blink_led, "BlinkLed",  4096, NULL, 5, NULL);
} // init_led

// * * * * *

//  / * / * / *
void task_counter(void *arg) {
    while (1) {
//      if ( total_seconds > 0 ) {
		if        ( total_seconds   > tick ) { total_seconds -= tick;
		} else if ( total_seconds  == tick ) { LAMP_turn_Off();
		}
//      } // if 
		vTaskDelay( 10000 / portTICK_PERIOD_MS);
    } // while
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
	ESP_LOGI(TAG, "=== === === ===  initi_web_page_buffer ... ... ... ...\n");

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
		return;
	} else { 
		if (st.st_size >= sizeof(index_html)) {
			ESP_LOGE(TAG, "index.html too large (%ld bytes), buffer is %d", st.st_size, sizeof(index_html));
			return;
        } else {
           ESP_LOGI(TAG, "Loaded start.html, size: %ld", st.st_size);
		};
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
        ESP_LOGI(TAG, "Loaded script.js,  size: %d", js_size);
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
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");  // Кэш на 1 час; no-cache для тестов.
//    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");  // no-cache для тестов.
//    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate")

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
    httpd_resp_set_type(req, "application/javascript");
    //httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600"); // Кэш на 1 час;
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache"); // no-cache для тестов.
    return httpd_resp_send(req, script_js, js_len);
} // js_handler

static esp_err_t get_req_handler(httpd_req_t *req) {
    // Обработка GET-запроса для корневого пути
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
/* * * * * */
static esp_err_t upload_handler(httpd_req_t *req) {
    char buf[1024];
    esp_err_t ret;
    size_t remaining = req->content_len;

    FILE *fd = fopen("/spiffs/led_sequence.cfg", "w");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        size_t buf_len = sizeof(buf) < remaining ? sizeof(buf) : remaining;
        ret = httpd_req_recv(req, buf, buf_len);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(fd);
            ESP_LOGE(TAG, "File receive failed");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (fwrite(buf, 1, ret, fd) != ret) {
            fclose(fd);
            ESP_LOGE(TAG, "File write failed");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    fclose(fd);
    ESP_LOGI(TAG, "File uploaded and saved as /spiffs/led_sequence.cfg");
    const char *resp = "File uploaded successfully";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* - - - - */
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

	int len = lamp_settings_json(buffer, sizeof(buffer));
    if (len < 0 || len >= sizeof(buffer)) {
        ESP_LOGE(TAG, "Failed to format JSON or buffer too small");
        return;
    }
    ESP_LOGI(TAG, "msg: %s", buffer);
    
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)buffer;
    ws_pkt.len     = len;
    ws_pkt.type    = HTTPD_WS_TYPE_TEXT;

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
    ESP_LOGI(TAG, "===================== handle_ws_req");
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

    ESP_LOGI(TAG, "Frame length: %d", ws_pkt.len);
	if (ws_pkt.len > 512) {
        ESP_LOGE(TAG, "Frame too long: %d", ws_pkt.len);
        return ESP_FAIL;
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

    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

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
					monitor_wifi_status();
					lamp_settings_from_json(json);
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
		.uri = "/*",
		.method = HTTP_GET,
		.handler = not_found_handler,
		.user_ctx = NULL
	};

    httpd_uri_t upload_uri = {
        .uri = "/upload",
        .method = HTTP_POST,
        .handler = upload_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &ws);
		httpd_register_uri_handler(server, &style_uri);
		httpd_register_uri_handler(server, &js_uri);
		httpd_register_uri_handler(server, &not_found_uri);
        httpd_register_uri_handler(server, &upload_uri);
    }

    return server;
} // setup_websocket_server
//- - - - - -
void set_wifi_tx_power() {
    int8_t max_tx_power = 1; // 80 -> 20 dBm  ; 1 -> 0.25 dBm
    esp_wifi_set_max_tx_power(max_tx_power);
}
// --
esp_err_t read_wifi_config_from_file(void) {
    ESP_LOGI(TAG, "Reading WiFi configuration from SPIFFS");
//    strlcpy(wifi_ssid, EXAMPLE_ESP_WIFI_SSID, sizeof(wifi_ssid));

    FILE* file = fopen("/spiffs/wifi.cfg", "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open wifi_config.txt, using default values");
        return ESP_FAIL;
    }

    char temp_ssid[32] = {0};

    if (fgets(temp_ssid, sizeof(temp_ssid), file) == NULL) {
        ESP_LOGE(TAG, "Failed to read SSID");
        fclose(file);
        return ESP_FAIL;
    }
    temp_ssid[strcspn(temp_ssid, "\r")] = 0;
	strcpy(wifi_ssid,     temp_ssid);
    ESP_LOGI(TAG, "Read SSID: %s<<!!!", wifi_ssid);

    char temp_password[64] = {0};
    if (fgets(temp_password, sizeof(temp_password), file) == NULL) {
        ESP_LOGE(TAG, "Failed to read password");
        fclose(file);
        return ESP_FAIL;
    }
    temp_password[strcspn(temp_password, "\r")] = 0;
	strcpy(wifi_password, temp_password);
    ESP_LOGI(TAG, "Read Password: %s<<!!!", wifi_password);

    fclose(file);
    return ESP_OK;
} // read_wifi_config_from_file
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
//            .ssid = EXAMPLE_ESP_WIFI_SSID,
//            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .ssid_len = strlen(wifi_ssid),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
//            .password = EXAMPLE_ESP_WIFI_PASS,
            .password = "",
            .max_connection = EXAMPLE_MAX_STA_CONN,

#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else  CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
/*
    if (strlen(wifi_password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }
*/
            .pmf_cfg = {
                    .required = true,
            },
        },
    }; // wifi_config_t 
	strncpy((char*)wifi_config.ap.ssid, wifi_ssid, sizeof(wifi_config.ap.ssid));
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
	strlcpy((char*)wifi_config.ap.password, wifi_password, sizeof(wifi_config.ap.password));

ESP_LOGI(TAG, "wifi_init_softap ??????? SSID:%s<>password:%s<>channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, EXAMPLE_ESP_WIFI_CHANNEL);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20)); // ??
    ESP_ERROR_CHECK(esp_wifi_start());

/*
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"));
    esp_netif_set_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    esp_netif_dhcps_start(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"));
*/
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_ssid/*EXAMPLE_ESP_WIFI_SSID*/, wifi_password, EXAMPLE_ESP_WIFI_CHANNEL);
}
// ***************
void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    init_spiffs();

    init_led();
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


    vTaskDelay(pdMS_TO_TICKS(10));
    int button_state = gpio_get_level(BTN_1);
    if (button_state == 1) {
      ESP_LOGI(TAG, "+++ ++ ++ ++  btn ... ... ... ...\n");
    } else {
      ESP_LOGI(TAG, "--- -- -- --  btn ... ... ... ...\n");
	};


    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_1, touch_isr_handler, NULL);

	ESP_LOGI(TAG, "              btn ... handler ... ...\n");
	
//    button_queue = xQueueCreate(10, 0); // Создание очереди (10 элементов, каждый — button_event_t)
    button_queue = xQueueCreate(10, sizeof(button_event_t));
    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Ошибка создания очереди!");
        return;
    }
    xTaskCreate(touch_task, "touch_task", 2048, NULL, 10, NULL);


    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    if (read_wifi_config_from_file() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WiFi configuration!");
        return;
    }
    wifi_init_softap();

	ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
	initi_web_page_buffer();
	ESP_LOGI(TAG, "+++ +++ +++ +++  setup_websocket_server ... ... ... ...\n");
	setup_websocket_server();

    LAMP_init();
} // main