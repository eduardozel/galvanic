#ifndef WS2812_H_
#define WS2812_H_

#include <stdlib.h>
#include <string.h>

#define LED_COUNT_MAX 40

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_t;

typedef struct {
    rgb_t leds[LED_COUNT_MAX];
    int duration_sec;
} led_state_t;

extern led_state_t *led_states;


void initWS2812( void );
void setLEDsArray(rgb_t *led_array, size_t count);
void setAllLED_rgb( uint32_t red, uint32_t green, uint32_t blue ); // uint8_t
void setAllLED( rgb_t color );
void offAllLED( void );
void fade_in_warm_white( int max );
rgb_t hsv2rgb(float h, float s, float v);

#endif