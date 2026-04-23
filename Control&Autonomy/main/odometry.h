#ifndef ODOMETRY_H
#define ODOMETRY_H

#include "state.h"
#include "encoder.h"

struct VelocityHeading {
    float v;
    float w;
};

VelocityHeading compute_body_velocities(float vL, float vR);
RobotState encoder_update_state(const RobotState& prev, float vL, float vR);
RobotState uwb_update_state(const RobotState& prev, float x, float y, float th);














#endif