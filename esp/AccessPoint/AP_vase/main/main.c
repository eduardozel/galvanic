#include <stdio.h>

#include <stdlib.h>
#include <string.h>

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

#define test_gpio 6

#define LED_PIN 8
#define led_on  0
#define led_off 1


#define EXAMPLE_ESP_WIFI_SSID      "vase" // CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      "gvc123456" // CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   5 // CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       3 // CONFIG_ESP_MAX_STA_CONN



httpd_handle_t server = NULL;
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static const char *TAG = "AP vase";

static int led_state = 0;

static int total_seconds[2];
static const int tick = 10;

#define INDEX_HTML_PATH "/spiffs/start.html"
#define STYLE_CSS_PATH  "/spiffs/styles.css"

static char index_html[8192+4096];
char style_css[2048];

static char response_data[8192+4096];


static void STOP( uint8_t ch ){
  total_seconds[ch]= 0;
  offAllLED();
}

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

void task_counter(void *arg) {
    while (1) {
		if        ( total_seconds[0]   > tick ) { total_seconds[0] -= tick;
		} else if ( total_seconds[0]  == tick ) { STOP(0);
		}
		if        ( total_seconds[1]   > tick ) { total_seconds[1] -= tick;
		} else if ( total_seconds[1]  == tick ) { STOP(1);
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
}; // init_spiffs

static void initi_web_page_buffer(void)
{
    memset((void *)index_html, 0, sizeof(index_html));
	memset((void *)style_css,  0, sizeof(style_css));
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
/*
    if (stat(STYLE_CSS_PATH, &st))
    {
        ESP_LOGE(TAG, "styles.css not found");
        return;
    }
	
    fp = fopen(STYLE_CSS_PATH, "r");
    if (fread(style_css, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed for styles.css");
    }
    fclose(fp);
*/
} // initi_web_page_buffer

esp_err_t style_handler(httpd_req_t *req) {
    FILE *file = fopen(STYLE_CSS_PATH, "r");
    if (!file) {
        ESP_LOGE("style_handler", "Failed to open styles.css");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
	fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    ESP_LOGI(TAG, "------------------styles.css size: %ld", size);
    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
} // style_handler


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
    int fd = resp_arg->fd;

	uint32_t voltage1 = 23;//adc1_get_raw(ADC_v1);  // int analogVolts = analogReadMilliVolts(2);
	uint32_t voltage2 = 43;//adc1_get_raw(ADC_v2);

	uint32_t current1 = 100;//adc1_get_raw(ADC_c1);
	uint32_t current2 = 200;//adc1_get_raw(ADC_c2);

	
	// (0-4095 -> 0-2.5 V)
	float v1 = (voltage1 / 4095.0) * 2.5; 
	float v2 = (voltage2 / 4095.0) * 2.5;

	// (0-4095 -> 0-2.5 V)
	float c1 = (current1 / 1.0); 
	float c2 = (current2 / 1.0);


    char buffer[128];// = getVoltJS();
    snprintf(buffer, sizeof(buffer), "{\"v1\": %.1f, \"v2\": %.1f, \"c1\": %.1f, \"c2\": %.1f, \"timer1\": \"%d\", \"timer2\": \"%d\" }", v1, v2, c1, c2, total_seconds[0], total_seconds[1] );
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
				int chanS = -1;
				int chanF = -1;
				const char *action = cJSON_GetObjectItem(json, "act")->valuestring;
				if (strcmp(action, "start1") == 0) {
					chanS =0;
				} else if (strcmp(action, "start2") == 0) {
					chanS =1;
				} else if (strcmp(action, "stop1") == 0) {
					chanF = 0;
				} else if (strcmp(action, "stop2") == 0) {
					chanF = 1;
				} else if (strcmp(action, "getState") == 0) {
					return trigger_async_send(req->handle, req);
				}
				if ( chanS >= 0 ) {
					const char *dsVAL = cJSON_GetObjectItem(json, "val")->valuestring;//->valueint;
					const char *DUR   = cJSON_GetObjectItem(json, "duration")->valuestring;//->valueint;
					int dsIDX = atoi(dsVAL);
                    if ( 0 == chanS ) {
	                  setAllLED(255, 210, 150 );
					} else {
	                  setAllLED(  0, 210, 0 );
					}
					int duration = atoi(DUR)*60;
					total_seconds[chanS] = duration;
					ESP_LOGI(TAG, "start val: %s <idx> %d <duration> %d", dsVAL, dsIDX, duration); //duration
				};
				if ( chanF >= 0 ) {
					STOP(chanF);
				}
			} // if (json)
			cJSON_Delete(json);
		}
    }
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

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &ws);
		httpd_register_uri_handler(server, &style_uri);
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
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    init_spiffs();

    total_seconds[0] = 0;
	total_seconds[1] = 0;

    init_led();
	initWS2812();

    xTaskCreate(&task_blink_led, "BlinkLed",  4096, NULL, 5, NULL);
	xTaskCreate(&task_counter,   "countdown", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
//	
	ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
	initi_web_page_buffer();
	setup_websocket_server();
	
	setAllLED( 0, 0, 150 );
    vTaskDelay( 2000 );
	offAllLED();
} // main