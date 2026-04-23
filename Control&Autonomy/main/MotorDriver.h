#ifndef MOTORDRIVER_H
#define MOTORDRIVER_H

#include "state.h"
#include "controllers.h"

struct MotorVoltages {
    float vL_cmd;
    float vR_cmd;
};

void motor_driver_init();

MotorVoltages denormalize_motor_commands(const MotorCommands& cmds);

void write_motor_voltages(const MotorCommands& cmds);

#endif