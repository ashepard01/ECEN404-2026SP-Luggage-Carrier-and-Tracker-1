#include <algorithm>
#include <cmath>
#include <vector>
#include <limits>
#include "oa_module.h"
#include <cstdio>



/*
LiDAR ranges
   ↓
Polar histogram (obstacle density vs angle)
   ↓
Smooth histogram (via angular inflation)
   ↓
Cost over headings (goal + memory + obstacle)
   ↓
Pick heading θ*
   ↓
Convert to steering (v, ω)

Tuning parameters:

Obstacle influence:
    W_OBS
    ROBOT_RADIUS
    SAFETY_DISTANCE
    D_LOOK

Goal vs memory:
    W_GOAL
    W_MEM

Steering gain:
    K_W

Speed scaling:
    MAX_SPEED
    cone_width
    d_stop
    d_safe
    exponent on g_clear
    W_MAX
    V_MIN

Perception window:
    FRONT_MIN
    FRONT_MAX

Most sensitive:
    W_OBS
    d_safe
    K_W
    W_GOAL
    W_MEM
*/

namespace {

// ----------------------------
// Tunable parameters
// ----------------------------

// Physical robot geometry
constexpr float ROBOT_RADIUS    = 0.5f;
constexpr float SAFETY_DISTANCE = 0.30f;

// Lookahead distance (meters)
constexpr float HIST_LOOK = 3.0f;
constexpr float CLEAR_LOOK = 3.5f;

// Histogram resolution
constexpr int BIN_COUNT = 360;

// Only consider front-facing sector (degrees mapped to bins)
constexpr int FRONT_MIN = 0;
constexpr int FRONT_MAX = 180;

// Max allowed speed
constexpr float MAX_SPEED = 0.4f;

// Steering proportional gain
constexpr float K_W = 1.6f;

// Cost weights
constexpr float W_GOAL = 1.2f;   // how much to align with goal
constexpr float W_MEM  = 0.0f;   // how much to resist sharp heading change
constexpr float W_OBS  = 17.0f;  // obstacle penalty

// Turn-rate scaling limit
constexpr float W_MAX = 1.2f;    // rad/s

// Minimum crawl speed
constexpr float V_MIN = 0.0f;

// Persistent memory (previous chosen heading in degrees)
// This replaces the old side/cooldown hysteresis
static float g_prev_theta_deg = 0.0f;

//for unknown ranges
constexpr float UNKNOWN_OBS_WEIGHT = 4.0f;


// ------------------------------------------------------------
// Utility: angle difference in degrees wrapped to [-180, 180]
// ------------------------------------------------------------
inline float angle_diff_deg(float a, float b) {
    float d = std::fmod((a - b + 180.0f), 360.0f);
    if (d < 0) d += 360.0f;
    return d - 180.0f;
}


// ------------------------------------------------------------
// Blind spots (if your LiDAR has structural gaps)
// ------------------------------------------------------------
bool is_blind(int angle_deg) {
    // return (angle_deg >= -70 && angle_deg <= -50) ||
    //        (angle_deg >= 45 && angle_deg <= 65);
    return false;
}


// ------------------------------------------------------------
// Construct polar histogram
// Each bin represents obstacle "density" at that angle
// ------------------------------------------------------------
std::vector<float> construct_polar_histogram(const std::vector<float>& ranges) {

    std::vector<float> H(BIN_COUNT, 0.0f);

    for (int r = 0; r < BIN_COUNT; r++) {

        // Map bin index to angle: i=0 → -180°, i=180 → 0°
        int angle_deg = r - 90; //90 is the front of the rover

        // Skip blind regions
        if (is_blind(angle_deg)) {
            H[r] = std::numeric_limits<float>::infinity();
            continue;
        }

        // Only evaluate front-facing sector
        if (r < FRONT_MIN || r > FRONT_MAX)
            continue;

        // Cap sensing distance
        float raw_range = ranges[r];

        if (raw_range < 0.0f) {
            //H[r] = std::max(H[r], UNKNOWN_OBS_WEIGHT);
            continue;
        }
        
        float r_i = std::min(raw_range, HIST_LOOK);
        
        if (r_i == 0.0f || r_i >= HIST_LOOK)
            continue;

        // Obstacle weight increases quadratically as it gets closer
        float weight = (HIST_LOOK - r_i) * (HIST_LOOK - r_i);

        // Angular inflation (robot width projection)
        float inflation = ROBOT_RADIUS + SAFETY_DISTANCE;

        float alpha;
        if (r_i < inflation)
            alpha = static_cast<float>(M_PI) / 2.0f;
        else
            alpha = std::asin(std::min(1.0f, inflation / r_i));

        const float ALPHA_MAX = 20.0f * static_cast<float>(M_PI) / 180.0f;
        alpha = std::min(alpha, ALPHA_MAX);

        int spread_bins =
            static_cast<int>(alpha * 180.0f / static_cast<float>(M_PI));

        // Spread obstacle influence laterally
        for (int k = -spread_bins; k <= spread_bins; k++) {
            int idx = r + k;
            if (idx >= 0 && idx < BIN_COUNT)
                H[idx] = std::max(H[idx], weight);
        }
    }

    return H;
}


// ------------------------------------------------------------
// Choose best steering direction using cost function
// ------------------------------------------------------------
float choose_direction(const std::vector<float>& H,
                       float heading_error_deg) {

    // --- tie-break settings ---
    constexpr int FRONT_BIN = 90;            // 90 is straight ahead
    constexpr int CENTER_HALF_WIDTH = 12;     // center blockage check: bins 82..98
    constexpr int FORBID_HALF_WIDTH = 20;    // if blocked, disallow headings -12..+12 deg
    constexpr float CENTER_BLOCK_THRESHOLD = 0.65f;

    // Average obstacle cost directly in front
    float center_sum = 0.0f;
    int center_count = 0;
    for (int k = -CENTER_HALF_WIDTH; k <= CENTER_HALF_WIDTH; k++) {
        int idx = FRONT_BIN + k;
        if (idx >= 0 && idx < BIN_COUNT) {
            center_sum += H[idx];
            center_count++;
        }
    }

    float center_avg = (center_count > 0) ? (center_sum / center_count) : 0.0f;
    bool center_blocked = (center_avg > CENTER_BLOCK_THRESHOLD);

    //float best_theta = g_prev_theta_deg;
    float best_theta = 0.0f;
    float best_cost  = std::numeric_limits<float>::infinity();

    for (int r = FRONT_MIN; r <= FRONT_MAX; r++) {

        float theta = static_cast<float>(FRONT_BIN - r); // 0 deg = straight ahead

        // If the center is blocked, do not allow near-zero headings.
        if (center_blocked && std::fabs(theta) <= FORBID_HALF_WIDTH) {

            continue;
        }

        float obstacle_cost = 0.0f;
        float wsum = 0.0f;
        
        for (int k = -22; k <= 22; k++) {
            int idx = r + k;
            if (idx >= FRONT_MIN && idx <= FRONT_MAX) {
                float wk = 1.0f - std::fabs(static_cast<float>(k)) / 22.0f;
                wk = std::max(0.0f, wk);
                obstacle_cost += wk * H[idx];
                wsum += wk;
            }
        }
        
        if (wsum > 0.0f) {
            obstacle_cost /= wsum;
        }
        
        float cost =
            W_GOAL * std::fabs(angle_diff_deg(theta, heading_error_deg)) +
            W_MEM  * std::fabs(angle_diff_deg(theta, g_prev_theta_deg)) +
            W_OBS  * obstacle_cost;

        if (cost < best_cost) {
            best_cost  = cost;
            best_theta = theta;
        }
    }

    return best_theta;
}

} // anonymous namespace


