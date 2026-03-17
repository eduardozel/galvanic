// // esp32c3 esp-idf v 5.5.1
//
// GV.c
// 
#include <stdint.h>
#include "DS1803.h"
#include "GV.h"

//#include "esp_adc/adc_oneshot.h"

volatile int total_seconds[2];


void ADC_init(
) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_v1, ADC_ATTEN_DB_11); // ADC_ATTEN_DB_11 0 mV ~ 2500 mV;  ADC_ATTEN_DB_6 0 mV ~ 1300 mV -- ADC_ATTEN_DB_12
    adc1_config_channel_atten(ADC_v2, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_c1, ADC_ATTEN_DB_0);  // ADC_ATTEN_DB_0 0 mV ~ 750 mV
    adc1_config_channel_atten(ADC_c2, ADC_ATTEN_DB_0);
}

uint32_t get_ADC( uint8_t channel
) {
  uint32_t tmp = adc1_get_raw(channel);   // int analogVolts = analogReadMilliVolts(2);
  return 1;
}; // get_ADC

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
