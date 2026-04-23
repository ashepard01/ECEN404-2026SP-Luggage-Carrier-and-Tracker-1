#include <cstdio>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
}

#include "uwb_rx_task.h"

static const char* TAG = "UWB_RX";

//using 16,17,18,19,25,26

// -------------------- config --------------------
#define UWB_UART_NUM       UART_NUM_1
#define UWB_UART_TX_PIN    12   // change if needed
#define UWB_UART_RX_PIN    13   // change if needed
#define UWB_UART_BAUD      115200
#define UWB_BUF_SIZE       256

// -------------------- shared data --------------------


static uwb_data_t g_uwb_storage = {};
static uwb_data_t* g_uwb_data = &g_uwb_storage;
static SemaphoreHandle_t g_uwb_mutex = NULL;

void uwb_mutex_init()
{
    if (g_uwb_mutex == NULL) {
        g_uwb_mutex = xSemaphoreCreateMutex();
        configASSERT(g_uwb_mutex != NULL);
    }
}

void shared_write_uwb_data(const uwb_data_t* data)
{
    if (!data || g_uwb_mutex == NULL) return;
    xSemaphoreTake(g_uwb_mutex, portMAX_DELAY);
    *g_uwb_data = *data;
    xSemaphoreGive(g_uwb_mutex);
}

bool shared_copy_uwb_data(uwb_data_t* data)
{
    if (!data || g_uwb_mutex == NULL) return false;
    xSemaphoreTake(g_uwb_mutex, portMAX_DELAY);
    *data = *g_uwb_data;
    xSemaphoreGive(g_uwb_mutex);
    return true;
}


// -------------------- parser --------------------
static bool parse_uwb_line(const char* line, uwb_data_t* out)
{
    if (!line || !out) return false;

    char tag[8] = {0};
    float x, y, heading, distance, heading_error;

    int n = sscanf(line, "%7[^,],%f,%f,%f,%f,%f",
                   tag, &x, &y, &heading, &distance, &heading_error);

    if (n != 6) return false;
    if (strcmp(tag, "UWB") != 0) return false;

    out->x = x;
    out->y = y;
    out->heading = heading;
    out->distance_to_user = distance;
    out->heading_error = heading_error;
    out->valid = true;
    out->last_update_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    return true;
}

// -------------------- uart init --------------------
static void uwb_uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = UWB_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
#if ESP_IDF_VERSION_MAJOR >= 5
        .source_clk = UART_SCLK_DEFAULT,
#endif
    };

    uart_driver_install(UWB_UART_NUM, UWB_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UWB_UART_NUM, &uart_config);
    uart_set_pin(UWB_UART_NUM, UWB_UART_TX_PIN, UWB_UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// -------------------- task --------------------
extern "C" void uwb_rx_task(void* arg)
{
    uwb_uart_init();

    uint8_t byte;
    char line[UWB_BUF_SIZE];
    int idx = 0;

    while (true) {
        int len = uart_read_bytes(UWB_UART_NUM, &byte, 1, pdMS_TO_TICKS(20));

        if (len > 0) {
            if (byte == '\n' || byte == '\r') {
                if (idx > 0) {
                    line[idx] = '\0';

                    uwb_data_t uwb_data;
                    if (parse_uwb_line(line, &uwb_data)) {
                        shared_write_uwb_data(&uwb_data);

                        ESP_LOGI(TAG,
                                 "x=%.3f y=%.3f hdg=%.3f dist=%.3f he=%.3f",
                                 uwb_data.x,
                                 uwb_data.y,
                                 uwb_data.heading,
                                 uwb_data.distance_to_user,
                                 uwb_data.heading_error);
                    } else {
                        ESP_LOGW(TAG, "Bad line: %s", line);
                    }

                    idx = 0;
                }
            } else {
                if (idx < UWB_BUF_SIZE - 1) {
                    line[idx++] = (char)byte;
                } else {
                    idx = 0; // overflow, drop line
                }
            }
        }

        uwb_data_t uwb_data;
        if (shared_copy_uwb_data(&uwb_data)) {
            uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
            if (uwb_data.valid && (now_ms - uwb_data.last_update_ms) > 500) {
                uwb_data.valid = false;
                shared_write_uwb_data(&uwb_data);
            }
        }
    }
}