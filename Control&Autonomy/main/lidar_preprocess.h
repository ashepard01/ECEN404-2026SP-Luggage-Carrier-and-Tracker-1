#pragma once

#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "control_task.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "rplidar.h"

// void lidar_publish_task(void* arg);

#ifdef __cplusplus
}
#endif

std::vector<float> get_clean_lidar_ranges();