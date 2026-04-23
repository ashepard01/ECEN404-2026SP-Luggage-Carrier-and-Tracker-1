// alarm_test_task.cpp
extern "C" {
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_log.h"
    }
    
    #include "hx711.h"
    #include "buzzer.h"
    
    static const char* TAG = "ALARM_TEST";
    
    // Change only if you want different behavior while testing.
    static constexpr TickType_t LOOP_PERIOD_TICKS  = pdMS_TO_TICKS(50);
    static constexpr TickType_t CHECK_PERIOD_TICKS = pdMS_TO_TICKS(500);
    static constexpr int32_t PRESENT_THRESHOLD_RAW = 12000;
    static constexpr int32_t REMOVED_THRESHOLD_RAW = 6000;
    
extern "C" void alarm_test_task(void* arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();
    TickType_t last_check = 0;

    bool object_present = false;
    bool armed = false;
    bool initialized = false;

    hx711_init();
    buzzer_init();

    vTaskDelay(pdMS_TO_TICKS(500));
    hx711_tare(16);

    object_present = false;
    armed = false;
    initialized = true;
    
    ESP_LOGI(TAG, "Alarm test task started. Waiting for object placement.");
    buzzer_stop_beep();

    while (true)
    {
        TickType_t now_ticks = xTaskGetTickCount();
        uint32_t now_ms = (uint32_t)(now_ticks * portTICK_PERIOD_MS);

        if (initialized && (now_ticks - last_check) >= CHECK_PERIOD_TICKS) {
            const int32_t raw   = hx711_read_raw();
            const int32_t tared = hx711_read_tared_average(1);

            // First time a real object is detected, arm the alarm system.
            if (!armed && tared >= PRESENT_THRESHOLD_RAW) {
                armed = true;
                object_present = true;
                buzzer_stop_beep();

                ESP_LOGI(TAG, "OBJECT DETECTED / SYSTEM ARMED  raw=%ld tared=%ld",
                        (long)raw, (long)tared);
            }
            // After system is armed, detect removal.
            else if (armed && object_present && tared <= REMOVED_THRESHOLD_RAW) {
                object_present = false;
                buzzer_start_beep();

                ESP_LOGW(TAG, "OBJECT REMOVED / ALARM ON  raw=%ld tared=%ld",
                        (long)raw, (long)tared);
            }
            // After alarm is on, detect object restored.
            else if (armed && !object_present && tared >= PRESENT_THRESHOLD_RAW) {
                object_present = true;
                buzzer_stop_beep();

                ESP_LOGI(TAG, "OBJECT RESTORED / ALARM OFF raw=%ld tared=%ld",
                        (long)raw, (long)tared);
            }

            ESP_LOGI(TAG,
                    "raw=%ld tared=%ld armed=%d object_present=%d beeping=%d",
                    (long)raw,
                    (long)tared,
                    (int)armed,
                    (int)object_present,
                    (int)buzzer_is_beeping());

            last_check = now_ticks;
        }

        buzzer_update(now_ms);
        vTaskDelayUntil(&last_wake, LOOP_PERIOD_TICKS);
    }
}