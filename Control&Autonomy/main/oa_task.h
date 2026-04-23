#ifndef OA_TASK_H
#define OA_TASK_H

#include "oa_module.h"
#include "encoder.h"
#include "odometry.h"
#include "MotorDriver.h"
#include "controllers.h"
#include "control_task.h"
#include "lidar_preprocess.h"

#ifdef __cplusplus
extern "C" {
#endif

void oa_task(void* arg);

#ifdef __cplusplus
}
#endif

#endif // OA_TASK_H