// components/webserver/include/webserver.h

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Инициализирует буферы веб-страниц из SPIFFS
 * @return ESP_OK при успехе
 */

typedef struct {
    const char *ssid;
    const char *password;
    uint8_t channel;
    uint8_t max_connections;
} webserver_ap_config_t;

/*
extern char index_html[4096];
extern char style_css[4096 + 512];
extern char script_js[4096 + 4096 + 1024];
*/

esp_err_t webserver_init_buffers(void);

/**
 * @brief Инициализирует Wi-Fi в режиме SoftAP и запускает HTTP/WebSocket сервер.
 *
 * Должен вызываться ПОСЛЕ nvs_flash_init() и esp_netif_init().
 *
 * @param config  Конфигурация точки доступа (не NULL)
 * @return        ESP_OK при успехе
 */
esp_err_t webserver_start(const webserver_ap_config_t *confg);

/**
 * @brief Асинхронная отправка состояния лампы по WebSocket
 * @param server Хэндлер сервера
 */
void webserver_broadcast_lamp_state(httpd_handle_t server);

#endif // WEBSERVER_H