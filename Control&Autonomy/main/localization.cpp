#include "localization.h"
#include "control_task.h"
#include "lidar_preprocess.h"


void localization_task(void* arg){
    const TickType_t period = pdMS_TO_TICKS(200); // 5 Hz
    TickType_t last_wake = xTaskGetTickCount();
    RobotState robot;
    Waypoint fob;


    while (true){
        // uwb_data_t uwb_data;
        // if (shared_copy_uwb_data(&uwb_data)) {
        //     if (uwb_data.valid) {
        //         if (shared_copy_robot(&robot)) {
        //             robot = uwb_update_state(robot, uwb_data.x, uwb_data.y, uwb_data.heading);
        //             shared_write_robot(robot);

        //         }
        //         if (shared_copy_fob(&fob)) {
        //             fob.distance = uwb_data.distance_to_user;
        //             fob.heading_error = uwb_data.heading_error;
        //             fob.active = true;
        //             shared_write_fob(fob);
        //         }
        //     }
        // }

        std::vector<float> lidar_ranges = get_clean_lidar_ranges();
        if (lidar_ranges.size() > 0){
            shared_write_lidar(lidar_ranges);
        }

        vTaskDelayUntil(&last_wake, period);
    }


}
