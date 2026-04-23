#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#include <vector>
extern "C" {
#endif

typedef struct {
    uint32_t seq;
    int64_t  t_us;
    uint16_t range_mm[360];
    uint8_t  quality[360];
} ScanFrame;

typedef struct {
    uint32_t nodes_total;
    uint32_t nodes_pushed;
    uint32_t nodes_angle_bad;
    uint32_t nodes_range_bad;
    uint32_t nodes_quality_bad;
    uint32_t frames_published;
    uint32_t start_flags;
} lidar_dbg_t;

extern lidar_dbg_t dbg;

void lidar_start_task(void);
bool lidar_get_latest(ScanFrame* out);

/* New plain-C helper */
bool lidar_get_latest_ranges_mm(float out_ranges[360]);

#ifdef __cplusplus
}
std::vector<float> get_lidar_ranges();
#endif