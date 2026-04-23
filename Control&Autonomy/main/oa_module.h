#ifndef OA_MODULE_H
#define OA_MODULE_H

#include <vector>
#include "state.h"

/*
Obstacle Avoidance Module (Histogram-Based)

Takes:
    - Base linear and angular velocity (v_ref, w_ref)
    - 360° LiDAR ranges (size must be 360)
    - Goal heading error (theta_goal_rel in radians)
    - Distance to goal (currently unused but kept for extensibility)

Returns:
    - Modified Commands {v, w} after obstacle avoidance

Design:
    Waypoint layer → produces intent (v_ref, w_ref)
    OA layer       → enforces feasibility constraints
*/

struct Commands {
    float v;
    float w;
};

Commands apply_obstacle_avoidance(
    const std::vector<float>& lidar_ranges,
    float theta_goal_rel,
    float distance_to_goal,
    RobotState& robot
);

#endif // OA_MODULE_H
