//
// webserver.c
// esp32c3
// esp-idf v 5.5.1
//
#include "webserver.h"
#include <stdio.h>
#include <stdint.h>

#include <string.h>
#include <sys/stat.h>

#include <esp_http_server.h>
#include "esp_log.h"

#include "esp_mac.h"
#include "esp_wifi.h"


#include "esp_event.h"


#include "lwip/err.h"
#include "lwip/sys.h"

#include "cJSON.h"

#include "DS1803.h"
#include "GV.h"

static const char *TAG = "webserver";

#define EXAMPLE_MAX_STA_CONN       3 // CONFIG_ESP_MAX_STA_CONN

#define INDEX_HTML_PATH "/spiffs/start.html"
#define STYLE_CSS_PATH  "/spiffs/styles.css"
#define SCRIPT_JS_PATH  "/spiffs/script.js"

webserver_ap_config_t ap_cfg = {0};

static char index_html[4096];
static char script_js[4096 + 1024];
static char style_css[4096 + 1024];

static esp_netif_t *ap_netif = NULL;

httpd_handle_t server = NULL;

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};


static esp_err_t load_file_to_buffer( const char *path
                                    , char *buffer
									, size_t buf_size
) {
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG, "File not found: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    if (st.st_size >= buf_size) {
        ESP_LOGE(TAG, "File too large: %s (%ld bytes)", path, st.st_size);
        return ESP_ERR_NO_MEM;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return ESP_FAIL;
    }

    size_t bytes_read = fread(buffer, 1, st.st_size, fp);
    fclose(fp);

    if ( bytes_read != st.st_size) {
        ESP_LOGE(TAG, "Failed to read %s", path);
        return ESP_FAIL;
    }

    buffer[bytes_read] = '\0';
    ESP_LOGI(TAG, "Loaded %s (%d bytes)", path, (int)bytes_read);
    return ESP_OK;
} // load_file_to_buffer

esp_err_t webserver_init_buffers(void) {
    ESP_LOGI(TAG, "Loading web assets from SPIFFS...");

    memset(index_html, 0, sizeof(index_html));
    memset(style_css,  0, sizeof(style_css));
    memset(script_js,  0, sizeof(script_js));

    esp_err_t err;
    if ((err = load_file_to_buffer("/spiffs/start.html", index_html, sizeof(index_html))) != ESP_OK) return err;
    if ((err = load_file_to_buffer("/spiffs/styles.css", style_css,  sizeof(style_css)))  != ESP_OK) return err;
    if ((err = load_file_to_buffer("/spiffs/script.js",  script_js,  sizeof(script_js)))  != ESP_OK) return err;

    return ESP_OK;
} // webserver_init_buffers

esp_err_t js_handler(httpd_req_t *req) {
    if (script_js[0] == '\0') {
        ESP_LOGE(TAG, "script.js not loaded");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    size_t js_len = strlen(script_js);
    ESP_LOGI(TAG, "Serving script.js from memory, size: %d", js_len);
    httpd_resp_set_type(req, "application/javascript");
    //httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600"); //cache 1 hour;
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache"); // no-cache for tests
    return httpd_resp_send(req, script_js, js_len);
} // js_handler


esp_err_t style_handler(httpd_req_t *req) {
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
//    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");  // Кэш на 1 час; no-cache для тестов.
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");  // no-cache для тестов.
//    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate")

    esp_err_t ret = httpd_resp_send(req, style_css, css_len);     // Отправляем данные одним куском (поскольку файл маленький)
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send styles.css: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
} // style_handler


static esp_err_t get_req_handler(httpd_req_t *req) {
    // Обработка GET-запроса для корневого пути
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t not_found_handler(httpd_req_t *req) {
    ESP_LOGE(TAG, "404: URI not found: %s", req->uri);
    httpd_resp_send_404(req);
    return ESP_OK;
} // not_found_handler


static esp_err_t cfg_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Sending config values+++++++++++++++++++++++++++++++++++++");
    
    char json_buf[512];
    int  offset = 0;
    
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, 
                       "{\"values\":[");
    
    for (int i = 0; i < MAX_VALUES; i++) {
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset,
                           "%u%s", Hvalues[i], (i < MAX_VALUES - 1) ? "," : "");
    } // for
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, 
                       "],\"display\":[");
    
    for (int i = 0; i < MAX_VALUES; i++) {
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset,
                           "\"%s\"%s", Hdisplay[i], (i < MAX_VALUES - 1) ? "," : "");
    } // for
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "]}");

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
} // cfg_values_handler


