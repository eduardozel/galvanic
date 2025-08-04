#include "WS2812.h"

#include <stdlib.h>
#include "esp_log.h"
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      8

#define EXAMPLE_LED_NUMBERS         16
#define EXAMPLE_CHASE_SPEED_MS      10

static const char *TAG = "ws2812";

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];


    rmt_channel_handle_t       led_chan;
    rmt_tx_channel_config_t    tx_chan_config;
	led_strip_encoder_config_t encoder_config;
	rmt_encoder_handle_t       led_encoder;
    rmt_transmit_config_t      tx_config;
// - - - - - - - - - - -
/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 * https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/led_strip_simple_encoder
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}


void initWS2812()
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    led_chan = NULL;
    tx_chan_config = (rmt_tx_channel_config_t) {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    led_encoder = NULL;
    encoder_config = (led_strip_encoder_config_t) {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));
	
	tx_config = (rmt_transmit_config_t) {
        .loop_count = 0, // no transfer loop
    };
}

void setAllLED( uint32_t red, uint32_t green, uint32_t blue, uint16_t hue )
{
	for (int j = 0; j < EXAMPLE_LED_NUMBERS; j ++) {
	            led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
                led_strip_pixels[j * 3 + 0] = 210;// green
                led_strip_pixels[j * 3 + 1] = 255;// red;
                led_strip_pixels[j * 3 + 2] = 150;// blue
	}
	ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
} // setAllLED

//set_warm_white(); 
// Теплый белый: GRB = 210, 255, 150
/*
Дополнительно: Вы можете настроить яркость, уменьшая значения, но сохраняя пропорции.
 Пример с яркостью 50%:
   G = 105, R = 127, B = 75
 Но будьте осторожны: из-за нелинейности восприятия глаза, простое уменьшение в 2 раза может дать не совсем теплый белый.
 
 Теплый белый: Используйте значения:

    0xFF9632 (255, 150, 50) - стандартный теплый

    0xFF8B1A (255, 139, 26) - более теплый оттенок
	
	
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
 */