#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MANUAL_CMD_STOP = 0,
    MANUAL_CMD_FORWARD,
    MANUAL_CMD_REVERSE,
    MANUAL_CMD_TURNLEFT,
    MANUAL_CMD_TURNRIGHT
} ManualCommand_t;

void shared_state_init(void);

void set_rover_state(State_t state);
State_t get_rover_state(void);


void set_manual_command(ManualCommand_t command);
ManualCommand_t get_manual_command(void);
const char *manual_command_to_string(ManualCommand_t cmd);

#ifdef __cplusplus
}
#endif