//
// Created by Kok on 4/21/26.
//

#include "bt.h"

#include "app_state.h"
#include "ble.h"
#include "tasks_common.h"
#include "log.h"
#include "power.h"
#include "status_led.h"

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
    // Wait for telemetry task to initialize
    while (
        !gAppState.Tasks->TelemetryParserTask.Active ||
        gAppState.Tasks->GnssConfigTask.Name == NULL            // Check if task is deleted (config completed)
    ) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    BLE_ErrorTypeDef ble_err = BLE_ERROR_OK;
    gAppState.Tasks->BleTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = BLE_TASK_CORE_ID,
        .Name = "NimBLE Task",
        .Priority = BLE_TASK_PRIORITY,
        .StackDepth = BLE_TASK_STACK_DEPTH,
        .Args = NULL,
    };

    *gAppState.hble = (BLE_HandleTypeDef){
        .BLE_Task = &gAppState.Tasks->BleTask,
        .Config = {
            .DeviceName = "GNSS Receiver",
            .GapAppearance = 0x1444,             // Location and Navigation Pod
            .AdvertisingIntervalMS = 50,
            .GapRole = BLE_GAP_ROLE_PERIPHERAL,
            .PrivateAddressEnabled = 0,
            .NonResolvablePrivateAddress = 0,
            .MaxConnections = 1,
            .DiscoverabilityMode = BLE_DISC_MODE_ALLOW_ALL,
            .ConnectionMode = BLE_CONN_MODE_ALLOW_ALL,
            .ManufacturerData = {
                .ManufacturerName = BT_MANUFACTURER_NAME,
                .ModelNumber = BT_MODEL_NAME,
                .FirmwareRevision = BT_FIRMWARE_REVISION,
            },
            .GapParams = {
                .Activate = 1,
                .Latency = 0,
                .MinimumIntervalMs = 45,
                .MaximumIntervalMs = 70,
                .MinConnEventLengthMs = 0,
                .MaxConnEventLengthMs = 0,
                .SupervisionTimeoutMs = 4000,
            },
            .Security = {
                .EncryptedConnection = 1,
                .IOCapability = BLE_IOCAP_DISP_ONLY,
                .ProtectionType = BLE_PROTECTION_PASSKEY
            },
        }
    };

    // Configure platform-specific options
    BT_Configure(gAppState.hble);
    BT_InitNotifications();

    if ((ble_err = BLE_Init(gAppState.hble)) != BLE_ERROR_OK) {
        STATUSLED_SetState(STATUSLED_STATE_ERROR_BT_CFG);
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to initialize BLE! Error code: %d", ble_err);
        POWER_WaitAndRestart(3000);
    } else {
        STATUSLED_SetState(STATUSLED_STATE_READY_TO_CONNECT);
        LOGGER_Log(LOGGER_LEVEL_INFO, "BLE initialized!");
    };

    SCHEDULER_Remove(&gConfigTask);
}