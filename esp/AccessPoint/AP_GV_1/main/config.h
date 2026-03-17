#ifndef CFG_H_
#define CFG_H_

#include "webserver.h"

void init_spiffs();
void read_values();
esp_err_t read_wifi_config( webserver_ap_config_t *cfg);

#endif