#ifndef DS1803_H_
#define DS1803_H_
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
// https://esp32tutorials.com/

#define MAX_VALUES 11
/*
static uint8_t  values0[MAX_VALUES] = { 0x00, 0x07, 0x09, 0x10, 0x19, 0x21, 0x2D, 0x3A, 0x48, 0x5C, 0x6A }; // top
static uint8_t  values1[MAX_VALUES] = { 0x00, 0x07, 0x0C, 0x12, 0x18, 0x23, 0x2E, 0x3A, 0x44, 0x5A, 0x65 }; // bottom

                                     0.1   0.2   0.4   0.6   0.8   1.0   1.2   1.4   1.6   1.8 
*/
extern uint8_t values0[MAX_VALUES]; // top
extern uint8_t values1[MAX_VALUES]; // bottom

void DS1803_init( void );
void DS1803_set( uint8_t chn, uint8_t idx );


#endif