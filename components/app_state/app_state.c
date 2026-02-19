//
// Created by Kok on 2/12/26.
//

#include "app_state.h"

TIMER_HandleTypeDef htim;
BLE_HandleTypeDef hble;
APP_TasksTypeDef tasks;
APP_SharedValuesTypeDef shared_values;

APP_StateTypeDef gAppState;

void APP_Init() {
    gAppState = (APP_StateTypeDef){
        .htimled = &htim,
        .hble = &hble,
        .Tasks = &tasks,
        .SharedValues = &shared_values
    };
}
