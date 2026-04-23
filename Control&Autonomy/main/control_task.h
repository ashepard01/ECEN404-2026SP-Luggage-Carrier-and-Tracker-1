#pragma once

#include <vector>
#include "controllers.h"
#include "odometry.h"
#include "oa_module.h"
#include "state.h"


struct SharedData {
    RobotState robot;
    Waypoint fob;
    std::vector<float> lidar_ranges;
    Commands cmds;
};

#ifdef __cplusplus
extern "C" {
#endif

void control_task(void* arg);
void control_runtime_init(void);
void shared_write_robot(const RobotState& robot);
void shared_write_cmds(Commands cmds);
void shared_write_lidar(const std::vector<float>& lidar_ranges);
void shared_write_fob(const Waypoint& fob);
bool shared_copy_fob(Waypoint* fob);
bool shared_copy_robot(RobotState* robot);
bool shared_copy_cmds(Commands* cmds);
bool shared_copy_lidar(std::vector<float>* lidar_ranges);
bool shared_copy_pose(float* x, float* y, float* th);

#ifdef __cplusplus
}
#endif