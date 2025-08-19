#ifndef WS2812_H_
#define WS2812_H_

#include <stdlib.h>
#include <string.h>


void initWS2812( void );
void setAllLED( uint32_t red, uint32_t green, uint32_t blue );
void offAllLED( void );
void fade_in_warm_white( int max );

#endif