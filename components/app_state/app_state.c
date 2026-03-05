//
// Created by Kok on 2/12/26.
//

#include "app_state.h"

TIMER_HandleTypeDef htim;
BLE_HandleTypeDef hble;
M10_HandleTypeDef hm10;
APP_TasksTypeDef tasks;
APP_SharedValuesTypeDef shared_values;

APP_StateTypeDef gAppState;

void APP_Init() {
    gAppState = (APP_StateTypeDef){
        .htimled = &htim,
        .hble = &hble,
        .hm10 = &hm10,
        .Tasks = &tasks,
        .SharedValues = &shared_values
    };
}
