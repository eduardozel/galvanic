#ifndef GV_H_
#define GV_H_

#include "driver/adc.h"

#define ADC_v1 ADC1_CHANNEL_0 // GPIO 0
#define ADC_v2 ADC1_CHANNEL_1 // GPIO 1
#define ADC_c1 ADC1_CHANNEL_2 // GPIO 2
#define ADC_c2 ADC1_CHANNEL_3 // GPIO 3

extern volatile int total_seconds[2];

#define MAX_VALUES_CBX 5 // 11
//#define DISPLAY_STR_LEN 8

static uint8_t     Hvalues[MAX_VALUES_CBX]  = {    5,     6,     7,     8,     9};  // {0};
static const char *Hdisplay[MAX_VALUES_CBX] = { "1.0", "1.2", "1.4", "1.6", "1.8"}; // {""};
//static char        Hdisplay_buf[MAX_VALUES][DISPLAY_STR_LEN] = {{0}};

void ADC_init();
uint32_t get_ADC( uint8_t channel);
void STOP( uint8_t ch );

#endif