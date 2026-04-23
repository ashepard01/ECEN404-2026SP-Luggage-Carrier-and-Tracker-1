#include "outer_controller.h"
#include "oa_module.h"
#include "control_task.h"

SharedData* g_shared = get_shared_data();


Commands outer_controller(float heading_error, float distance_to_goal){

    if (distance_to_goal < 1.7f){
        return Commands{0.0f, 0.0f};
    }

    

}






