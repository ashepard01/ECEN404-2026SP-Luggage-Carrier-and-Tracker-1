#include <stdio.h>
#include <cmath>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "PID.h"
#include "controllers.h"

#include "esp_timer.h"
#include "encoder.h"
#include "MotorDriver.h"
#include "odometry.h"


// ----------------------------------------------------
// Configuration
// ----------------------------------------------------




// ----------------------------------------------------
// Simple deadband compensation
// ----------------------------------------------------
// static float apply_deadband(float u)
// {
//     if (std::fabs(u) < 1e-4f)
//         return 0.0f;

//     float sign = (u >= 0.0f) ? 1.0f : -1.0f;
//     float mag  = std::fabs(u);

//     if (mag < kDeadbandVelocity)
//         mag = kDeadbandVelocity;

//     return sign * mag;
// }


static constexpr TickType_t kControlPeriodMs = 10;
static constexpr float kControlPeriodS = kControlPeriodMs / 1000.0f; // 100 Hz


// ----------------------------------------------------
// Control Task (Velocity Tuning Harness)
// ----------------------------------------------------
extern "C" void control_task_tuning(void* arg)
{
    setvbuf(stdin, NULL, _IONBF, 0);   // disable buffering
    // Independent wheel PIDs
    // (float integral, float prev_meas, float kp, float ki, float kd, 
    // float u_min, float u_max, 
    //float i_min, float i_max, float prev_error)
    PID pid_L(0.0f, 0.0f, 0.7f, 0.8f, 0.0f,
              -1.0, 1.0,
              -5.0f, 5.0f, 0.0f);

    PID pid_R(0.0f, 0.0f, 0.7f, 0.8f, 0.0f,
              -1.0, 1.0,
              -5.0f, 5.0f, 0.0f);

    RobotState robot = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    PIDs pids = {pid_L, pid_R};
    const TickType_t period = pdMS_TO_TICKS(kControlPeriodMs);
    TickType_t last_wake = xTaskGetTickCount();
    write_motor_voltages(MotorCommands{0.0f, 0.0f});
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(2000));
    while (1) {
        encoder_update(kControlPeriodS); // calculates m/s wheel velocity
        update_state(robot);
        //write_motor_voltages(MotorCommands{-1.0f, -1.0f});
        auto left_vel = encoder_get_left_velocity();
        auto right_vel = encoder_get_right_velocity();
        MotorCommands cmds = update_controllers(1.5, 0.0, left_vel, right_vel, pids);

        write_motor_voltages(cmds);
        //printf("%.6f, %.6f\n", left_vel, cmds.vL_cmd);
        printf("%.6f, %.6f, %.6f\n", encoder_get_left_velocity(), encoder_get_right_velocity(), robot.th);
        //fflush(stdout);
        vTaskDelayUntil(&last_wake, period);
    }
    while (true) {
        write_motor_voltages(MotorCommands{0.0f, 0.0f});
        vTaskDelayUntil(&last_wake, period);
    }
    // okay so 5V is full back 0V is full forward. 1 = 0 and -1 = 5 (-u + 1)
}
// 0  full fowrdward. so w_max if setpoint is 0 then error = w_max. w_max * K_p = ~2.5