#include "state_machine.h"
#include "shared_state.h"
#include "MotorDriver.h"
#include "esp_log.h"
#include <string.h>
#include "PID.h"
#include "buzzer.h"
#include "lidar_preprocess.h"
#include <vector>

static const char* TAG = "STATE_MACHINE";

static void enter_state(State_t new_state);
static void stop_rover();
static constexpr float MANUAL_STOP_DISTANCE_M = 1.0f;
static constexpr int MANUAL_FRONT_CENTER_BIN = 90;   // your OA uses 90 as front
static constexpr int MANUAL_FRONT_HALF_WIDTH = 28;   // checks bins 75..105

static bool manual_front_blocked()
{
    std::vector<float> lidar_ranges = get_clean_lidar_ranges();
    if (lidar_ranges.size() != 360) {
        return false;
    }

    float d_front = 99.0f;

    for (int k = -MANUAL_FRONT_HALF_WIDTH; k <= MANUAL_FRONT_HALF_WIDTH; k++) {
        int idx = MANUAL_FRONT_CENTER_BIN + k;
        if (idx < 0 || idx >= static_cast<int>(lidar_ranges.size())) {
            continue;
        }

        float d = lidar_ranges[idx];
        if (d > 0.0f) {   // ignore invalid/unknown values
            d_front = std::min(d_front, d);
        }
    }

    return d_front <= MANUAL_STOP_DISTANCE_M;
}

const char *state_to_string(State_t state)
{
    switch (state) {
        case STATE_IDLE:             return "STATE_IDLE";
        case STATE_NAVIGATING:       return "STATE_NAVIGATING";
        case STATE_EMERGENCY_STOP:   return "STATE_EMERGENCY_STOP";
        case STATE_ERROR:            return "STATE_ERROR";
        case STATE_ALARM:            return "STATE_ALARM";
        case STATE_MANUAL_DRIVE:     return "STATE_MANUAL_DRIVE";
        default:                     return "STATE_UNKNOWN";
    }
}

void state_machine_init(void)
{
    state_machine_set_state(STATE_IDLE);
    set_manual_command(MANUAL_CMD_STOP);
    stop_rover();
}

void state_machine_handle_event(Event_t event) {
    State_t current = get_rover_state();
    State_t next = current;

    switch (current) {
        case STATE_IDLE:
            switch (event) {
                case EVENT_EMERGENCY_STOP:
                    next = STATE_EMERGENCY_STOP;
                    break;
                case EVENT_ERROR:
                    next = STATE_ERROR;
                    break;
                case EVENT_WEIGHT_REMOVED:
                    next = STATE_ALARM;
                    break;
                default:
                    break;
            }
            break;

        case STATE_MANUAL_DRIVE:
            switch (event) {
                case EVENT_EMERGENCY_STOP:
                    next = STATE_EMERGENCY_STOP;
                    break;
                case EVENT_ERROR:
                    next = STATE_ERROR;
                    break;
                case EVENT_WEIGHT_REMOVED:
                    next = STATE_ALARM;
                    break;
                default:
                    break;
            }
            break;

        case STATE_NAVIGATING:
            switch (event) {
                case EVENT_EMERGENCY_STOP:
                    next = STATE_EMERGENCY_STOP;
                    break;
                case EVENT_ERROR:
                    next = STATE_ERROR;
                    break;
                case EVENT_WEIGHT_REMOVED:
                    next = STATE_ALARM;
                    break;
                default:
                    break;
            }
            break;

        case STATE_ALARM:
            switch (event) {
                case EVENT_EMERGENCY_STOP:
                    next = STATE_EMERGENCY_STOP;
                    break;
                case EVENT_ERROR:
                    next = STATE_ERROR;
                    break;
                case EVENT_WEIGHT_RESTORED:
                    next = STATE_IDLE;
                    break;
                default:
                    break;
            }
            break;

        case STATE_EMERGENCY_STOP:
            switch (event) {
                case EVENT_EMERGENCY_CLEARED:
                    next = STATE_IDLE;
                    break;
                case EVENT_ERROR:
                    next = STATE_ERROR;
                    break;
                default:
                    break;
            }
            break;

        case STATE_ERROR:
            switch (event) {
                case EVENT_EMERGENCY_STOP:
                    next = STATE_EMERGENCY_STOP;
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }

    if (next != current) {
        state_machine_set_state(next);
        ESP_LOGI(TAG, "Transition %d -> %d", current, next);
    }
}

void state_machine_set_state(State_t requested)
{
    State_t current = get_rover_state();
    if (requested == current) return;

    set_rover_state(requested);
    enter_state(requested);
}

void state_machine_step(void)
{
    State_t state = get_rover_state();

    switch (state) {
        case STATE_IDLE:
        case STATE_ALARM:
        case STATE_EMERGENCY_STOP:
        case STATE_ERROR:
            stop_rover();
            break;

        case STATE_MANUAL_DRIVE: {
            MotorCommands motor = {0.0f, 0.0f};
            ManualCommand_t cmd = get_manual_command();
        
            switch (cmd) {
                case MANUAL_CMD_FORWARD:
                    motor.vL_cmd = 0.2f;
                    motor.vR_cmd = 0.2f;
                    break;
        
                case MANUAL_CMD_REVERSE:
                    motor.vL_cmd = -1.0f;
                    motor.vR_cmd = -1.0f;
                    break;
        
                case MANUAL_CMD_TURNLEFT:
                    motor.vL_cmd = -1.0f;
                    motor.vR_cmd = 0.4f;
                    break;
        
                case MANUAL_CMD_TURNRIGHT:
                    motor.vL_cmd = 0.4f;
                    motor.vR_cmd = -1.0f;
                    break;
        
                case MANUAL_CMD_STOP:
                default:
                    motor.vL_cmd = 0.0f;
                    motor.vR_cmd = 0.0f;
                    break;
            }

            if (cmd == MANUAL_CMD_FORWARD && manual_front_blocked()) {
                ESP_LOGW(TAG, "Manual forward blocked by obstacle");
                motor.vL_cmd = 0.0f;
                motor.vR_cmd = 0.0f;
            }
        
            write_motor_voltages(motor);
            break;
        }

        case STATE_NAVIGATING:
            // leave blank for now or stop_rover() if nav is not active yet

            //stop_rover();
            break;
    }
}

static void enter_state(State_t new_state) {
    switch (new_state) {
        case STATE_IDLE:
            set_manual_command(MANUAL_CMD_STOP);
            buzzer_stop_beep();
            stop_rover();
            break;

        case STATE_ALARM:
            set_manual_command(MANUAL_CMD_STOP);
            buzzer_start_beep();
            stop_rover();
            break;

        case STATE_EMERGENCY_STOP:
            set_manual_command(MANUAL_CMD_STOP);
            buzzer_stop_beep();
            stop_rover();
            break;

        case STATE_ERROR:
            set_manual_command(MANUAL_CMD_STOP);
            buzzer_stop_beep();
            stop_rover();
            break;

        default:
            break;
    }
}

static void stop_rover()
{
    // replace with your actual neutral command function
    MotorCommands cmd = {0.0f, 0.0f};
    write_motor_voltages(cmd);
}