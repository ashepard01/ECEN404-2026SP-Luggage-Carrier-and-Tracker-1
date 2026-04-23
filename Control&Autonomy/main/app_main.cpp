#include "control_task.h"
#include "comms.h"
#include "shared_state.h"

extern "C" {
#include "nvs_flash.h"
#include "esp_log.h"
#include "rplidar.h"
}

#include "encoder.h"
#include "MotorDriver.h"
#include "state_machine.h"
#include "manual_test.h"
#include "velocity_test.h"
#include "oa_task.h"
#include "control_task.h"
#include "uwb_rx_task.h"
#include "localization.h"
#include "lidar_preprocess.h"
#include "state_machine_task.h"
#include "alarm_test_task.h"


extern "C" void oa_task_tuning(void* arg);
extern "C" void control_task(void* arg);


extern "C" void manual_test(void* arg);
extern "C" void uwb_rx_task(void* arg);

extern "C" void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(ret);

    
    encoder_init();
    motor_driver_init();

    MotorCommands neutral_cmds = {0.0f, 0.0f};
    write_motor_voltages(neutral_cmds);

    vTaskDelay(pdMS_TO_TICKS(3000));
    

    shared_state_init();
    state_machine_init();
    wifi_init_softap();
    start_http_server();
    lidar_start_task();
    vTaskDelay(pdMS_TO_TICKS(5000));
    control_runtime_init();
    uwb_mutex_init();
    std::vector<float> lidar_ranges;

    // xTaskCreate(
    //     alarm_test_task,
    //     "alarm_test_task",
    //     4096,
    //     nullptr,
    //     5,
    //     nullptr
    // );


    xTaskCreate(localization_task, 
                    "localization_task", 
                    8192, 
                    NULL, 
                    5, 
                    NULL);

    xTaskCreate(uwb_rx_task,        // task function
                    "uwb_rx_task",      // name
                    4096,               // stack size in bytes
                    NULL,               // parameter
                    4,                  // priority
                    NULL);

    xTaskCreatePinnedToCore(state_machine_task,
                            "state_machine_task",
                            4096,
                            nullptr,
                            6,
                            nullptr,
                            1);


    xTaskCreatePinnedToCore(oa_task, 
                            "oa_task", 
                            4096, 
                            nullptr, 
                            6, 
                            nullptr, 
                            0);

    xTaskCreatePinnedToCore(control_task,
                            "control_task",
                            4096,
                            nullptr,
                            6,
                            nullptr,
                            0);



    ESP_LOGI("APP_MAIN", "System started");
}