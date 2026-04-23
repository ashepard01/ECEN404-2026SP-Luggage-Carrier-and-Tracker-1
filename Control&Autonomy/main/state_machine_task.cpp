#include "state_machine_task.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
}

#include "shared_state.h"
#include "state_machine.h"
#include "hx711.h"
#include "buzzer.h"
#include "events.h"

static const char* TAG = "STATE_MACHINE_TASK";

// Use the same period your manual test used.
// If this is defined elsewhere, this will compile cleanly.
#ifndef CONTROL_TASK_PERIOD_MS
#define CONTROL_TASK_PERIOD_MS 20
#endif
constexpr TickType_t WEIGHT_CHECK_PERIOD_TICKS = pdMS_TO_TICKS(500); // 2 Hz

static TickType_t s_last_weight_check = 0;
static bool s_weight_initialized = false;

static bool s_weight_armed = false;
static bool s_object_present = false;

static constexpr int32_t PRESENT_THRESHOLD_RAW = 12000;
static constexpr int32_t REMOVED_THRESHOLD_RAW = 6000;

extern "C" void state_machine_task(void* arg)
{
    (void)arg;

    const TickType_t period_ticks = pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS);
    TickType_t last_wake = xTaskGetTickCount();

    hx711_init();
    buzzer_init();

    vTaskDelay(pdMS_TO_TICKS(500));
    hx711_tare(16);

    s_last_weight_check = xTaskGetTickCount();
    s_weight_initialized = true;
    s_weight_armed = false;
    s_object_present = false;

    ESP_LOGI(TAG, "State machine task started. Waiting for object placement.");
    while (true) {
        TickType_t now_ticks = xTaskGetTickCount();
        uint32_t now_ms = static_cast<uint32_t>(now_ticks * portTICK_PERIOD_MS);

        if (s_weight_initialized &&
            (now_ticks - s_last_weight_check) >= WEIGHT_CHECK_PERIOD_TICKS)
        {
            const int32_t raw   = hx711_read_raw();
            const int32_t tared = hx711_read_tared_average(1);

            if (!s_weight_armed && tared >= PRESENT_THRESHOLD_RAW)
            {
                s_weight_armed = true;
                s_object_present = true;

                ESP_LOGI(TAG, "OBJECT DETECTED / SYSTEM ARMED  raw=%ld tared=%ld",
                        (long)raw, (long)tared);
            }
            else if (s_weight_armed && s_object_present && tared <= REMOVED_THRESHOLD_RAW)
            {
                s_object_present = false;
                state_machine_handle_event(EVENT_WEIGHT_REMOVED);

                ESP_LOGW(TAG, "OBJECT REMOVED / ALARM EVENT  raw=%ld tared=%ld",
                        (long)raw, (long)tared);
            }
            else if (s_weight_armed && !s_object_present && tared >= PRESENT_THRESHOLD_RAW)
            {
                s_object_present = true;
                state_machine_handle_event(EVENT_WEIGHT_RESTORED);

                ESP_LOGI(TAG, "OBJECT RESTORED / CLEAR ALARM raw=%ld tared=%ld",
                        (long)raw, (long)tared);
            }

            ESP_LOGI(TAG,
                    "raw=%ld tared=%ld armed=%d object_present=%d state=%d",
                    (long)raw,
                    (long)tared,
                    (int)s_weight_armed,
                    (int)s_object_present,
                    (int)get_rover_state());

            s_last_weight_check = now_ticks;
        }

        buzzer_update(now_ms);
        state_machine_step();

        vTaskDelayUntil(&last_wake, period_ticks);
    }
}