//
// Created by Kok on 2/12/26.
//

#ifndef ESP32S3_APP_STATE_H
#define ESP32S3_APP_STATE_H

#include "timer.h"
#include "task_scheduler.h"
#include "ble.h"

typedef struct {
    SCHEDULER_TaskTypeDef LedTask;
    SCHEDULER_TaskTypeDef BleTask;
} APP_Tasks;

typedef struct {
    TIMER_HandleTypeDef *htim;
    BLE_HandleTypeDef *hble;
    APP_Tasks *Tasks;
} APP_State;

extern APP_State gAppState;

#endif //ESP32S3_APP_STATE_H