static void wifi_event_handler( void* arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void*   event_data
) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
} // wifi_event_handler

// = = = = = =

static void ws_async_send(void *arg)
{
  httpd_ws_frame_t ws_pkt;
  struct async_resp_arg *resp_arg = arg;
  httpd_handle_t hd = resp_arg->hd;

	uint32_t voltage1 = 1; // adc1_get_raw(ADC_v1);  // int analogVolts = analogReadMilliVolts(2);
	uint32_t voltage2 = 1; // adc1_get_raw(ADC_v2);

	uint32_t current1 = 5; // adc1_get_raw(ADC_c1);
	uint32_t current2 = 5; // adc1_get_raw(ADC_c2);

	
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
						DS1803_set( 1, dsIDX);
					} else {
						DS1803_set( 0, dsIDX);
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
// - - - - - - - -

void set_wifi_tx_power() {
    int8_t max_tx_power = 1; // 80 -> 20 dBm  ; 1 -> 0.25 dBm
    esp_wifi_set_max_tx_power(max_tx_power);
}

// - - - - - - - - - - -

esp_err_t webserver_start(const webserver_ap_config_t *AP_cfg) {
  if (!AP_cfg) {
    ESP_LOGE(TAG, "Invalid config");
    return ESP_ERR_INVALID_ARG;
  }
  ESP_LOGI(TAG, "---------------SSID>%s<password>%s", AP_cfg->ssid, AP_cfg->password);
  set_wifi_tx_power();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ap_netif = esp_netif_create_default_wifi_ap();
  assert(ap_netif);	
	
  wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_event_handler_instance_register( WIFI_EVENT,
                                                       ESP_EVENT_ANY_ID,
                                                       &wifi_event_handler,
 													   NULL,
 													   NULL));

   wifi_config_t wifi_cfg = {0};
   size_t ssid_len = strlen(AP_cfg->ssid);
   if (ssid_len == 0 || ssid_len > 32) {
     ESP_LOGE(TAG, "Invalid SSID length");
     return ESP_ERR_INVALID_ARG;
   }
   memcpy(wifi_cfg.ap.ssid, AP_cfg->ssid, ssid_len);
   wifi_cfg.ap.ssid_len = ssid_len;

   if ( strlen(AP_cfg->password) > 0) {
        if (strlen(AP_cfg->password) < 8) {
            ESP_LOGE(TAG, "Password too short (<8 chars)>%s<>%s", AP_cfg->password, AP_cfg->ssid);
            return ESP_ERR_INVALID_ARG;
        }
        strncpy((char *)wifi_cfg.ap.password, AP_cfg->password, sizeof(wifi_cfg.ap.password) - 1);
        wifi_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
   } else {
        wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
   }
   ESP_LOGI(TAG, "SSID>%s<password>%s", AP_cfg->ssid, AP_cfg->password);

   wifi_cfg.ap.channel = AP_cfg->channel;
   wifi_cfg.ap.max_connection = AP_cfg->max_connections;
   wifi_cfg.ap.pmf_cfg.required = false;

   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
   ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
   ESP_ERROR_CHECK(esp_wifi_start());

   ESP_LOGI(TAG, "SoftAP started: SSID='%s', channel=%d", AP_cfg->ssid, AP_cfg->channel);
// - * - * - * - * 

//    static httpd_handle_t server = NULL;
    if (server) return ESP_ERR_INVALID_STATE;

    httpd_config_t httpd_cfg = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &httpd_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTPD");
        return ESP_FAIL;
    }

    httpd_uri_t uris[] = {
        {.uri = "/",              .method = HTTP_GET, .handler = get_req_handler,   .user_ctx = NULL},
        {.uri = "/styles.css",    .method = HTTP_GET, .handler = style_handler,     .user_ctx = NULL},
        {.uri = "/script.js",     .method = HTTP_GET, .handler = js_handler,        .user_ctx = NULL},
        {.uri = "/config_values", .method = HTTP_GET, .handler = cfg_handler,       .user_ctx = NULL},
        {.uri = "/ws",            .method = HTTP_GET, .handler = handle_ws_req,     .user_ctx = NULL, .is_websocket = true},
        {.uri = "/*",             .method = HTTP_GET, .handler = not_found_handler, .user_ctx = NULL}
//        {.uri = "/upload",    .method = HTTP_POST,.handler = upload_handler,    .user_ctx = NULL}
    };

    for (size_t i = 0; i < sizeof(uris)/sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }

    ESP_LOGI(TAG, "Web server started on http://<ip>/");
    return ESP_OK;
} // webserver_start

