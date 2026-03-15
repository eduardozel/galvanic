#ifndef GV_H_
#define GV_H_

#define VALUES_PATH     "/spiffs/values.cfg"

extern volatile int total_seconds[2];

#define MAX_VALUES 5 // 11
//#define DISPLAY_STR_LEN 8

static uint8_t     Hvalues[MAX_VALUES]  = {    5,     6,     7,     8,     9};  // {0};
static const char *Hdisplay[MAX_VALUES] = { "1.0", "1.2", "1.4", "1.6", "1.8"}; // {""};
//static char        Hdisplay_buf[MAX_VALUES][DISPLAY_STR_LEN] = {{0}};


void STOP( uint8_t ch );

#endif