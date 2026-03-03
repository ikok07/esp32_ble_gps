//
// Created by Kok on 2/14/26.
//

#include "bt.h"

#include <sys/types.h>

#include "app_state.h"
#include "tasks_common.h"
#include "log.h"

void bt_config_task(void *arg);

static SCHEDULER_TaskTypeDef gConfigTask = {
    .Active = 0,
    .CoreID = BT_CFG_TASK_CORE_ID,
    .Name = "BT Config Task",
    .Priority = BT_CFG_TASK_PRIORITY,
    .StackDepth = BT_CFG_TASK_STACK_DEPTH,
    .Args = NULL,
    .Function = bt_config_task
};

void BT_Init() {
    SCHEDULER_Create(&gConfigTask);
}

void bt_config_task(void *arg) {

    // Wait for LED Task to be initialized
    while (!gAppState.Tasks->LedTask.Active) {};

    BLE_ErrorTypeDef ble_err = BLE_ERROR_OK;
    gAppState.Tasks->BleTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = BLE_TASK_CORE_ID,
        .Name = "NimBLE Task",
        .Priority = BLE_TASK_PRIORITY,
        .StackDepth = BLE_TASK_STACK_DEPTH,
        .Args = NULL,
    };

    gAppState.Tasks->LedNotifyTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = LED_NOTIFY_TASK_CORE_ID,
        .Name = "LED Notify Task",
        .Priority = LED_NOTIFY_TASK_PRIORITY,
        .StackDepth = LED_NOTIFY_TASK_STACK_DEPTH,
        .Args = &gAppState.SharedValues->LedLightState.SubscribersQueue,
        .Function = led_notify_task
    };

    *gAppState.hble = (BLE_HandleTypeDef){
        .BLE_Task = &gAppState.Tasks->BleTask,
        .Config = {
            .DeviceName = "LED Sensor",
            .GapAppearance = 0x02C0, // Sensor appearance
            .AdvertisingIntervalMS = 50,
            .GapRole = BLE_GAP_ROLE_PERIPHERAL,
            .PrivateAddressEnabled = 0,
            .MaxConnections = 1,
            .Security = {
                .EncryptedConnection = 1,
                .IOCapability = BLE_IOCAP_DISP_ONLY,
                .ProtectionType = BLE_PROTECTION_PASSKEY
            },
            .ManufacturerData = {
                .ManufacturerName = "LED Industries LTD.",
                .SerialNumber = "LS001"
            }
        }
    };

    if ((ble_err = BLE_Init(gAppState.hble)) != BLE_ERROR_OK) {
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to initialize BLE! Error code: %d", ble_err);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "BLE initialized!");

    // Start LED Notify task
    SCHEDULER_Create(&gAppState.Tasks->LedNotifyTask);

    // Remove the config task
    SCHEDULER_Remove(&gConfigTask);
}

