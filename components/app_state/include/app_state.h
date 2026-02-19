//
// Created by Kok on 2/12/26.
//

#ifndef ESP32S3_APP_STATE_H
#define ESP32S3_APP_STATE_H

#include "timer.h"
#include "task_scheduler.h"
#include "ble.h"
#include "shared_values.h"

typedef struct {
    SCHEDULER_TaskTypeDef LedTask;
    SCHEDULER_TaskTypeDef LedNotifyTask;
    SCHEDULER_TaskTypeDef BleTask;
} APP_TasksTypeDef;

typedef struct {
    SHVAL_HandleTypeDef LedLightState;
    SHVAL_HandleTypeDef LedAutoCycleEnabled;
} APP_SharedValuesTypeDef;

typedef struct {
    TIMER_HandleTypeDef *htimled;
    BLE_HandleTypeDef *hble;
    APP_TasksTypeDef *Tasks;
    APP_SharedValuesTypeDef *SharedValues;
} APP_StateTypeDef;

extern APP_StateTypeDef gAppState;

void APP_Init();

#endif //ESP32S3_APP_STATE_H