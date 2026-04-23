#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define CONTROL_TASK_PERIOD_MS 50

void control_task(void* arg);

#ifdef __cplusplus
}
#endif