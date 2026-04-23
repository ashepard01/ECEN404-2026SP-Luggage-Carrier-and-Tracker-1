#include "lidar_preprocess.h"

#include <algorithm>
#include <cstddef>
#include "esp_timer.h"
#include <cstdint>
#include <utility>

namespace {

// ----------------------------
// Tunable preprocessing params
// ----------------------------

constexpr int BIN_COUNT = 360;

// OA uses front hemisphere 90..270
constexpr int FRONT_MIN = 0;
constexpr int FRONT_MAX = 180;

// Search radius for hole fill
constexpr int FILL_RADIUS = 2;

// Raw lidar helper returns mm, OA expects meters
constexpr float MM_TO_M = 0.001f;

// If no nearby valid sample exists, treat as "clear far away"
constexpr float CLEAR_RANGE_M = 8.0f;

constexpr int64_t HOLD_US = 500000;   // 150 ms
constexpr float UNKNOWN_RANGE_M = -1.0f;

static float last_valid_m[BIN_COUNT];
static int64_t last_valid_us[BIN_COUNT];
static bool hold_initialized = false;

inline int wrap_index(int i) {
    while (i < 0) i += BIN_COUNT;
    while (i >= BIN_COUNT) i -= BIN_COUNT;
    return i;
}

inline bool is_valid_raw_mm(float x_mm) {
    return x_mm > 0.0f;
}

float min_nonzero_neighbor_mm(const float raw_mm[BIN_COUNT], int center, int radius) {
    float best = 0.0f;

    for (int d = 0; d <= radius; ++d) {
        int left  = wrap_index(center - d);
        int right = wrap_index(center + d);

        float a = raw_mm[left];
        float b = raw_mm[right];

        if (is_valid_raw_mm(a)) {
            best = (best == 0.0f) ? a : std::min(best, a);
        }
        if (is_valid_raw_mm(b)) {
            best = (best == 0.0f) ? b : std::min(best, b);
        }
    }

    return best;
}

} // anonymous namespace

#include <cstdio>



std::vector<float> get_clean_lidar_ranges() {
    float raw_mm[BIN_COUNT] = {0.0f};
    std::vector<float> clean_m(BIN_COUNT, CLEAR_RANGE_M);


    if (!hold_initialized) {
        for (int i = 0; i < BIN_COUNT; ++i) {
            last_valid_m[i] = CLEAR_RANGE_M;
            last_valid_us[i] = 0;
        }
        hold_initialized = true;
    }
    
    int64_t now_us = esp_timer_get_time();

    // If no frame yet, return all-clear array instead of zeros.
    if (!lidar_get_latest_ranges_mm(raw_mm)) {
        return clean_m;
    }

    // Convert direct valid samples everywhere.
    for (int i = 0; i < BIN_COUNT; ++i) {
        if (is_valid_raw_mm(raw_mm[i])) {
            clean_m[i] = raw_mm[i] * MM_TO_M;
            last_valid_m[i] = clean_m[i];
            last_valid_us[i] = now_us;
        }
    }

    // Fill holes only in the front hemisphere used by OA.
    for (int i = FRONT_MIN; i <= FRONT_MAX; ++i) {
        if (is_valid_raw_mm(raw_mm[i])) {
            clean_m[i] = raw_mm[i] * MM_TO_M;
            last_valid_m[i] = clean_m[i];
            last_valid_us[i] = now_us;
            continue;
        }
    
        float filled_mm = min_nonzero_neighbor_mm(raw_mm, i, FILL_RADIUS);
    
        if (is_valid_raw_mm(filled_mm)) {
            clean_m[i] = filled_mm * MM_TO_M;
            last_valid_m[i] = clean_m[i];
            last_valid_us[i] = now_us;
            continue;
        }
    
        if ((now_us - last_valid_us[i]) <= HOLD_US) {
            clean_m[i] = last_valid_m[i];
        } else {
            clean_m[i] = UNKNOWN_RANGE_M;
        }
    }

    // Guarantee no zeros anywhere.
    for (int i = 0; i < BIN_COUNT; ++i) {
        if (clean_m[i] == 0.0f) {
            clean_m[i] = CLEAR_RANGE_M;
        }
    }

    //debug_cleaned_ranges(raw_mm, clean_m);

    return clean_m;
}

// extern "C" void lidar_publish_task(void* arg)
// {
//     const TickType_t period = pdMS_TO_TICKS(50);
//     TickType_t last_wake = xTaskGetTickCount();
//     std::vector<float> lidar_ranges;

//     while (true) {
//         lidar_ranges = get_clean_lidar_ranges();
//         shared_write_lidar(lidar_ranges);
//         vTaskDelayUntil(&last_wake, period);
//     }
// }