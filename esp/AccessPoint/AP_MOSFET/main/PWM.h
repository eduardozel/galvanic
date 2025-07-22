#ifndef PWM_H_
#define PWM_H_
#include <stdio.h>
#include <string.h>
#include "esp_log.h"


void init_PWM( void );
void PWM_set( uint8_t chn, uint8_t idx );

#endif