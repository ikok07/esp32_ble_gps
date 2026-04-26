//
// Created by Kok on 2/12/26.
//

#include "app_state.h"

led_strip_handle_t hstatusled;
FS_HandleTypeDef hfs;
M10_HandleTypeDef hm10;
BLE_HandleTypeDef hble;

APP_TasksTypeDef tasks;
APP_SharedValuesTypeDef shared_values;
APP_SharedQueuesTypeDef shared_queues;

APP_StateTypeDef gAppState;

void APP_Init() {
    gAppState = (APP_StateTypeDef){
        .hfs = &hfs,
        .hstatusled = &hstatusled,
        .hm10 = &hm10,
        .hble = &hble,
        .Tasks = &tasks,
        .SharedValues = &shared_values,
        .Queues = &shared_queues
    };

}
