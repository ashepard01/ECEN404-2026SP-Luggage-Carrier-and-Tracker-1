#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

// Initialize buzzer GPIO.
void buzzer_init();

// Immediate control.
void buzzer_on();
void buzzer_off();
bool buzzer_is_on();

// Beep-state control.
void buzzer_start_beep();
void buzzer_stop_beep();
bool buzzer_is_beeping();

// Nonblocking update function.
// Call this periodically from your existing task loop.
void buzzer_update(uint32_t now_ms);

#endif // BUZZER_H