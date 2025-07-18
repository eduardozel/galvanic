#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_LOW_SPEED_MODE // LEDC_HIGH_SPEED_MODE not enable ESP32c3
#define LEDC_HS_CH0_GPIO       (0) // 4 GPIO4 для канала 1
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_HS_CH1_GPIO       (1) // 5 GPIO5 для канала 2
#define LEDC_HS_CH1_CHANNEL    LEDC_CHANNEL_1
#define LEDC_DUTY_RES          LEDC_TIMER_8_BIT // Разрешение ШИМ (0-255)
#define LEDC_FREQUENCY         (1000) // Частота ШИМ 1 кГц

void app_main(void)
{
    // Настройка таймера ШИМ
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .speed_mode = LEDC_HS_MODE,
        .timer_num = LEDC_HS_TIMER
    };
    ledc_timer_config(&ledc_timer);

    // Настройка канала 1
    ledc_channel_config_t ledc_channel_0 = {
        .channel    = LEDC_HS_CH0_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    };
    ledc_channel_config(&ledc_channel_0);

    // Настройка канала 2
    ledc_channel_config_t ledc_channel_1 = {
        .channel    = LEDC_HS_CH1_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_CH1_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    };
    ledc_channel_config(&ledc_channel_1);

    // Пример изменения скважности (duty cycle) для управления сопротивлением
    while (1) {
        // Канал 1: Увеличение скважности от 0 до 255 (0% до 100%, Vgs от 0 до 3.3V)
        for (int duty = 0; duty <= 255; duty += 5) {
            ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL, duty);
            ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL);
            printf("Channel 1 A Duty: %d\n", duty);
            vTaskDelay(100 / portTICK_PERIOD_MS); // Задержка 100 мс
        }
        // Канал 1: Уменьшение скважности от 255 до 0
        for (int duty = 255; duty >= 0; duty -= 5) {
            ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL, duty);
            ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL);
            printf("Channel 1 V Duty: %d\n", duty);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        // Канал 2: Увеличение скважности от 0 до 255
        for (int duty = 0; duty <= 255; duty += 5) {
            ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH1_CHANNEL, duty);
            ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH1_CHANNEL);
            printf("Channel 2 A Duty: %d\n", duty);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        // Канал 2: Уменьшение скважности от 255 до 0
        for (int duty = 255; duty >= 0; duty -= 5) {
            ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH1_CHANNEL, duty);
            ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH1_CHANNEL);
            printf("Channel 2 V Duty: %d\n", duty);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}
