//
// Created by Kok on 2/12/26.
//

#ifndef ESP32S3_APP_STATE_H
#define ESP32S3_APP_STATE_H

#include "task_scheduler.h"
#include "shared_values.h"

#include "m10.h"
#include "ble.h"

typedef struct {
    SCHEDULER_TaskTypeDef BleTask;
    SCHEDULER_TaskTypeDef GnssUartTask;
    SCHEDULER_TaskTypeDef TelemetryParserTask;
    SCHEDULER_TaskTypeDef BTLocAndSpdNotifyTask;
    SCHEDULER_TaskTypeDef GnssFixQltNotifyTask;
    SCHEDULER_TaskTypeDef ElevationNotifyTask;
    SCHEDULER_TaskTypeDef DateTimeNotifyTask;
} APP_TasksTypeDef;

typedef struct {
    SHVAL_PointerHandleTypeDef GnssBaseData;
    SHVAL_PointerHandleTypeDef GnssPrecisionData;
} APP_SharedValuesTypeDef;

typedef struct {
    QueueHandle_t TelemetryParserQueue;
} APP_SharedQueuesTypeDef;

typedef struct {
    M10_HandleTypeDef *hm10;
    BLE_HandleTypeDef *hble;
    APP_TasksTypeDef *Tasks;
    APP_SharedValuesTypeDef *SharedValues;
    APP_SharedQueuesTypeDef *Queues;
} APP_StateTypeDef;

extern APP_StateTypeDef gAppState;

void APP_Init();

#endif //ESP32S3_APP_STATE_H