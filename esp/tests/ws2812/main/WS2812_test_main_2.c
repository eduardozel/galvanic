#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "led_strip.h"

// Конфигурация
#define LED_GPIO         8      // GPIO для управления лентой
#define LED_CHANNEL      RMT_CHANNEL_0
#define LED_COUNT        16     // Количество светодиодов
#define LED_BRIGHTNESS  32      // Яркость (0-255)

static led_strip_handle_t led_strip;

void initialize_led_strip() {
    // Конфигурация RMT
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    // Конфигурация драйвера
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    // Инициализация ленты
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // Очистка ленты
    led_strip_clear(led_strip);
}

void set_led_color(uint32_t index, uint32_t red, uint32_t green, uint32_t blue) {
    led_strip_set_pixel(led_strip, index, red * LED_BRIGHTNESS / 255, green * LED_BRIGHTNESS / 255, blue * LED_BRIGHTNESS / 255);
    led_strip_refresh(led_strip);
}

void rainbow_demo() {
    uint32_t colors[] = {
        0xFF0000, // Красный
        0x00FF00, // Зеленый
        0x0000FF, // Синий
        0xFFFF00, // Желтый
        0xFF00FF, // Пурпурный
        0x00FFFF  // Голубой
    };
    
    while (1) {
        for (int color = 0; color < sizeof(colors)/sizeof(colors[0]); color++) {
            for (int i = 0; i < LED_COUNT; i++) {
                uint32_t r = (colors[color] >> 16) & 0xFF;
                uint32_t g = (colors[color] >> 8) & 0xFF;
                uint32_t b = colors[color] & 0xFF;
                set_led_color(i, r, g, b);
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            vTaskDelay(500 / portTICK_PERIOD_MS);
            led_strip_clear(led_strip);
        }
    }
}

void app_main() {
    initialize_led_strip();
    rainbow_demo();
}