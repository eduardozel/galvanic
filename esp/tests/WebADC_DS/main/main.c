#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "cJSON.h"

#include "spi_flash_mmap.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "driver/adc.h"
//#include "esp_adc/adc_oneshot.h"
//#include "driver/gpio.h"

#include <esp_http_server.h>
#include "connect_wifi.h"

#include "DS1803.h"

#define LED_PIN 8

// https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html#analogsetattenuation
// https://microsin.net/programming/arm/esp32-adc.html?ysclid=m4pxyiamqf966518188
// https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32/api-reference/peripherals/adc_oneshot.html
// https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/adc.html

#define ADC_CHANNEL_1 ADC1_CHANNEL_0 // GPIO 0
#define ADC_CHANNEL_2 ADC1_CHANNEL_1 // GPIO 1

httpd_handle_t server = NULL;
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static const char *TAG = "WebADC"; // TAG for debug
int led_state = 0;

#define INDEX_HTML_PATH "/spiffs/index.html"
#define STYLE_CSS_PATH  "/spiffs/styles.css"

char index_html[8192];
char style_css[4096];

char response_data[4096];

static void init_adc() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL_1, ADC_ATTEN_DB_11); // ADC_ATTEN_DB_11 0 mV ~ 2500 mV;  ADC_ATTEN_DB_6 0 mV ~ 1300 mV -- ADC_ATTEN_DB_12
    adc1_config_channel_atten(ADC_CHANNEL_2, ADC_ATTEN_DB_11);
}


static void initi_web_page_buffer(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    memset((void *)index_html, 0, sizeof(index_html));
	memset((void *)style_css,  0, sizeof(style_css));
    struct stat st;
    if (stat(INDEX_HTML_PATH, &st))
    {
        ESP_LOGE(TAG, "index.html not found");
        return;
    }

    FILE *fp = fopen(INDEX_HTML_PATH, "r");
    if (fread(index_html, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);

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
}

esp_err_t style_handler(httpd_req_t *req) {
    FILE *file = fopen(STYLE_CSS_PATH, "r");
    if (!file) {
        ESP_LOGE("style_handler", "Failed to open style.css");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

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

static void ws_async_send(void *arg)
{
    httpd_ws_frame_t ws_pkt;
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;

	uint32_t voltage1 = adc1_get_raw(ADC_CHANNEL_1);  // int analogVolts = analogReadMilliVolts(2);
	uint32_t voltage2 = adc1_get_raw(ADC_CHANNEL_2);
	
	// Преобразуем в вольты (0-4095 -> 0-2.5 В)
	float v1 = (voltage1 / 4095.0) * 2.5; 
	float v2 = (voltage2 / 4095.0) * 2.5;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "{\"channel1\": %.1f, \"channel2\": %.1f, \"state\": %d}", v1, v2, led_state );
    ESP_LOGI(TAG, "voltage: %s", buffer);

    led_state = !led_state;
    gpio_set_level(LED_PIN, led_state);
    
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

    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
		if ( strcmp((char *)ws_pkt.payload, "toggle") == 0) {
          free(buf);
          return trigger_async_send(req->handle, req);
		} else {
			cJSON *json = cJSON_Parse((char *)ws_pkt.payload);
			if (json) {
				const char *action = cJSON_GetObjectItem(json, "act")->valuestring;
				if (strcmp(action, "start1") == 0) {
					int dsVAL = cJSON_GetObjectItem(json, "val")->valueint;
					ESP_LOGI(TAG, "start1 val: %d", dsVAL);
				}
			} // if (json)
		}
    }
    return ESP_OK;
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

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &ws);
    }

    return server;
}

void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_adc();
	DS1803_init();
	httpd_uri_t style_uri = {
		.uri = "/styles.css",
		.method = HTTP_GET,
		.handler = style_handler, // Убедитесь, что у вас есть обработчик для этого URI
		.user_ctx = NULL
	};
    httpd_register_uri_handler(server, &style_uri);
    connect_wifi();
// WiFi.setTxPower(WIFI_POWER_8_5dBm);
    if (wifi_connect_status)
    {
        gpio_reset_pin(LED_PIN);
        gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

        led_state = 0;
        ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
        initi_web_page_buffer();
        setup_websocket_server();
    }
}