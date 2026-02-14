//
// Created by Kok on 2/12/26.
//

#include "app_state.h"

TIMER_HandleTypeDef htim;
BLE_HandleTypeDef hble;
APP_Tasks tasks;

APP_State gAppState = {
    .htim = &htim,
    .hble = &hble,
    .Tasks = &tasks
};