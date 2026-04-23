#include <iostream>
#include "controllers.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include "controllers.h"

static constexpr float V_WHEEL_MIN_CTRL = 0.03f;

// wheel speed below this is treated as zero command
static constexpr float V_WHEEL_DEADBAND = 0.03f;

// --------------------------------------------------
// Tunable minimum motor commands
// If output is nonzero and motion is requested:
//   positive command >= FORWARD_MIN_U
//   negative command <= -REVERSE_MIN_U
// --------------------------------------------------
static constexpr float FORWARD_MIN_U = 0.15f;
static constexpr float REVERSE_MIN_U = 0.26f;

// optional clamp so command stays in valid range
static constexpr float U_CMD_MIN = -1.0f;
static constexpr float U_CMD_MAX =  1.0f;

// max allowed change in motor command per control update
// increase this if response feels too sluggish
static constexpr float MAX_U_STEP = 0.16f;

static inline float signf(float x) {
    if (x > 0.0f) return 1.0f;
    if (x < 0.0f) return -1.0f;
    return 0.0f;
}

static inline float clampf(float x, float lo, float hi) {
    return std::clamp(x, lo, hi);
}

// Apply minimum breakout command only when motion is actually requested.
// Direction comes from reference, not noisy PID output sign.
static inline float apply_breakout_command(float u, float v_ref) {
    // no requested motion -> no command
    if (std::fabs(v_ref) < V_WHEEL_MIN_CTRL) {
        return 0.0f;
    }

    // if PID output is tiny or wrong sign, force it into the requested direction
    if (signf(u) != signf(v_ref) || std::fabs(u) < 1e-4f) {
        u = 0.0f;
    }

    if (v_ref > 0.0f) {
        u = std::max(u, FORWARD_MIN_U);
    } else {
        u = std::min(u, -REVERSE_MIN_U);
    }

    return clampf(u, U_CMD_MIN, U_CMD_MAX);
}

static inline float slew_limit(float target, float prev, float max_step) {
    float delta = target - prev;
    delta = clampf(delta, -max_step, max_step);
    return prev + delta;
}

PIDs init_controllers() {
    PID pid_L = PID(0.0, 0.0, 0.8, 0.9, 0.0, -1, 1, -5.0, 5.0, 0.0);
    PID pid_R = PID(0.0, 0.0, 0.8, 0.9, 0.0, -1, 1, -5.0, 5.0, 0.0);

    return PIDs{pid_L, pid_R};
}

// changing for tuning dont forget to change back
// """const RobotState& robot, """
MotorCommands update_controllers(float v_ref, float w_ref, float vL, float vR, PIDs& pid) {
    float vL_ref = v_ref - (TRACK_WIDTH_B / 2.0f) * w_ref;
    float vR_ref = v_ref + (TRACK_WIDTH_B / 2.0f) * w_ref;

    // Optional wheel-reference deadband logic
    // if (std::fabs(vL_ref) < V_WHEEL_MIN_CTRL) {
    //     vL_ref = 0.0f;
    // }
    //
    // if (std::fabs(vR_ref) < V_WHEEL_MIN_CTRL) {
    //     vR_ref = 0.0f;
    // }

    float vL_cmd = pid.L.update(vL_ref, vL);
    float vR_cmd = pid.R.update(vR_ref, vR);

    // Keep breakout minimums, but only when motion is actually requested
    // and in the direction of the requested motion
    vL_cmd = apply_breakout_command(vL_cmd, vL_ref);
    vR_cmd = apply_breakout_command(vR_cmd, vR_ref);

    // Slew limit final output to prevent command jumping / jitter
    static float prev_vL_cmd = 0.0f;
    static float prev_vR_cmd = 0.0f;

    vL_cmd = slew_limit(vL_cmd, prev_vL_cmd, MAX_U_STEP);
    vR_cmd = slew_limit(vR_cmd, prev_vR_cmd, MAX_U_STEP);

    // float max_step_L = MAX_U_STEP;
    // float max_step_R = MAX_U_STEP;

    // // If commanded wheel directions oppose each other, this is a turn-in-place
    // // or strong turning condition. Let commands change faster.
    // if ((vL_ref * vR_ref) < 0.0f) {
    //     max_step_L = 0.25f;
    //     max_step_R = 0.25f;
    // }

    // vL_cmd = slew_limit(vL_cmd, prev_vL_cmd, max_step_L);
    // vR_cmd = slew_limit(vR_cmd, prev_vR_cmd, max_step_R);

    prev_vL_cmd = vL_cmd;
    prev_vR_cmd = vR_cmd;

    return MotorCommands{vL_cmd, vR_cmd};
}