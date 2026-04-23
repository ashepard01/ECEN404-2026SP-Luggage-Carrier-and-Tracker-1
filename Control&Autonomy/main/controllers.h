#ifndef CONTROLLERS_H
#define CONTROLLERS_H
#include "PID.h"
#include "state.h"


struct PIDs {
    PID L;
    PID R;
};

//normalized velocity commands [-1, 1]
struct MotorCommands {
    float vL_cmd;
    float vR_cmd;  
};


PIDs init_controllers();
MotorCommands update_controllers(float v_ref, float w_ref, float vL, float vR, PIDs& pids);
//const RobotState& robot, 


#endif