#include "shared_state.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Rover state updated by app commands */
static State_t g_rover_state = STATE_IDLE;
ManualCommand_t g_manual_command = MANUAL_CMD_STOP;
static portMUX_TYPE g_state_lock = portMUX_INITIALIZER_UNLOCKED;

void shared_state_init(void)
{
    taskENTER_CRITICAL(&g_state_lock);
    g_rover_state = STATE_IDLE;
    g_manual_command = MANUAL_CMD_STOP;
    taskEXIT_CRITICAL(&g_state_lock);
}

void set_rover_state(State_t state)
{
    taskENTER_CRITICAL(&g_state_lock);
    g_rover_state = state;
    taskEXIT_CRITICAL(&g_state_lock);
}

State_t get_rover_state(void)
{
    State_t state;
    taskENTER_CRITICAL(&g_state_lock);
    state = g_rover_state;
    taskEXIT_CRITICAL(&g_state_lock);
    return state;
}

void set_manual_command(ManualCommand_t command)
{
    taskENTER_CRITICAL(&g_state_lock);
    g_manual_command = command;
    taskEXIT_CRITICAL(&g_state_lock);
}

ManualCommand_t get_manual_command(void)
{
    ManualCommand_t cmd;

    taskENTER_CRITICAL(&g_state_lock);
    cmd = g_manual_command;
    taskEXIT_CRITICAL(&g_state_lock);

    return cmd;
}

const char *manual_command_to_string(ManualCommand_t cmd)
{
    switch (cmd) {
        case MANUAL_CMD_FORWARD:   return "forward";
        case MANUAL_CMD_REVERSE:   return "reverse";
        case MANUAL_CMD_TURNLEFT:  return "turnleft";
        case MANUAL_CMD_TURNRIGHT: return "turnright";
        case MANUAL_CMD_STOP:      return "stop";
        default:                   return "stop";
    }
}