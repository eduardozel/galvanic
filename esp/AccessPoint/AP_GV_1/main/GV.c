// // esp32c3 esp-idf v 5.5.1
//
// GV.c
// 
#include <stdint.h>
#include "DS1803.h"
#include "GV.h"

volatile int total_seconds[2];

static const char *TAG = "ADC";
static adc_oneshot_unit_handle_t adc1_handle = NULL;

void ADC_init(void) {
    esp_err_t ret;
    
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC1 unit init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,      // Диапазон ~0–2500 мВ
        .bitwidth = ADC_BITWIDTH_12,   // 12 бит (0–4095)
    };
    
    // === Настройка каналов напряжения (высокое усиление) ===
    ret = adc_oneshot_config_channel(adc1_handle, ADC_v1, &chan_config);
    if (ret != ESP_OK) ESP_LOGE(TAG, "ADC_v1 config failed");
    
    ret = adc_oneshot_config_channel(adc1_handle, ADC_v2, &chan_config);
    if (ret != ESP_OK) ESP_LOGE(TAG, "ADC_v2 config failed");
    
    // === Настройка каналов тока (низкое усиление, 0–750 мВ) ===
    chan_config.atten = ADC_ATTEN_DB_0;  // Диапазон ~0–750 мВ
    
    ret = adc_oneshot_config_channel(adc1_handle, ADC_c1, &chan_config);
    if (ret != ESP_OK) ESP_LOGE(TAG, "ADC_c1 config failed");
    
    ret = adc_oneshot_config_channel(adc1_handle, ADC_c2, &chan_config);
    if (ret != ESP_OK) ESP_LOGE(TAG, "ADC_c2 config failed");
    
    ESP_LOGI(TAG, "ADC initialized successfully");
}

uint32_t get_ADC(uint8_t channel) {
    if (!adc1_handle) {
        ESP_LOGE(TAG, "ADC not initialized");
        return 0;
    }
    
    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(adc1_handle, (adc_channel_t)channel, &raw_value);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        return 0;
    }
    
    return (uint32_t)raw_value;  // 0–4095
}

uint32_t get_ADC_avg( uint8_t channel
                    , uint8_t samples
) {
    if (samples == 0) samples = 1;
    if (samples > 32) samples = 32;
    
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += get_ADC(channel);
        esp_rom_delay_us(100);
    }
    
    return sum / samples;
} // get_ADC_avg

void ADC_deinit(void
) {
    if (adc1_handle) {
        adc_oneshot_del_unit(adc1_handle);
        adc1_handle = NULL;
        ESP_LOGI(TAG, "ADC deinitialized");
    }
} // ADC_deinit

void STOP( uint8_t ch 
){
	total_seconds[ch]= 0;
    if ( 0 == ch ) {
      DS1803_set( 1, 0);
	} else {
      DS1803_set( 0, 0);
	}
} // STOP

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
