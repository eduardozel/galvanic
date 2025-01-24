#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"

#define EXAMPLE_ESP_WIFI_SSID      "galvanica" 
#define EXAMPLE_ESP_WIFI_PASS      "gvc123456" 
#define EXAMPLE_MAX_STA_CONN       2

static const char *TAG = "WiFi_AP";

httpd_handle_t server = NULL;

// Обработчик GET-запросов
static esp_err_t get_handler(httpd_req_t *req) {
    const char* response = "Hello from ESP32!";
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Настройка веб-сервера
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
    }
    return server;
}

// Функция для поиска свободного канала
int find_free_channel() {
    uint16_t num = 0;
    wifi_ap_record_t ap_info[20]; // Массив для хранения информации о найденных точках доступа

    // Сканируем доступные сети
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_records(&num, ap_info);

    // Проверяем занятые каналы
    bool channels[13] = {false}; // Массив для отслеживания занятых каналов
    for (int i = 0; i < num; i++) {
        if (ap_info[i].primary > 0 && ap_info[i].primary <= 13) {
            channels[ap_info[i].primary - 1] = true; // Отметить канал как занятый
        }
    }

    // Ищем первый свободный канал
    for (int channel = 2; channel <= 13; channel++) {
        if (!channels[channel - 1]) {
            return channel; // Возвращаем первый свободный канал
        }
    }

    return 1; // Возвращаем канал 1, если все заняты
}

// Инициализация Wi-Fi в режиме точки доступа
void wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel =  7, //find_free_channel(), // Поиск свободного канала 
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
//??            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    ESP_LOGI(TAG, "WiFi SoftAP started. SSID: %s, Password: %s, Channel: %d", 
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, wifi_config.ap.channel);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_softap();         // Инициализация Wi-Fi
    start_webserver();          // Настройка веб-сервера
}