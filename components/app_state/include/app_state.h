//
// Created by Kok on 2/12/26.
//

#ifndef ESP32S3_APP_STATE_H
#define ESP32S3_APP_STATE_H

#include "task_scheduler.h"
#include "shared_values.h"
#include "led_strip.h"

#include "fs.h"
#include "m10.h"
#include "ble.h"

typedef struct {
    SCHEDULER_TaskTypeDef StatusLedTask;
    SCHEDULER_TaskTypeDef GnssUartTask;
    SCHEDULER_TaskTypeDef GnssConfigTask;
    SCHEDULER_TaskTypeDef GnssSaveDataTask;
    SCHEDULER_TaskTypeDef BleTask;
    SCHEDULER_TaskTypeDef TelemetryParserTask;
    SCHEDULER_TaskTypeDef BTLocAndSpdNotifyTask;
    SCHEDULER_TaskTypeDef GnssFixQltNotifyTask;
    SCHEDULER_TaskTypeDef ElevationNotifyTask;
    SCHEDULER_TaskTypeDef GnssDateTimeUpdTask;
    SCHEDULER_TaskTypeDef CheckGnssFixTask;
} APP_TasksTypeDef;

typedef struct {
    SHVAL_PointerHandleTypeDef GnssBaseData;
    SHVAL_PointerHandleTypeDef GnssPrecisionData;
    SHVAL_PointerHandleTypeDef GnssAuxUtcUpdateData;                    // Provided by the connected BLE device
} APP_SharedValuesTypeDef;

typedef struct {
    QueueHandle_t TelemetryParserQueue;
} APP_SharedQueuesTypeDef;

typedef struct {
    FS_HandleTypeDef *hfs;
    led_strip_handle_t *hstatusled;
    M10_HandleTypeDef *hm10;
    BLE_HandleTypeDef *hble;
    APP_TasksTypeDef *Tasks;
    APP_SharedValuesTypeDef *SharedValues;
    APP_SharedQueuesTypeDef *Queues;
} APP_StateTypeDef;

extern APP_StateTypeDef gAppState;

void APP_Init();

#endif //ESP32S3_APP_STATE_H