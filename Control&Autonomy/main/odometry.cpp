#include <stdio.h>
#include "odometry.h"
#include "encoder.h"

VelocityHeading compute_body_velocities(float vL, float vR){
    return {(vL + vR) / 2, (vR - vL) / TRACK_WIDTH_B};
};


RobotState encoder_update_state(const RobotState& prev, float vL, float vR) {

    RobotState next = prev;
    float dt = kControlPeriodMs / 1000.0f;
    VelocityHeading vh = compute_body_velocities(vL, vR);

    // next.x  = prev.x + vh.v * std::cos(prev.th) * dt;
    // next.y  = prev.y + vh.v * std::sin(prev.th) * dt;
    // next.th = wrap_angle(prev.th + vh.w * dt);

    next.v  = vh.v;
    next.w  = vh.w;
    next.vL = vL;
    next.vR = vR;

    return next;
}

RobotState uwb_update_state(const RobotState& prev, float x, float y, float th) {

    RobotState next = prev;
    next.x = x;
    next.y = y;
    next.th = th;
    return next;
}


//control:
//get encoder state
//update state
//oa module
//controllers

