#pragma once

#ifdef __cplusplus
extern "C" {
#endif

RobotState robot = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

void control_task_tuning(void* arg);

#ifdef __cplusplus
}
#endif