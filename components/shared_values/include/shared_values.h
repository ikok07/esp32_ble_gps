//
// Created by Kok on 2/18/26.
//

#ifndef ESP32S3_BLE_SHARED_VALUES_H
#define ESP32S3_BLE_SHARED_VALUES_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"

typedef enum {
    SHVAL_ERROR_OK,
    SHVAL_ERROR_VAL_UNAVAILABLE,
    SHVAL_ERROR_SUB_QUEUE_FULL
} SHVAL_ErrorTypeDef;

typedef struct {
    uint32_t InitialValue;
    uint32_t SubscribersQueueSize;
} SHVAL_ConfigTypeDef;

typedef struct {
    uint32_t Value;
    SemaphoreHandle_t Mutex;

    QueueHandle_t SubscribersQueue;
    uint32_t SubscribersCount;
} SHVAL_HandleTypeDef;

SHVAL_HandleTypeDef SHVAL_Init(SHVAL_ConfigTypeDef *Config);
SHVAL_ErrorTypeDef SHVAL_GetValue(const SHVAL_HandleTypeDef *hshval, uint32_t *Value, uint32_t TimeoutMS);
SHVAL_ErrorTypeDef SHVAL_SetValue(SHVAL_HandleTypeDef *hshval, uint32_t Value, uint32_t TimeoutMS);

#endif //ESP32S3_BLE_SHARED_VALUES_H