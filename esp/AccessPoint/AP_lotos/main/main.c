//
// main.c
// esp-idf v 5.5.1
//
#include <stdio.h>
#include <string.h>

#include "lamp.h"
#include "esp_log.h"

#include "webserver.h"

#include <stdlib.h>
#include <math.h>

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "spi_flash_mmap.h"
#include "esp_spiffs.h"

#include "nvs_flash.h"

/*
#include <esp_http_server.h>
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"

*/

#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/gpio.h"


static gpio_num_t BTN_1_PIN;

#define LED_PIN 8
#define led_on  0
#define led_off 1

static QueueHandle_t button_queue;
typedef enum {
    EVENT_BUTTON_1,   // Событие от TTP223_1
    EVENT_BUTTON_2    // Событие от TTP223_2
} button_event_t;

static const char *TAG = "AP lotos";

static webserver_ap_config_t ap_cfg;

static int freqLED = 10000;
static int led_state = 0;

static const int tick = 10;

//static char response_data[8192+4096];
/***********************/

static void IRAM_ATTR touch_isr_handler(void* arg) {
    BaseType_t higher_priority_task_woken = pdFALSE;
    button_event_t event = EVENT_BUTTON_1;
    xQueueSendFromISR( button_queue, &event, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void touch_task(void* arg) {
    button_event_t event;
//    static uint64_t last_time_on = 0;   // Для debounce TTP223
//    static uint64_t last_time_off = 0;  // Для debounce  OFF

    while (1) {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY)) {
            switch (event) {
                case EVENT_BUTTON_1:
                  ESP_LOGI(TAG, "Touch button1 detected");
                  if (!LAMP_on) {
                    LAMP_turn_On();
                  } else {
				    LAMP_turn_Off();
                  } // if LAMP_on
                  break;
                default:
                    ESP_LOGW(TAG, "Неизвестное событие: %d", event);
                    break;
            }; // switch 
        } // if
    } // while
}

/* * * * * * */
void task_blink_led(void *arg) {
    while (1) {
        gpio_set_level(LED_PIN, led_off);
        vTaskDelay(  freqLED / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, led_on);
        vTaskDelay( 50 / portTICK_PERIOD_MS);
    }
} // task_blink_led

static void init_led(){
    gpio_reset_pin(LED_PIN);
    gpio_set_direction( LED_PIN, GPIO_MODE_OUTPUT);
	led_state = 0;
    xTaskCreate(&task_blink_led, "BlinkLed",  2048, NULL, 5, NULL);
} // init_led

// * * * * *

//  / * / * / *
void lamp_timeout_task(void *arg) {
    while (1) {
//      if ( total_seconds > 0 ) {
		if        ( total_seconds   > tick ) { total_seconds -= tick;
		} else if ( total_seconds  == tick ) { LAMP_turn_Off();
		}
//      } // if 
		vTaskDelay( 10000 / portTICK_PERIOD_MS);
    } // while
} // lamp_timeout_task

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

// --
/*
На ESP32-C3 избегайте:

GPIO_NUM_11 — строб памяти (flash)
GPIO_NUM_12–GPIO_NUM_17 — внутренняя SPI Flash/PSRAM

bool is_valid_gpio(gpio_num_t pin) {
    // Список допустимых GPIO для ESP32-C3 (без USB и стробов)
    const gpio_num_t valid_pins[] = {
        GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4,
        GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9,
        GPIO_NUM_10, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_20, GPIO_NUM_21
    };
    const size_t count = sizeof(valid_pins) / sizeof(valid_pins[0]);

    for (size_t i = 0; i < count; i++) {
        if (valid_pins[i] == pin) {
            return true;
        }
    }
    return false;
}

if (pin_num < 0 || pin_num > 21) {
    ESP_LOGE(TAG, "GPIO %d out of range [0..21]", pin_num);
    return ESP_ERR_INVALID_ARG;
}

gpio_num_t gpio_pin = (gpio_num_t)pin_num;

if (!is_valid_gpio(gpio_pin)) {
    ESP_LOGE(TAG, "GPIO %d is not usable on ESP32-C3", pin_num);
    return ESP_ERR_INVALID_ARG;
}

*/

