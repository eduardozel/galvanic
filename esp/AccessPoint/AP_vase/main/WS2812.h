#ifndef WS2812_H_
#define WS2812_H_



#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_t;

void initWS2812( void );
void setAllLED_rgb( uint32_t red, uint32_t green, uint32_t blue ); // uint8_t
void setAllLED( rgb_t color );
void offAllLED( void );
void fade_in_warm_white( int max );
rgb_t hsv2rgb(float h, float s, float v);

#endif