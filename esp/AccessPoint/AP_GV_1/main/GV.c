#include <stdint.h>
#include "DS1803.h"

volatile int total_seconds[2];

void STOP( uint8_t ch ){
	total_seconds[ch]= 0;
    if ( 0 == ch ) {
      DS1803_set( 1, 0);
	} else {
      DS1803_set( 0, 0);
	}
}

/*
static void init_sound(void)
{
  ESP_LOGI(TAG, "configured buzzer GPIO!");
	gpio_reset_pin(BUZZER_PIN);
	gpio_set_direction( BUZZER_PIN, GPIO_MODE_OUTPUT);
} // init_sound

void sound_beep(unsigned char dur_hms)
{
	ledc_timer_config_t   ledc_timer;
	ledc_channel_config_t ledc_channel;
	ESP_LOGI(TAG, "configure ledc >>");
	ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;	// resolution of PWM duty
	ledc_timer.freq_hz = 3300;						          // frequency of PWM signal
	ESP_LOGI(TAG, "configure ledc >>>");
	ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;    // LEDC_HIGH_SPEED_MODE;	// timer mode
	ledc_timer.timer_num = LEDC_TIMER;              // LEDC_HS_TIMER;		    	// timer index
	ESP_LOGI(TAG, "configure ledc >>>!!");
	ledc_timer_config(&ledc_timer);

    ESP_LOGI(TAG, "configured ledc_timer");

	ledc_channel.channel    = LEDC_CHANNEL;//LEDC_HS_CH0_CHANNEL;
	ledc_channel.duty       = 4096;
	ledc_channel.gpio_num   = BUZZER_PIN;
	ledc_channel.speed_mode = LEDC_MODE;//LEDC_HS_MODE;
	ledc_channel.hpoint     = 0;
	ledc_channel.timer_sel  = LEDC_TIMER;//LEDC_HS_TIMER;

	ledc_channel_config(&ledc_channel);
    ESP_LOGI(TAG, "configured ledc_channel");	
//	vTaskDelay(pdMS_TO_TICKS(dur_hms*100));
    vTaskDelay( 1000 / portTICK_PERIOD_MS);
	ledc_stop(LEDC_MODE,LEDC_CHANNEL,0); // LEDC_HS_MODE LEDC_HS_CH0_CHANNEL
} // sound_beep
*/
