#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>


#include "cJSON.h"

#include "lamp.h"


#define CONFIG_FILE     "/spiffs/light.cfg"

static const char *TAG = "LAMP";

/*
static const char *lamp_state_to_str(LAMP_state_t s)
{
    switch (s) {
    case white:   return "white";
    case rainbow: return "rainbow";
    case custom:  return "custom";
    default:      return "unknown";
    }
}
*/
/*
static int lamp_state_from_str(const char *s, LAMP_state_t *out)
{
    if (!s || !out) return -1;
    if (strcasecmp(s, "white") == 0) {
        *out = white;
    } else if (strcasecmp(s, "rainbow") == 0) {
        *out = rainbow;
    } else if (strcasecmp(s, "color") == 0) {
        *out = custom;
    } else { // попробовать прочитать как число:
        char *end; 
        long v = strtol(s, &end, 10);
        if (end != s) {
            if (v == white || v == rainbow || v == custom) {
                *out = (LAMP_state_t)v;
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }
    return 0;
}
*/

typedef enum {
    white   = 0,
    rainbow = 1,
    custom  = 2
} LAMP_state_t;


bool LAMP_on = false;
LAMP_state_t lamp_state = white;

bool         rainbow_active = false;
TaskHandle_t rainbow_task_handle = NULL;

int   current_duration = 5*60;
int   total_seconds = 0;
int   brightness = 4;
rgb_t custom_color;

void rainbow_task(void *arg) {
    float hue = 0.0f;
    float step = 360.0f / 32.0f; // 32 steps for full rainbow
    int direction = 1;           // 1: forward, -1: backward

    while (rainbow_active) {
        // Calculate RGB from HSV (S=1, V=1 max brightness)
        rgb_t color = hsv2rgb(hue, 1.0f, 1.0f);
        setAllLED(color );

        hue += direction * step;
        if (hue >= 360.0f) {
            hue = 360.0f;
            direction = -1;
        } else if (hue <= 0.0f) {
            hue = 0.0f;
            direction = 1;
        }

        vTaskDelay(pdMS_TO_TICKS( 1500));
    }
    vTaskDelete(NULL);
} // rainbow_task

void start_rainbow(void) {
    if (rainbow_active) return; // Already running
    rainbow_active = true;
    xTaskCreate(rainbow_task, "rainbow_task", 2048, NULL, 5, &rainbow_task_handle);
} // start_rainbow

void stop_rainbow(void) {
    if (!rainbow_active) return;
    rainbow_active = false;
    if (rainbow_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait a bit for task to exit loop
        rainbow_task_handle = NULL;
    }
} // stop_rainbow
/*==================*/
//-----------------
void LAMP_turn_On(void){
	total_seconds = current_duration;
	if ( white == lamp_state ) {
	  ESP_LOGI(TAG, "white");
	  fade_in_warm_white( brightness );
	} else if ( custom == lamp_state ) {
	  ESP_LOGI(TAG, "custom\nred=%d",custom_color.red);
      setAllLED( custom_color );
	} else if ( rainbow == lamp_state ) {
	  ESP_LOGI(TAG, "rainbow");
	  start_rainbow();
	} else {
	  ESP_LOGE(TAG, "unknown lamp_state");
	};
	LAMP_on = true;
}; // LAMPon

void LAMP_turn_Off( void ){
  total_seconds = 0;
  stop_rainbow();
  offAllLED();
  LAMP_on = false;
} // LAMP_turn_Off

void write_config_file(void) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "brightness=%d\nduration=%d\n", brightness, current_duration );
    fprintf(f, "red=%d\ngreen=%d\nblue=%d\n", custom_color.red, custom_color.green, custom_color.blue);
    ESP_LOGI(TAG, "Config written:\n brightness=%d\nduration=%d\n", brightness, current_duration);
	fprintf(f, "lamp_state=%d\n",(int)lamp_state);
    fclose(f);
} // write_config_file

