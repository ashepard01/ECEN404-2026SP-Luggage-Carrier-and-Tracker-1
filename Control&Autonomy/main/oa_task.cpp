#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <vector>
#include <cmath>
#include "oa_task.h"
#include "state_machine.h"
#include "shared_state.h"


extern "C" void oa_task(void* arg)
{
    const TickType_t period = pdMS_TO_TICKS(333);   // 5 Hz to start
    TickType_t last_wake = xTaskGetTickCount();

    // Optional: initialize local tuning refs / test values
    // float v_ref = 0.2f;
    // float w_ref = 0.4f;
    float theta_goal_rel = 0.0;
    float distance_to_goal = 1000.0f;

    // const float goal_x = 5.0f;
    // const float goal_y = 5.0f;

    // 360-bin placeholder LiDAR vector for bypass / future use
    std::vector<float> lidar_ranges;
    RobotState robot;




    while (true) {

        State_t state = get_rover_state();

        if (state != STATE_NAVIGATING) {
            shared_write_cmds(Commands{0.0f, 0.0f});
            vTaskDelayUntil(&last_wake, period);
            continue;
        }

        bool ok_lidar = shared_copy_lidar(&lidar_ranges);
        bool ok_robot = shared_copy_robot(&robot);

        if (ok_lidar && ok_robot) {
            // float dx = goal_x - robot.x;
            // float dy = goal_y - robot.y;
            // float distance_to_goal = std::sqrt(dx * dx + dy * dy);
            // float theta_goal = std::atan2(dy, dx);
            // float theta_goal_rel = wrap_angle(theta_goal - robot.th);


            int unknown_count = 0;
            for (int i = 0; i <= 180; ++i) {
                if (lidar_ranges[i] < 0.0f) unknown_count++;
            }

            printf("LDBG,0=%.2f,45=%.2f,90=%.2f,135=%.2f,180=%.2f,unknown=%d\n",
                lidar_ranges[0], lidar_ranges[45], lidar_ranges[90],
                lidar_ranges[135], lidar_ranges[180], unknown_count);

            int valid_front = 0;
            for (int i = 0; i <= 180; ++i) {
                if (lidar_ranges[i] > 0.0f) valid_front++;
            }

            if (valid_front < 40) {
                shared_write_cmds(Commands{0.0f, 0.0f});
                vTaskDelayUntil(&last_wake, period);
                continue;
            }


            Commands cmds = apply_obstacle_avoidance(lidar_ranges, theta_goal_rel, distance_to_goal, robot);

            // printf("OA,th_goal=%.3f,dist=%.3f,v=%.3f,w=%.3f,x=%.3f,y=%.3f,th=%.3f\n",
            //        theta_goal_rel, distance_to_goal,
            //        cmds.v, cmds.w,
            //        robot.x, robot.y, robot.th);

            static float v_cmd_prev = 0.0f;
            static float w_cmd_prev = 0.0f;

            const float dt = 0.333f;
            const float v_rate = 0.15f;   // m/s per second
            const float w_rate = 0.6f;    // rad/s per second

            float dv_max = v_rate * dt;
            float dw_max = w_rate * dt;

            float dv = cmds.v - v_cmd_prev;
            if (dv > dv_max) dv = dv_max;
            if (dv < -dv_max) dv = -dv_max;

            float dw = cmds.w - w_cmd_prev;
            if (dw > dw_max) dw = dw_max;
            if (dw < -dw_max) dw = -dw_max;

            cmds.v = v_cmd_prev + dv;
            cmds.w = w_cmd_prev + dw;

            v_cmd_prev = cmds.v;
            w_cmd_prev = cmds.w;
            
            shared_write_cmds(cmds);

            // printf("GOAL,goal_x=%.3f,goal_y=%.3f,dx=%.3f,dy=%.3f,dist=%.3f,th_goal_rel=%.3f\n",
            //     goal_x, goal_y, dx, dy, distance_to_goal, theta_goal_rel);
        }
        vTaskDelayUntil(&last_wake, period);
    }
}