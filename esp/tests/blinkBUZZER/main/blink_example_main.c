/*

https://www.esp32.com/viewtopic.php?t=16839

*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/ledc.h"

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (5) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz

static const char *TAG = "example";

/* 
   Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/

#define LED_PIN 8
#define BUZZER_PIN 9

void task_blink_led(void *arg) {
    while (1) {
        gpio_set_level(LED_PIN, 1);
		ESP_LOGI(TAG, "Turning On the LED ");
        vTaskDelay( 1000 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, 0);
        vTaskDelay( 5000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Turning Off the LED ");
    }
}

void esp32_beep(unsigned char dur_hms)
{
	ledc_timer_config_t ledc_timer;
	ledc_channel_config_t ledc_channel;

	ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;	// resolution of PWM duty
	ledc_timer.freq_hz = 3300;						// frequency of PWM signal
	ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE; // LEDC_HIGH_SPEED_MODE;	// timer mode
	ledc_timer.timer_num = LEDC_TIMER;//LEDC_HS_TIMER;			// timer index
	ledc_timer_config(&ledc_timer);
	
	ledc_channel.channel    = LEDC_CHANNEL;//LEDC_HS_CH0_CHANNEL;
	ledc_channel.duty       = 4096;
	ledc_channel.gpio_num   = BUZZER_PIN;
	ledc_channel.speed_mode = LEDC_MODE;//LEDC_HS_MODE;
	ledc_channel.hpoint     = 0;
	ledc_channel.timer_sel  = LEDC_TIMER;//LEDC_HS_TIMER;

	ledc_channel_config(&ledc_channel);
/*	
	vTaskDelay(pdMS_TO_TICKS(dur_hms*100));
	ledc_stop(LEDC_MODE,LEDC_CHANNEL,0); // LEDC_HS_MODE LEDC_HS_CH0_CHANNEL
*/
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(LED_PIN);
	gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction( LED_PIN, GPIO_MODE_OUTPUT);
	gpio_set_direction( BUZZER_PIN, GPIO_MODE_OUTPUT);
}
//        vTaskSuspend(NULL);
//        xTaskResume(task_blink_led1);

void app_main(void)
{
    configure_led();
    xTaskCreate(&task_blink_led, "BlinkLed", 4096, NULL, 5, NULL);
    esp32_beep(33);

//        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
}
