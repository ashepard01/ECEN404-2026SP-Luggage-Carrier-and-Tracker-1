#include <stdio.h>
#include "driver/dac_oneshot.h"
#include <algorithm>
#include "MotorDriver.h"




static dac_oneshot_handle_t dac_left;
static dac_oneshot_handle_t dac_right;


void motor_driver_init(){
    dac_oneshot_config_t cfg_left = {
        .chan_id = DAC_CHAN_0   // GPIO25
    };
    dac_oneshot_new_channel(&cfg_left, &dac_left);
    
    dac_oneshot_config_t cfg_right = {
        .chan_id = DAC_CHAN_1   // GPIO26
    };
    dac_oneshot_new_channel(&cfg_right, &dac_right);

}

MotorVoltages denormalize_motor_commands(const MotorCommands& cmds){

    return {std::clamp(-2.5f * cmds.vL_cmd + 2.5f, 0.0f, 3.3f), std::clamp(-2.5f * cmds.vR_cmd + 2.5f, 0.0f, 3.3f)};

}

void write_motor_voltages(const MotorCommands& cmds){

    MotorVoltages voltages = denormalize_motor_commands(cmds);

    uint8_t left_dac = (uint8_t)(voltages.vL_cmd / 3.3f * 255.0f);
    uint8_t right_dac = (uint8_t)(voltages.vR_cmd / 3.3f * 255.0f);

    // uint8_t left_dac = (uint8_t)(0.0f / 3.3f * 255.0f);
    // uint8_t right_dac = (uint8_t)(0.0f / 3.3f * 255.0f);

    dac_oneshot_output_voltage(dac_left, left_dac);
    dac_oneshot_output_voltage(dac_right, right_dac);

}