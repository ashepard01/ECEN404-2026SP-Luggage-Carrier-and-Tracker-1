#pragma once
#include <cmath>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct RobotState {
    float x;
    float y;
    float th;
    float v;
    float w;
    float vL;
    float vR;
};


struct Waypoint {
    float distance;
    float heading_error;
    bool active;
};


struct RateLimitOutput {
    float v;
    float a;
};


struct ApplyRateLimitOutput{
    float vL;
    float vR;
    float aL;
    float aR;
};


static constexpr TickType_t kControlPeriodMs = 10;



constexpr float TRACK_WIDTH_B = 0.635f;   // distance between left/right wheel centers (plot units)
constexpr float BODY_LENGTH_L = 30.0f;   // chassis length (plot units)
constexpr float WHEEL_LEN = 10.0f;       // wheel "length" along body x (plot only)
constexpr float WHEEL_THK = 4.0f;        // wheel thickness (plot only)

constexpr float LINEAR_VELOCITY_MAX = 10.0f;
constexpr float LINEAR_ACCELERATION_MAX = 4.0f;
constexpr float LINEAR_DECCELERATION_MAX = 8.0f;
constexpr float ANGULAR_VELOCITY_MAX = 3.0f;
constexpr float ANGULAR_ACCELERATION_MAX = 4.0f;


constexpr float TURN_IN_PLACE_THRESH = 2.1f;
constexpr float STEER_GAIN = 1.0f;
constexpr float VELOCITY_GAIN = 1.0f;

constexpr float BRAKE_FACTOR = 3.0f;     // multiply stopping distance
constexpr float BRAKE_BUFFER = 0.0f;     // + meters buffer

constexpr float DT = 0.02f;

//RANGING
constexpr float MAX_RANGE = 80.0;
constexpr float DIAGONAL_DIST = std::sqrt(std::pow(TRACK_WIDTH_B / 2, 2) + std::pow(BODY_LENGTH_L / 2, 2));
constexpr float DISTANCE_TOL = DIAGONAL_DIST + 8.0f; 
constexpr float R_STOP = DISTANCE_TOL + 6.0;
constexpr float R_SLOW = DISTANCE_TOL + 10.0;
constexpr float GOAL_SLOW_RADIUS = DISTANCE_TOL * 1.5f;

constexpr float MIN_GAP_WIDTH = DIAGONAL_DIST * 1.25;
constexpr float LIDAR_NOISE = 0.0;

constexpr float W_GOAL = 10.0;
constexpr float W_TURN = 1.0;
constexpr float W_CLEAR = 2.0;



inline float wrap_angle(float angle){

    double pi = 3.1415926535;

    while (angle > pi) {
        angle -= 2 * pi;
    }

    while (angle < -pi){
        angle += 2 * pi;
    }
    return angle;
}


inline float clamp(float x, float lo, float hi) {
    return std::max(lo, std::min(hi, x));
}

inline int angle_to_index(float theta) {
    float theta_deg = theta * 180.0f / static_cast<float>(M_PI);
    int idx = static_cast<int>(std::round(theta_deg)) % 360;
    if (idx < 0) idx += 360;
    return idx;
}



inline RateLimitOutput rate_limit(float current, float cmd, float up_rate, float down_rate, float lo, float hi){
    float dv = cmd - current;
    float dv_applied = std::clamp(dv, -down_rate * DT, up_rate * DT);
    float nxt = std::clamp(current + dv_applied, lo, hi);
    float applied = (DT > 0) ? dv_applied / DT : 0.0;
    return RateLimitOutput{nxt, applied};
}


inline ApplyRateLimitOutput apply_wheel_rate_limits(const RobotState& robot, float vL_cmd, float vR_cmd){

    float vR_cmd_clamped = clamp(vR_cmd, -LINEAR_VELOCITY_MAX, LINEAR_VELOCITY_MAX);
    float vL_cmd_clamped = clamp(vL_cmd, -LINEAR_VELOCITY_MAX, LINEAR_VELOCITY_MAX);

    float vR = rate_limit(robot.vR, vR_cmd_clamped, LINEAR_ACCELERATION_MAX, LINEAR_DECCELERATION_MAX, -LINEAR_VELOCITY_MAX, LINEAR_VELOCITY_MAX).v;
    float aR = rate_limit(robot.vR, vR_cmd_clamped, LINEAR_ACCELERATION_MAX, LINEAR_DECCELERATION_MAX, -LINEAR_VELOCITY_MAX, LINEAR_VELOCITY_MAX).a;

    float vL = rate_limit(robot.vL, vL_cmd_clamped, LINEAR_ACCELERATION_MAX, LINEAR_DECCELERATION_MAX, -LINEAR_VELOCITY_MAX, LINEAR_VELOCITY_MAX).v;
    float aL = rate_limit(robot.vL, vL_cmd_clamped, LINEAR_ACCELERATION_MAX, LINEAR_DECCELERATION_MAX, -LINEAR_VELOCITY_MAX, LINEAR_VELOCITY_MAX).a;

    return ApplyRateLimitOutput{vL, vR, aL, aR};

}
