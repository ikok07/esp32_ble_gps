//
// Created by Kok on 2/13/26.
//

#ifndef ESP32S3_BLE_BLE_H
#define ESP32S3_BLE_BLE_H

#include "task_scheduler.h"

#define __weak __attribute__((weak))

typedef enum {
    BLE_ERROR_OK,
    BLE_ERROR_INVALID_STATE,
    BLE_ERROR_MISSING_HANDLE,
    BLE_ERROR_NVS,
    BLE_ERROR_INIT,
    BLE_ERROR_GAP_NAME,
    BLE_ERROR_GAP_APPEARANCE,
    BLE_ERROR_ADV_ADDR,
    BLE_ERROR_ADV_ADDR_CALC,
    BLE_ERROR_ADV_ADDR_COPY,
    BLE_ERROR_ADV_FIELDS,
    BLE_ERROR_RSP_FIELDS,
    BLE_ERROR_ADV_START
} BLE_ErrorTypeDef;

typedef enum {
    BLE_GAP_ROLE_PERIPHERAL,
    BLE_GAP_ROLE_CENTRAL,
} BLE_GapRoleTypeDef;

typedef enum {
    BLE_STATE_INACTIVE,
    BLE_STATE_READY_FOR_ADV
} BLE_StateTypedDef;

typedef struct {
    char *DeviceName;
    uint16_t GapAppearance;
    uint8_t PrivateAddressEnabled;
    BLE_GapRoleTypeDef GapRole;
    uint16_t AdvertisingIntervalMS;
} BLE_ConfigTypeDef;

typedef struct {
    SCHEDULER_TaskTypeDef *BLE_Task;
    BLE_ConfigTypeDef Config;
    BLE_StateTypedDef State;
    uint8_t AddressType;
    uint8_t Address[6];
    char AddressStr[20];
} BLE_HandleTypeDef;

BLE_ErrorTypeDef BLE_Init(BLE_HandleTypeDef *hble);

uint8_t BLE_CanAdvertise();
BLE_ErrorTypeDef BLE_StartAdvertising();

void BLE_StackResetCB(int reason);

#endif //ESP32S3_BLE_BLE_H