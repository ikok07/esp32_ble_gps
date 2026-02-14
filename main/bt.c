//
// Created by Kok on 2/14/26.
//

#include "bt.h"
#include "app_state.h"
#include "tasks_common.h"
#include "log.h"

void bt_config_task(void *arg);

void BT_Init() {
    SCHEDULER_TaskTypeDef config_task = {
        .CoreID = BT_CFG_TASK_CORE_ID,
        .Name = "BT Config Task",
        .Priority = BT_CFG_TASK_PRIORITY,
        .StackDepth = BT_CFG_TASK_STACK_DEPTH,
        .Args = NULL,
        .Function = bt_config_task
    };
    SCHEDULER_Create(&config_task);
}

void bt_config_task(void *arg) {
    gAppState.Tasks->BleTask = (SCHEDULER_TaskTypeDef){
        .CoreID = BLE_TASK_CORE_ID,
        .Name = "NimBLE Task",
        .Priority = BLE_TASK_PRIORITY,
        .StackDepth = BLE_TASK_STACK_DEPTH,
        .Args = NULL,
    };

    *gAppState.hble = (BLE_HandleTypeDef){
        .BLE_Task = &gAppState.Tasks->BleTask,
        .Config = {
            .DeviceName = "LED Sensor",
            .GapAppearance = 0x02C0, // Sensor appearance
            .AdvertisingIntervalMS = 500,
            .GapRole = BLE_GAP_ROLE_PERIPHERAL,
            .PrivateAddressEnabled = 0
        }
    };

    if (BLE_Init(gAppState.hble) != BLE_ERROR_OK) {
        LOGGER_Log(LOGGER_LEVEL_FATAL, "Failed to initialize BLE!");
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "BLE initialized!");

    while (!BLE_CanAdvertise()) {}

    LOGGER_Log(LOGGER_LEVEL_INFO, "BLE can advertise!");

    if (BLE_StartAdvertising() != BLE_ERROR_OK) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to start advertising!");
        while (1);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "BLE started advertising!");

    vTaskSuspend(NULL);
}

