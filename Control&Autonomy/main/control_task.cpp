#include <stdio.h>

#include "control_task.h"

#include "encoder.h"
#include "MotorDriver.h"
#include "shared_state.h"
#include "controllers.h"
#include "oa_module.h"
#include "odometry.h"
#include "state_machine.h"
#include "PID.h"
extern "C" {
#include "freertos/task.h"
#include "esp_log.h"
}

#include <cstring>
#include <vector>







static SharedData g_shared_storage = {};
static SharedData* g_shared = &g_shared_storage;
static SemaphoreHandle_t g_shared_mutex = NULL;


static const char *TAG = "CONTROL_TASK";



void control_runtime_init(void) {
    g_shared->robot.x = 0.0f;
    g_shared->robot.y = 0.0f;
    g_shared->robot.th = 0.0f;

    //temp test values
    g_shared->fob.distance = 0.0f;
    g_shared->fob.heading_error = 0.0f;
    g_shared->fob.active = true;
    //temp test values

    g_shared->lidar_ranges.clear();
    g_shared->lidar_ranges.reserve(360);

    g_shared->cmds.v = 0.0f;
    g_shared->cmds.w = 0.0f;

    if (g_shared_mutex == NULL) {
        g_shared_mutex = xSemaphoreCreateMutex();
        configASSERT(g_shared_mutex != NULL);
    }

}

void shared_write_fob(const Waypoint& fob) {
    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    g_shared->fob = fob;
    xSemaphoreGive(g_shared_mutex);
}


bool shared_copy_fob(Waypoint* fob) {
    if (!fob) return false;
    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    *fob = g_shared->fob;
    xSemaphoreGive(g_shared_mutex);
    return true;
}

void shared_write_lidar(const std::vector<float>& lidar_ranges) {
    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    g_shared->lidar_ranges = lidar_ranges;
    xSemaphoreGive(g_shared_mutex);
}

void shared_write_cmds(Commands cmds) {
    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    g_shared->cmds = cmds;
    xSemaphoreGive(g_shared_mutex);
}

void shared_write_robot(const RobotState& robot) {
    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    g_shared->robot = robot;
    xSemaphoreGive(g_shared_mutex);
}

bool shared_copy_robot(RobotState* robot) {
    if (!robot) return false;
    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    *robot = g_shared->robot;
    xSemaphoreGive(g_shared_mutex);
    return true;
}

bool shared_copy_cmds(Commands* cmds) {
    if (!cmds) return false;
    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    *cmds = g_shared->cmds;
    xSemaphoreGive(g_shared_mutex);
    return true;
}

bool shared_copy_pose(float* x, float* y, float* th) {
    if (!x || !y || !th) return false;

    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    *x  = g_shared->robot.x;
    *y  = g_shared->robot.y;
    *th = g_shared->robot.th;
    xSemaphoreGive(g_shared_mutex);

    return true;
}

bool shared_copy_lidar(std::vector<float>* lidar_ranges) {
    if (!lidar_ranges) return false;

    xSemaphoreTake(g_shared_mutex, portMAX_DELAY);
    *lidar_ranges = g_shared->lidar_ranges;
    xSemaphoreGive(g_shared_mutex);

    return true;
}


void control_task(void* arg)
{

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(kControlPeriodMs);
    RobotState robot;
    Commands cmds;
    PIDs pids = init_controllers();
    State_t prev_state = get_rover_state();
    int print_div = 0;

    while (true) {
        // replace with real state-machine / PID logic
        // state_machine_step();

        State_t state = get_rover_state();

                // Reset whenever entering or leaving navigation mode
        if ((prev_state == STATE_NAVIGATING) && (state != STATE_NAVIGATING)) {
            pids = init_controllers();
            write_motor_voltages({0.0f, 0.0f});
        }
        else if ((prev_state != STATE_NAVIGATING) && (state == STATE_NAVIGATING)) {
            pids = init_controllers();
        }

        prev_state = state;


        if (state != STATE_NAVIGATING) {
            vTaskDelayUntil(&last_wake, period);
            continue;
        }

        bool ok_robot = shared_copy_robot(&robot);
        bool ok_cmds = shared_copy_cmds(&cmds);
        encoder_update(kControlPeriodMs / 1000.0f); // calculates m/s wheel velocity
        float vL = encoder_get_left_velocity();
        float vR = encoder_get_right_velocity();

        

        if (ok_robot && ok_cmds) {
            robot = encoder_update_state(robot, vL, vR);
            shared_write_robot(robot);
            MotorCommands motor_cmds = update_controllers(cmds.v, cmds.w, robot.vL, robot.vR, pids);
            write_motor_voltages(motor_cmds);


            // if (++print_div >= 10) {
            //     print_div = 0;
            //     printf("POSE,x=%.3f,y=%.3f,th=%.3f,v=%.3f,w=%.3f,vL=%.3f,vR=%.3f\n",
            //         robot.x, robot.y, robot.th,
            //         cmds.v, cmds.w,
            //         robot.vL, robot.vR);
            // }

        }

        // printf("CTRL_REF,v=%.3f,w=%.3f,measL=%.3f,measR=%.3f,x=%.3f,y=%.3f,th=%.3f\n",
        //     cmds.v, cmds.w,
        //     robot.vL, robot.vR, robot.x, robot.y, robot.th);

        //printf("y=%.3f,x=%.3f\n", robot.x, robot.y);

        // if (ok_cmds && ok_robot) {
        //     MotorCommands motor_cmds = update_controllers(v_cmd, w_cmd, robot.vL, robot.vR, pids);
        //     write_motor_voltages(motor_cmds);
        // }

        
        vTaskDelayUntil(&last_wake, period);
    }




    
    // while(1){
    //     write_motor_voltages({0.0, 0.0});
    //     vTaskDelayUntil(&last_wake, period);
    // }
}

