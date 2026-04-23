#pragma once
#include "events.h"

#ifdef __cplusplus
extern "C" {
#endif



void state_machine_init(void);
void state_machine_handle_event(Event_t event);
void state_machine_set_state(State_t requested);
void state_machine_step(void);
const char *state_to_string(State_t state);

#ifdef __cplusplus
}
#endif