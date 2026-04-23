#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include <cmath>
#include "state.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------
// Internal Encoder State
// ----------------------------
struct EncoderState {
    pcnt_unit_handle_t unit = nullptr;
    pcnt_channel_handle_t chan_a = nullptr;
    pcnt_channel_handle_t chan_b = nullptr;

    int count = 0;            // absolute accumulated count
    int last_count = 0;       // previous sampled count
    int delta_count = 0;      // counts over last update interval

    float velocity_mps = 0.0f;
};

static EncoderState enc_left;
static EncoderState enc_right;

// Initialize both quadrature encoder PCNT units.
void encoder_init(void);

// Poll the encoder counts and numerically differentiate into velocity.
// dt is the elapsed sample period in seconds.
void encoder_update(float dt);

// Wheel linear velocities in meters/second.
float encoder_get_left_velocity(void);
float encoder_get_right_velocity(void);

// Absolute accumulated counts since last zero.
int32_t encoder_get_left_count(void);
int32_t encoder_get_right_count(void);

// Reset hardware/software accumulated counts and velocities to zero.
void encoder_zero(void);

#ifdef __cplusplus
}
#endif

#endif // ENCODER_H