#include "sound.h"

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz

static const char *TAG = "buzzer";

// https://microsin.net/programming/arm/esp32-ledc-pwm.html

#define BUZZER_PIN 7

void init_sound(void)
{
    ESP_LOGI(TAG, "configured buzzer GPIO!");
	gpio_reset_pin(BUZZER_PIN);
	gpio_set_direction( BUZZER_PIN, GPIO_MODE_OUTPUT);
}

void sound_beep(unsigned char dur_hms)
{
	ledc_timer_config_t   ledc_timer;
	ledc_channel_config_t ledc_channel;

	ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;	// resolution of PWM duty
	ledc_timer.freq_hz = 3300;						// frequency of PWM signal
	ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;    // LEDC_HIGH_SPEED_MODE;	// timer mode
	ledc_timer.timer_num = LEDC_TIMER;              //LEDC_HS_TIMER;			// timer index
	ledc_timer_config(&ledc_timer);
	
	ledc_channel.channel    = LEDC_CHANNEL;//LEDC_HS_CH0_CHANNEL;
	ledc_channel.duty       = 4096;
	ledc_channel.gpio_num   = BUZZER_PIN;
	ledc_channel.speed_mode = LEDC_MODE;//LEDC_HS_MODE;
	ledc_channel.hpoint     = 0;
	ledc_channel.timer_sel  = LEDC_TIMER;//LEDC_HS_TIMER;

	ledc_channel_config(&ledc_channel);
	
//	vTaskDelay(pdMS_TO_TICKS(dur_hms*100));
//	ledc_stop(LEDC_MODE,LEDC_CHANNEL,0); // LEDC_HS_MODE LEDC_HS_CH0_CHANNEL

}