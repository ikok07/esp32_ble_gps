//
// Created by Kok on 2/18/26.
//

#include "shared_values.h"

SHVAL_HandleTypeDef SHVAL_Init(SHVAL_ConfigTypeDef *Config) {
    SHVAL_HandleTypeDef handle = {
        .Value = Config->InitialValue,
        .Mutex = xSemaphoreCreateMutex()
    };

    if (Config->SubscribersQueueSize > 0) {
        handle.SubscribersQueue = xQueueCreate(Config->SubscribersQueueSize, sizeof(uint32_t));
        handle.SubscribersCount = Config->SubscribersQueueSize;
    }

    return handle;
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

        if (hshval->SubscribersQueue) {
            // Queue is reset. All previous values are invalid
            xQueueReset(hshval->SubscribersQueue);
            for (int i = 0; i < hshval->SubscribersCount; i++) {
                xQueueSend(hshval->SubscribersQueue, &Value, portMAX_DELAY);
            }
        }

        xSemaphoreGive(hshval->Mutex);
        return SHVAL_ERROR_OK;
    } else {
        return SHVAL_ERROR_VAL_UNAVAILABLE;
    }
}
