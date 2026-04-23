#ifndef PID_H
#define PID_H

#include <cmath>
#include "state.h"

class PID {
    public:
        float integral;
        float prev_meas;
        float kp;
        float ki;
        float kd;
        float u_min;
        float u_max;
        float i_min;
        float i_max;
        float prev_error;
    
        PID();
        PID(float integral, float prev_meas, float kp, float ki, float kd,
            float u_min, float u_max, float i_min, float i_max, float prev_error);
    
        void reset();
        float update(float sp, float meas);
};
















#endif