void read_config_file(void) {
	ESP_LOGI(TAG, "read_config_file");
    FILE *f = fopen(CONFIG_FILE, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "File not found, using defaults");
        current_duration = 5;
        brightness = 3;
        write_config_file();  // Создаём файл с дефолтами
        return;
    }

    char line[64];

    while (fgets(line, sizeof(line), f)) {
		int tmp;
        if (sscanf(line, "brightness=%d", &brightness) == 1) {                       // if (sscanf(line, "max_time=%" SCNu32, &max_time) == 1) {
            ESP_LOGI(TAG, "Read brightness=%d", brightness);                         //     ESP_LOGI(TAG, "Read max_time=%" PRIu32, max_time);

        } else if (sscanf(line, "red=%hhu", &custom_color.red) == 1) {
            ESP_LOGI(TAG, "Read red=%d", custom_color.red);
        } else if (sscanf(line, "green=%hhu", &custom_color.green) == 1) {
            ESP_LOGI(TAG, "Read green=%d", custom_color.green);
        } else if (sscanf(line, "blue=%hhu", &custom_color.blue) == 1) {
            ESP_LOGI(TAG, "Read blue=%d", custom_color.blue);
        } else if (sscanf(line, "duration=%d", &current_duration) == 1) {
            ESP_LOGI(TAG, "Read current_duration=%d", current_duration);
        } else if (sscanf(line, "lamp_state=%d", &tmp) == 1) {
			lamp_state = (LAMP_state_t)tmp;
            ESP_LOGI(TAG, "Read lamp_state=%d", lamp_state);
        }
    } // while
    fclose(f);
} // read_config_file


void process_color_component(cJSON *json, const char *field_name, uint8_t *color_component, const char *color_name) {
    cJSON *item = cJSON_GetObjectItem(json, field_name);
    if (item != NULL) {
        if (cJSON_IsNumber(item)) {
            *color_component = item->valueint;
            ESP_LOGI(TAG, "%s set to %d", color_name, *color_component);
        } else {
            const char *str_val = item->valuestring;
            ESP_LOGE(TAG, "%s is not a number: %s", color_name, str_val);
            *color_component = 0;
        }
    } else {
        ESP_LOGW(TAG, "%s item not found in JSON, using default value", color_name);
    }
} // process_color_component

void lamp_settings_from_json(cJSON *json) {
    // Обработка длительности
    cJSON *duration_item = cJSON_GetObjectItem(json, "duration");
    if (duration_item != NULL) {
        if (cJSON_IsNumber(duration_item)) {
            current_duration = duration_item->valueint * 60; // minutes to seconds
        } else {
            const char *dstr = duration_item->valuestring;
            ESP_LOGE(TAG, "Duration is not a number: %s", dstr);
        }
    } else {
        ESP_LOGE(TAG, "Duration item not found in JSON");
    }

    // Обработка режима работы лампы
    cJSON *mode_item = cJSON_GetObjectItem(json, "mode");
    if (mode_item != NULL && mode_item->type == cJSON_String) {
        const char *mode = mode_item->valuestring;
        if (strcmp(mode, "white") == 0) {
            lamp_state = white;
        } else if (strcmp(mode, "custom") == 0) {
            lamp_state = custom;
        } else if (strcmp(mode, "rainbow") == 0) {
            lamp_state = rainbow;
        } else {
            ESP_LOGE(TAG, "Unknown mode: %s", mode);
        }
    }

    process_color_component(json, "red",   &custom_color.red,   "red");
    process_color_component(json, "green", &custom_color.green, "green");
    process_color_component(json, "blue",  &custom_color.blue,  "blue");

    cJSON *brightness_item = cJSON_GetObjectItem(json, "brightness");
    if (brightness_item != NULL) {
        if (cJSON_IsNumber(brightness_item)) {
            brightness = brightness_item->valueint;
        } else if (cJSON_IsString(brightness_item)) {
            brightness = atoi(brightness_item->valuestring);
        } else {
            ESP_LOGE(TAG, "Brightness is not a number or string");
        }
    }

    write_config_file();
    LAMP_turn_On();

    ESP_LOGI(TAG, "Settings applied - Brightness: %d, Duration: %d seconds", 
             brightness, current_duration);
}

int lamp_settings_json(char *buffer, size_t size) {
    return snprintf(buffer, size,
                    "{\"timer\": %d, \"red\": %d, \"green\": %d, \"blue\": %d, \"brightness\": %d, \"state\": %d}",
                    total_seconds, custom_color.red, custom_color.green, 
                    custom_color.blue, brightness, lamp_state);
} // lamp_settings_json

void LAMP_init(void){
    total_seconds = 0;
	read_config_file();
	initWS2812();
	fade_in_warm_white( brightness );
    vTaskDelay( 200 );
	offAllLED();
}; // LAMP_init