// ============================================================================
// Main OA entry
// ============================================================================
Commands apply_obstacle_avoidance(const std::vector<float>& lidar_ranges,
                                  float theta_goal_rel,
                                  float distance,
                                  RobotState& robot) {

    if (lidar_ranges.size() != BIN_COUNT)
        return Commands{robot.v, robot.w};

    // Convert goal error from radians to degrees
    float heading_error_deg =
        theta_goal_rel * 180.0f / static_cast<float>(M_PI);

    // Build histogram
    std::vector<float> H = construct_polar_histogram(lidar_ranges);

    // Choose best feasible direction
    float theta_star_deg = choose_direction(H, heading_error_deg);

    //TEST
    // float center_cost = H[90];
    // float left_cost   = H[60];
    // float right_cost  = H[120];

    // float theta_star_deg = 0.0f;
    // if (center_cost > 0.2f) {
    //     theta_star_deg = (left_cost < right_cost) ? -30.0f : 30.0f;
    // }
    //TEST

    // Update persistent memory
    g_prev_theta_deg = theta_star_deg;

    // Convert chosen heading to angular velocity
    float w =
        K_W * (theta_star_deg * static_cast<float>(M_PI) / 180.0f);

    // --------------------------------------------------------
    // Clearance-based speed scaling
    // --------------------------------------------------------

    int center_bin = static_cast<int>(90.0f - theta_star_deg); //90 is the front of the rover
    int cone_width = 25;  // degrees around chosen heading

    float d_min = CLEAR_LOOK;

    for (int k = -cone_width; k <= cone_width; k++) {
        int idx = center_bin + k;
        if (idx >= 0 && idx < BIN_COUNT) {
            float d = lidar_ranges[idx];
            if (d > 0.0f) {           // ignore invalid / unknown
                d_min = std::min(d_min, d);
            }
        }
    }

    float d_stop = ROBOT_RADIUS + SAFETY_DISTANCE;
    float d_safe = d_stop + 0.8f;

    // hard stop check directly in front of rover
    float d_front = CLEAR_LOOK;
    for (int k = -15; k <= 15; k++) {
        int idx = (90 + k + BIN_COUNT) % BIN_COUNT;
    
        float d = lidar_ranges[idx];
        if (d > 0.0f) {
            d_front = std::min(d_front, d);
        }
    }

    if (d_front <= d_stop) {
        // Obstacle is too close in front:
        // stop forward motion, but keep turning toward the selected escape heading.

        float theta_turn_deg = theta_star_deg;

        // If OA still picked almost straight ahead, force a side choice.
        if (std::fabs(theta_turn_deg) < 14.0f) {
            float left_sum = 0.0f;
            float right_sum = 0.0f;

            for (int i = 30; i < 90; i++)   left_sum  += H[i];
            for (int i = 91; i < 150; i++)  right_sum += H[i];

            if (left_sum < right_sum) {
                theta_turn_deg = 45.0f;   // turn left
            } else if (right_sum < left_sum) {
                theta_turn_deg = -45.0f;  // turn right
            } else {
                // tie: keep previous turning preference if available
                theta_turn_deg = (g_prev_theta_deg >= 0.0f) ? 35.0f : -35.0f;
            }
        }

        g_prev_theta_deg = theta_turn_deg;

        float w_turn =
            K_W * (theta_turn_deg * static_cast<float>(M_PI) / 180.0f);

        w_turn = std::clamp(w_turn, -W_MAX, W_MAX);

        printf("OA_BLOCKED,dfront=%.2f,dstop=%.2f,th_star=%.1f\n",
                d_front, d_stop, theta_star_deg);

        return Commands{0.0f, w_turn};
    }



    float g_clear;
    if (d_min <= d_stop)
        g_clear = 0.0f;
    else if (d_min >= d_safe)
        g_clear = 1.0f;
    else {
        g_clear = (d_min - d_stop) / (d_safe - d_stop);
        g_clear = g_clear * g_clear;
    }
    
    float g_front;
    if (d_front <= d_stop)
        g_front = 0.0f;
    else if (d_front >= d_safe)
        g_front = 1.0f;
    else {
        g_front = (d_front - d_stop) / (d_safe - d_stop);
        g_front = g_front * g_front;
    }
    
    // Turn-rate scaling
    float g_turn = std::max(0.3f, 1.0f - 0.7f * std::fabs(w) / W_MAX);
    
    // Respect both chosen-heading clearance and direct frontal danger
    float g_speed = std::min(g_clear, g_front);
    
    float v_scale =
        std::max(g_speed * g_turn, V_MIN / MAX_SPEED);

    float v = MAX_SPEED * v_scale;
    //float v = v_ref;
    float w_clamp = std::clamp(w, -W_MAX, W_MAX);

    // printf("OA,th_goal=%.1f,th_star=%.1f,H50=%.2f,H70=%.2f,H90=%.2f,H110=%.2f,H130=%.2f,dmin=%.2f,dfront=%.2f,v=%.2f,w=%.2f\n",
    //     heading_error_deg,
    //     theta_star_deg,
    //     H[50], H[70], H[90], H[110],
    //     H[130],
    //     d_min,
    //     d_front,
    //     v,
    //     w_clamp);

    float left_sum = 0.0f;
    float right_sum = 0.0f;

    for (int i = 30; i < 90; i++)   left_sum  += H[i];
    for (int i = 91; i < 150; i++)  right_sum += H[i];

    // printf("OA_SIDE,left_sum=%.2f,right_sum=%.2f,th_star=%.1f,w=%.2f\n",
    //     left_sum, right_sum, theta_star_deg, w_clamp);

    float raw_left_min = 99.0f;
    float raw_right_min = 99.0f;
    
    for (int i = 50; i <= 80; i++) {
        float d = lidar_ranges[i];
        if (d > 0.0f) raw_left_min = std::min(raw_left_min, d);
    }
    for (int i = 100; i <= 130; i++) {
        float d = lidar_ranges[i];
        if (d > 0.0f) raw_right_min = std::min(raw_right_min, d);
    }
    
    // printf("RAW_SIDE,left_min=%.2f,right_min=%.2f\n", raw_left_min, raw_right_min);
    printf("OA_DBG,th_star=%.1f,dmin=%.2f,dfront=%.2f,gclear=%.2f,gfront=%.2f,v=%.2f,w=%.2f\n",
       theta_star_deg, d_min, d_front, g_clear, g_front, v, w_clamp);

    return Commands{v, w_clamp};
}