// void control_task(void* arg)
// {
//     TickType_t last_wake = xTaskGetTickCount();
//     const TickType_t period = pdMS_TO_TICKS(kControlPeriodMs);

//     const float MOVE_THRESH = 0.02f;         // m/s threshold to count as real motion
//     const int HOLD_CYCLES_REQUIRED = 3;      // debounce against noise
//     const float RAMP_TIME_S = 8.0f;          // seconds to ramp
//     const float CMD_MAX = 0.35f;             // max raw motor command to test

//     while (true) {
//         // keep encoders fresh
//         encoder_update(kControlPeriodMs / 1000.0f);
//         float vL = encoder_get_left_velocity();
//         float vR = encoder_get_right_velocity();

//         // =====================================================
//         // VERSION 1: FORWARD BREAKAWAY TEST
//         // Uncomment this block to test forward deadband
//         // =====================================================
        
//         // {
//         //     static float test_time_s = 0.0f;
//         //     static int moving_count_L = 0;
//         //     static int moving_count_R = 0;
//         //     static bool left_reported = false;
//         //     static bool right_reported = false;

//         //     float alpha = test_time_s / RAMP_TIME_S;
//         //     if (alpha > 1.0f) alpha = 1.0f;

//         //     float u = CMD_MAX * alpha;

//         //     write_motor_voltages({u, u});

//         //     if (fabsf(vL) >= MOVE_THRESH) moving_count_L++;
//         //     else                          moving_count_L = 0;

//         //     if (fabsf(vR) >= MOVE_THRESH) moving_count_R++;
//         //     else                          moving_count_R = 0;

//         //     if (!left_reported && moving_count_L >= HOLD_CYCLES_REQUIRED) {
//         //         left_reported = true;
//         //         printf("BREAKAWAY_FWD_LEFT,u=%.3f,t=%.2f,vL=%.3f,vR=%.3f\n",
//         //                u, test_time_s, vL, vR);
//         //     }

//         //     if (!right_reported && moving_count_R >= HOLD_CYCLES_REQUIRED) {
//         //         right_reported = true;
//         //         printf("BREAKAWAY_FWD_RIGHT,u=%.3f,t=%.2f,vL=%.3f,vR=%.3f\n",
//         //                u, test_time_s, vL, vR);
//         //     }

//         //     printf("DBTEST_FWD,t=%.2f,u=%.3f,vL=%.3f,vR=%.3f\n",
//         //            test_time_s, u, vL, vR);

//         //     if (alpha < 1.0f) {
//         //         test_time_s += kControlPeriodMs / 1000.0f;
//         //     }
//         // }
        

//         // =====================================================
//         // VERSION 2: REVERSE BREAKAWAY TEST
//         // Uncomment this block to test reverse deadband
//         // =====================================================
//         {
//             static float test_time_s = 0.0f;
//             static int moving_count_L = 0;
//             static int moving_count_R = 0;
//             static bool left_reported = false;
//             static bool right_reported = false;

//             float alpha = test_time_s / RAMP_TIME_S;
//             if (alpha > 1.0f) alpha = 1.0f;

//             float u = -CMD_MAX * alpha;

//             write_motor_voltages({u, u});

//             if (fabsf(vL) >= MOVE_THRESH) moving_count_L++;
//             else                          moving_count_L = 0;

//             if (fabsf(vR) >= MOVE_THRESH) moving_count_R++;
//             else                          moving_count_R = 0;

//             if (!left_reported && moving_count_L >= HOLD_CYCLES_REQUIRED) {
//                 left_reported = true;
//                 printf("BREAKAWAY_REV_LEFT,u=%.3f,t=%.2f,vL=%.3f,vR=%.3f\n",
//                        u, test_time_s, vL, vR);
//             }

//             if (!right_reported && moving_count_R >= HOLD_CYCLES_REQUIRED) {
//                 right_reported = true;
//                 printf("BREAKAWAY_REV_RIGHT,u=%.3f,t=%.2f,vL=%.3f,vR=%.3f\n",
//                        u, test_time_s, vL, vR);
//             }

//             printf("DBTEST_REV,t=%.2f,u=%.3f,vL=%.3f,vR=%.3f\n",
//                    test_time_s, u, vL, vR);

//             if (alpha < 1.0f) {
//                 test_time_s += kControlPeriodMs / 1000.0f;
//             }
//         }
            

//         vTaskDelayUntil(&last_wake, period);
//     }
// }