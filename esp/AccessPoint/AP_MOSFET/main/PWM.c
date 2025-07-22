#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "PWM.h"
#include "esp_spiffs.h"

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_LOW_SPEED_MODE // LEDC_HIGH_SPEED_MODE not enable ESP32c3
#define LEDC_HS_CH0_GPIO       (0) // GPIO0 для канала 1
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_HS_CH1_GPIO       (1) // GPIO1 для канала 2
#define LEDC_HS_CH1_CHANNEL    LEDC_CHANNEL_1
#define LEDC_DUTY_RES          LEDC_TIMER_8_BIT // Разрешение ШИМ (0-255)
#define LEDC_FREQUENCY         (1000) // Частота ШИМ 1 кГц

#define VALUES_PATH "/spiffs/values.csv"
#define MAX_VALUES 11
static const char *TAG = "PWM";
/*
static uint8_t  values0[MAX_VALUES] = { 0x00, 0x07, 0x09, 0x10, 0x19, 0x21, 0x2D, 0x3A, 0x48, 0x5C, 0x6A }; // down
static uint8_t  values1[MAX_VALUES] = { 0x00, 0x07, 0x0C, 0x12, 0x18, 0x23, 0x2E, 0x3A, 0x44, 0x5A, 0x65 }; // up

                                     0.1   0.2   0.4   0.6   0.8   1.0   1.2   1.4   1.6   1.8 
*/
//static uint8_t  duty0[MAX_VALUES] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // down
//static uint8_t  duty1[MAX_VALUES] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // up

static uint8_t  duty0[MAX_VALUES] = { 0x00, 0x07, 0x09, 0x10, 0x19, 0x21, 0x2D, 0x3A, 0x48, 0x5C, 0x6A }; // down
static uint8_t  duty1[MAX_VALUES] = { 0x00, 0x07, 0x0C, 0x12, 0x18, 0x23, 0x2E, 0x3A, 0x44, 0x5A, 0x65 }; // up


void read_values() {
    uint8_t  value;

    FILE *f = fopen(VALUES_PATH, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s for reading", VALUES_PATH);
        return;
    }
    char line[128];
    uint8_t ln = 0;
    fgets(line, sizeof(line), f);
	fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f) != NULL && ln < 2) {
        char *token = strtok(line, ",");
        uint8_t vn = 0;
        while (token != NULL && vn < MAX_VALUES) {
			value = (uint8_t)atoi(token);
            if        ( 0 == ln ) { duty0[vn] = value;
			} else if ( 1 == ln ) { duty1[vn] = value;
			}
            token = strtok(NULL, ",");
            vn++;
        } // while token
        ln++;
    } // while line
    fclose(f);
/*
	printf("\nvalues0:\n");
	for (int i = 0; i < MAX_VALUES; i++) {
	  printf("v[%d]: %d<   >", i, values0[i]);
    }
*/
} // read_values

void init_PWM(void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .speed_mode = LEDC_HS_MODE,
        .timer_num = LEDC_HS_TIMER
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel_0 = {
        .channel    = LEDC_HS_CH0_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    };
    ledc_channel_config(&ledc_channel_0);

    ledc_channel_config_t ledc_channel_1 = {
        .channel    = LEDC_HS_CH1_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_CH1_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    };
    ledc_channel_config(&ledc_channel_1);
	
//	read_values();
} // init_PWM

void PWM_set( uint8_t chn, uint8_t idx ) 
{
  if        ( 1 == chn) {
	ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL, duty0[idx]);
	ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL);
	printf("Channel 1 A Duty: %d\n", duty0[idx]);
  } else if ( 0 == chn) {
	ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH1_CHANNEL, duty1[idx]);
	ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL);
	printf("Channe0 0 A Duty: %d\n", duty1[idx]);
  } else { 
  }
} // PWM_set
