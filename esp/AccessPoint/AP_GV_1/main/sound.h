#ifndef SOUND_H_
#define SOUND_H_
#include <stdio.h>
#include <string.h>
#include "driver/ledc.h"
#include "esp_log.h"

void init_sound( void );
void sound_beep( unsigned char dur_hms );

#endif