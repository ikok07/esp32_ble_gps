//
// Created by Kok on 2/18/26.
//

#include "shared_values.h"

SHVAL_HandleTypeDef SHVAL_Init(SHVAL_ConfigTypeDef *Config) {
    return (SHVAL_HandleTypeDef){
        .Value = Config->InitialValue,
        .Mutex = xSemaphoreCreateMutex(),
        .SubscribersQueue = xQueueCreate(Config->SubscribersQueueSize, sizeof(uint32_t)),
        .SubscribersCount = Config->SubscribersQueueSize
    };
}

SHVAL_ErrorTypeDef SHVAL_GetValue(const SHVAL_HandleTypeDef *hshval, uint32_t *Value, uint32_t TimeoutMS) {
    if (xSemaphoreTake(hshval->Mutex, pdMS_TO_TICKS(TimeoutMS))) {
        *Value = hshval->Value;
        xSemaphoreGive(hshval->Mutex);
        return SHVAL_ERROR_OK;
    } else {
        return SHVAL_ERROR_VAL_UNAVAILABLE;
    }
}

SHVAL_ErrorTypeDef SHVAL_SetValue(SHVAL_HandleTypeDef *hshval, uint32_t Value, uint32_t TimeoutMS) {
    if (xSemaphoreTake(hshval->Mutex, pdMS_TO_TICKS(TimeoutMS))) {
        hshval->Value = Value;
        for (int i = 0; i < hshval->SubscribersCount; i++) {
            xQueueSend(hshval->SubscribersQueue, &Value, portMAX_DELAY);
        }
        xSemaphoreGive(hshval->Mutex);
        return SHVAL_ERROR_OK;
    } else {
        return SHVAL_ERROR_VAL_UNAVAILABLE;
    }
}
