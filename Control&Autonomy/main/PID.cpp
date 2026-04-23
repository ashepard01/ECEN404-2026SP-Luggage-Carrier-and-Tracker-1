#include "PID.h"
#include <algorithm>
#include <stdio.h>








PID::PID(float integral, float prev_meas, float kp, float ki, float kd, float u_min, float u_max, float i_min, float i_max, float prev_error)
    : integral(integral), prev_meas(prev_meas), kp(kp), ki(ki), kd(kd), u_min(u_min), u_max(u_max), i_min(i_min), i_max(i_max), prev_error(prev_error) {}

PID::PID()
    : integral(0.0f),
      prev_meas(0.0f),
      kp(0.0f),
      ki(0.0f),
      kd(0.0f),
      u_min(0.0f),
      u_max(0.0f),
      i_min(0.0f),
      i_max(0.0f),
      prev_error(0.0f) {}


float PID::update(float sp, float meas)
{
    float error = sp - meas;

    // Derivative on measurement
    float derivative = -(meas - prev_meas) / DT;

    // Tentative integral
    float integral_candidate = integral + error * DT;
    //printf("integral_candidate: %.6f\n", integral_candidate);
    //printf("p candidate: %.6f\n", error*kp);
    integral_candidate = std::clamp(integral_candidate, i_min, i_max);

    float u_unsat = kp * error
                  + ki * integral_candidate
                  + kd * derivative;

    float u = std::clamp(u_unsat, u_min, u_max);

    bool sat_high = (u_unsat > u_max);
    bool sat_low  = (u_unsat < u_min);

    // Conditional integration:
    // allow integration unless it pushes farther into saturation
    if ((!sat_high && !sat_low) ||
        (sat_high && error < 0.0f) ||
        (sat_low  && error > 0.0f)) {
        integral = integral_candidate;
    }

    prev_meas = meas;
    prev_error = error;

    return u;
}

void PID::reset(){
    integral = 0.0;
    prev_meas = 0.0;
    prev_error = 0.0;
}