esp_err_t read_lamp_config_from_file(void) {
    ESP_LOGI(TAG, "Reading WiFi configuration from SPIFFS");

    FILE* file = fopen("/spiffs/lamp.cfg", "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open wifi_config.txt, using default values");
        return ESP_FAIL;
    }


    char wifi_ssid[32]     = {0};
    char wifi_password[64] = {0};
    int wifi_channel       = 6;

    char temp_ssid[32] = {0};
    if (fgets(temp_ssid, sizeof(temp_ssid), file) == NULL) {
        ESP_LOGE(TAG, "Failed to read SSID");
        fclose(file);
        return ESP_FAIL;
    }
    temp_ssid[strcspn(temp_ssid, "\r")] = 0;
	strcpy(wifi_ssid,     temp_ssid);
    ESP_LOGI(TAG, "Read SSID:%s<<!!!", wifi_ssid);

    char temp_password[64] = {0};
    if (fgets(temp_password, sizeof(temp_password), file) == NULL) {
        ESP_LOGE(TAG, "Failed to read password");
        fclose(file);
        return ESP_FAIL;
    }
    temp_password[strcspn(temp_password, "\r")] = 0;
	strcpy(wifi_password, temp_password);
    ESP_LOGI(TAG, "Read Password: %s<<!!!", wifi_password);

    char temp_chan[8] = {0};
    if (!fgets(temp_chan, sizeof(temp_chan), file)) goto fail;
    temp_chan[strcspn(temp_chan, "\r\n")] = '\0';
    wifi_channel = atoi(temp_chan);
    ESP_LOGI(TAG, "Read wifi_channel=%d", wifi_channel);

    ap_cfg = (webserver_ap_config_t){
        .ssid = wifi_ssid,
        .password = wifi_password,
        .channel = (uint8_t)wifi_channel,
        .max_connections = 3
    };

    char led_str[8] = {0};
    if (!fgets(led_str, sizeof(led_str), file)) goto fail;
    led_str[strcspn(led_str, "\r\n")] = '\0';
    int led_num = atoi(led_str);
//    if (led_num < 0 || led_num > 21) goto fail;
    LED_STRIP_GPIO = (gpio_num_t)led_num;
//    if (!is_valid_gpio(LED_STRIP_GPIO)) goto fail;

    char btn_str[8] = {0};
    if (!fgets(btn_str, sizeof(btn_str), file)) goto fail;
    btn_str[strcspn(btn_str, "\r\n")] = '\0';
    int btn_num = atoi(btn_str);
//    if (btn_num < 0 || btn_num > 21) goto fail;
    BTN_1_PIN = (gpio_num_t)btn_num;
//    if (!is_valid_gpio(BTN_1_PIN)) goto fail;
    ESP_LOGI(TAG, "Read LED_STRIP_GPIO=%d, BTN_1_PIN=%d", LED_STRIP_GPIO, BTN_1_PIN);

    fclose(file);
    return ESP_OK;
fail:
    fclose(file);
    ESP_LOGE(TAG, "Config file format error");
    return ESP_FAIL;
} // read_lamp_config_from_file
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
	if ( xTaskCreate(&lamp_timeout_task,   "countdown", 2048, NULL, 5, NULL) != pdPASS) {
      ESP_LOGE(TAG, "Failed to create lamp_timeout_task!");
    }
    if (read_lamp_config_from_file() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read Lamp configuration!");
        return;
    }

    gpio_config_t io_conf = {
//        .mode = GPIO_MODE_OUTPUT,
//        .pin_bit_mask = (1ULL << LED_GPIO)
    };


    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BTN_1_PIN);
    io_conf.intr_type = GPIO_INTR_POSEDGE;  // Rising edge

    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);


    vTaskDelay(pdMS_TO_TICKS(10));
    int button_state = gpio_get_level(BTN_1_PIN);
    if (button_state == 1) {
      ESP_LOGI(TAG, "+++ ++ ++ ++  btn ... ... ... ...\n");
    } else {
      ESP_LOGI(TAG, "--- -- -- --  btn ... ... ... ...\n");
	};

	ESP_LOGI(TAG, "              btn ... handler ... ...\n");

    button_queue = xQueueCreate(10, sizeof(button_event_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_1_PIN, touch_isr_handler, NULL);


    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Ошибка создания очереди!");
        return;
    }
    xTaskCreate(touch_task, "touch_task", 2048, NULL, 10, NULL);


    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");

	ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
	webserver_init_buffers();
	ESP_LOGI(TAG, "+++ +++ +++ +++  setup_websocket_server ... ... ... ...\n");
	webserver_start(&ap_cfg);

    LAMP_init();
} // main