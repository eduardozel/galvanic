// esp32c3 esp-idf v 5.5.1
//
// config.c
//
#include <stdint.h>
#include "esp_spiffs.h"

#include "config.h"
#include "DS1803.h"


#define VALUES_PATH "/spiffs/values.cfg"
#define WiFi_PATH   "/spiffs/wifi.cfg"

static const char *TAG = "read config";


void init_spiffs() {
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_conf));
}; // init_spiffs

// DS1803
void read_values() {
  uint8_t  value;
  FILE *f = fopen(VALUES_PATH, "r");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file %s for reading", VALUES_PATH);
    return;
  }
  char line[128];
  uint8_t ln = 0;
  fgets(line, sizeof(line), f);
  fgets(line, sizeof(line), f);
  while (fgets(line, sizeof(line), f) != NULL && ln < 2) {
      char *token = strtok(line, ",");
      uint8_t vn = 0;
      while (token != NULL && vn < MAX_VALUES) {
        value = (uint8_t)atoi(token);
        if        ( 0 == ln ) { values0[vn] = value;
        } else if ( 1 == ln ) { values1[vn] = value;
        }
        token = strtok(NULL, ",");
        vn++;
      } // while token
      ln++;
  } // while line
  fclose(f);
/**/
	printf("\nvalues0:\n");
	for (int i = 0; i < MAX_VALUES; i++) {
	  printf("[%d]:%d<>", i, values0[i]);
  }
	printf("\n");
	for (int i = 0; i < MAX_VALUES; i++) {
	  printf("[%d]:%d<>", i, values1[i]);
  }
	printf("\n");
/**/
} // read_values

// = - = - = - = - webserver read_wifi_config - = - = - = - =
esp_err_t read_wifi_config( webserver_ap_config_t *cfg
) {
	if (cfg == NULL) {
      ESP_LOGE(TAG, "Config structure pointer is NULL");
      return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Reading WiFi configuration from SPIFFS");

    FILE* file = fopen(WiFi_PATH, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open wifi.cfg, using default values");
        return ESP_FAIL;
    }

    char line[64] = {0};

    // --- SSID ---
    if (fgets(line, sizeof(line), file) == NULL) {
        ESP_LOGE(TAG, "Failed to read SSID");
        fclose(file);
        return ESP_FAIL;
    }
    line[strcspn(line, "\r\n")] = 0;
    strncpy(cfg->ssid, line, sizeof(cfg->ssid) - 1);

    // --- PASSWORD ---
    memset(line, 0, sizeof(line));
    if (fgets(line, sizeof(line), file) == NULL) {
        ESP_LOGE(TAG, "Failed to read PASSWORD");
        fclose(file);
        return ESP_FAIL;
    }
    line[strcspn(line, "\r\n")] = 0;
    strncpy(cfg->password, line, sizeof(cfg->password) - 1);

    // --- CHANNEL ---
    memset(line, 0, sizeof(line));
    if (fgets(line, sizeof(line), file) == NULL) {
        ESP_LOGE(TAG, "Failed to read CHANNEL");
        fclose(file);
        return ESP_FAIL;
    }
    cfg->channel = (uint8_t)atoi(line);

    cfg->max_connections = 3;
    fclose(file);
    return ESP_OK;
} // read_wifi_config


// * * * *  * * 
/*

void read_config_values(void) {
    FILE *f = fopen(VALUES_PATH, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "File not found: %s, using defaults", VALUES_PATH);
        
        for (int i = 0; i < MAX_VALUES; i++) {
            Hvalues[i] = (uint8_t)i;
            snprintf(Hdisplay_buf[i], DISPLAY_STR_LEN, "%.1f", i * 0.2f);
            Hdisplay[i] = Hdisplay_buf[i];
        }
        return;
    }
    
    char line[256];
    
    // Hvalues 
    if (fgets(line, sizeof(line), f) != NULL) {
        if (line[0] != '#') {
            char *token = strtok(line, ",");
            int idx = 0;
            while (token != NULL && idx < MAX_VALUES) {
                Hvalues[idx] = (uint8_t)atoi(token);
                ESP_LOGD(TAG, "Hvalues[%d] = %d", idx, Hvalues[idx]);
                token = strtok(NULL, ",");
                idx++;
            }
        }
    }
    
    // Hdisplay
    if (fgets(line, sizeof(line), f) != NULL) {
        if (line[0] != '#') {
            char *token = strtok(line, ",");
            int idx = 0;
            while (token != NULL && idx < MAX_VALUES) {
                token[strcspn(token, "\r\n")] = '\0';
                
                strncpy(Hdisplay_buf[idx], token, DISPLAY_STR_LEN - 1);
                Hdisplay_buf[idx][DISPLAY_STR_LEN - 1] = '\0';
                
                Hdisplay[idx] = Hdisplay_buf[idx];
                
                ESP_LOGD(TAG, "Hdisplay[%d] = \"%s\"", idx, Hdisplay[idx]);
                token = strtok(NULL, ",");
                idx++;
            }
        }
    }
    
    fclose(f);
    ESP_LOGI(TAG, "Loaded %d values from %s", MAX_VALUES, VALUES_PATH);
    

}
*/
