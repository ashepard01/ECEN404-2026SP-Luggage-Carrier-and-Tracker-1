#include "manual_test.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
}

#include "shared_state.h"
#include "state_machine.h"

static const char* TAG = "MANUAL_TEST";

extern "C" void manual_test(void* arg)
{
    (void)arg;

    const TickType_t period_ticks = pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS);
    TickType_t last_wake = xTaskGetTickCount();

    state_machine_init();
    ESP_LOGI(TAG, "Control task started (%d ms period)", CONTROL_TASK_PERIOD_MS);

    while (true) {
        state_machine_step();

        State_t state = get_rover_state();
        ManualCommand_t cmd = get_manual_command();

        ESP_LOGI(TAG, "tick state=%s cmd=%s",
                state_to_string(state),
                manual_command_to_string(cmd));

        vTaskDelayUntil(&last_wake, period_ticks);
    }
}