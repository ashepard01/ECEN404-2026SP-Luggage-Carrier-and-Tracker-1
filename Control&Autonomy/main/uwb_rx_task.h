#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x;
    float y;
    float heading;
    float distance_to_user;
    float heading_error;
    bool valid;
    uint32_t last_update_ms;
} uwb_data_t;

void uwb_mutex_init(void);
void shared_write_uwb_data(const uwb_data_t* data);
bool shared_copy_uwb_data(uwb_data_t* data);
void uwb_rx_task(void* arg);

#ifdef __cplusplus
}
#endif