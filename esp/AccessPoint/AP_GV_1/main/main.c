// esp32c3 esp-idf v 5.5.1
//
// main.c
//
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

#include "driver/adc.h"
#include "driver/ledc.h"
//#include "esp_adc/adc_oneshot.h"
//#include "driver/gpio.h"

// https://gorchilin.com/calculator/circuitjs

#include "webserver.h"
#include "DS1803.h"
#include "GV.h"

#define ADC_v1 ADC1_CHANNEL_0 // GPIO 0
#define ADC_v2 ADC1_CHANNEL_1 // GPIO 1
#define ADC_c1 ADC1_CHANNEL_2 // GPIO 2
#define ADC_c2 ADC1_CHANNEL_3 // GPIO 3

// gpio 4 RESET
#define brown_detect_gpio 5
#define test_gpio 6

#define BUZZER_PIN 7

#define LED_PIN 8
#define led_on  0
#define led_off 1

//#define I2C_MASTER_SCL_IO  9               /*!< gpio number for I2C master clock */
//#define I2C_MASTER_SDA_IO 10               /*!< gpio number for I2C master data  */


#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz


// https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html#analogsetattenuation
// https://microsin.net/programming/arm/esp32-adc.html?ysclid=m4pxyiamqf966518188
// https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32/api-reference/peripherals/adc_oneshot.html
// https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/adc.html


static const char *TAG = "AP galvanica";

static int led_state = 0;

static const int tick = 10;


static void IRAM_ATTR bd_isr_handler(void* arg) {
    DS1803_set( 1, 0);
    DS1803_set( 0, 0);
} // bd_isr_handler

static void init_adc() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_v1, ADC_ATTEN_DB_11); // ADC_ATTEN_DB_11 0 mV ~ 2500 mV;  ADC_ATTEN_DB_6 0 mV ~ 1300 mV -- ADC_ATTEN_DB_12
    adc1_config_channel_atten(ADC_v2, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_c1, ADC_ATTEN_DB_0);  // ADC_ATTEN_DB_0 0 mV ~ 750 mV
    adc1_config_channel_atten(ADC_c2, ADC_ATTEN_DB_0);
}

static void init_brown_detect(void)
{
    ESP_LOGI(TAG, "configured  brown detect gpio");
    gpio_reset_pin( brown_detect_gpio);
    gpio_set_direction( brown_detect_gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode( brown_detect_gpio, GPIO_PULLUP_ONLY);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(brown_detect_gpio, bd_isr_handler, NULL);
    gpio_set_intr_type(brown_detect_gpio, GPIO_INTR_POSEDGE);
	gpio_intr_enable(brown_detect_gpio);
} // brown_detect_gpio

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

// = - = - = - = - = - = - = - =
esp_err_t read_wifi_config(webserver_ap_config_t *cfg)
{
	if (cfg == NULL) {
      ESP_LOGE(TAG, "Config structure pointer is NULL");
      return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Reading WiFi configuration from SPIFFS");

    FILE* file = fopen("/spiffs/wifi.cfg", "r");
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

//** ** ** ** **
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

    init_adc();
    init_DS1803();
    init_led();
    init_brown_detect();
//    init_sound();
//	  sound_beep(100);

    xTaskCreate(&task_blink_led, "BlinkLed",  4096, NULL, 5, NULL);
    xTaskCreate(&task_counter,   "countdown", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    read_wifi_config(&ap_cfg);
  	ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
	  webserver_init_buffers();
	  ESP_LOGI(TAG, "+++ +++ +++ +++  setup_websocket_server ... ... ... ...\n");
	  webserver_start(&ap_cfg);

}