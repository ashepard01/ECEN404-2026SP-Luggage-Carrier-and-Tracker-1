#pragma once

#include <string.h>     /* strlen, snprintf */
#include <stdio.h>      /* printf, sscanf */
#include <stdbool.h>    /* bool */

#include "freertos/FreeRTOS.h" /* FreeRTOS base */
#include "freertos/task.h"     /* Task API */

#include "esp_wifi.h"    /* Wi-Fi driver */
#include "esp_event.h"   /* Event loop */
#include "esp_log.h"     /* Logging */
#include "nvs_flash.h"   /* NVS storage */
#include "esp_netif.h"   /* Network interface */

#include "esp_http_server.h" /* HTTP server */

#include "shared_state.h"
#include "events.h"


#ifdef __cplusplus
extern "C" {
#endif

void wifi_init_softap(void);
void start_http_server(void);

#ifdef __cplusplus
}
#endif