#ifndef DS1803_H_
#define DS1803_H_
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
// https://esp32tutorials.com/

void DS1803_init( void );
void DS1803_set( uint8_t chn, uint8_t val );

#endif