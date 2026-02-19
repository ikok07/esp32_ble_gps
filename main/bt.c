//
// Created by Kok on 2/14/26.
//

#include "bt.h"

#include <sys/types.h>

#include "led.h"
#include "app_state.h"
#include "board_specific.h"
#include "gap.h"
#include "tasks_common.h"
#include "log.h"

void bt_config_task(void *arg);
void led_notify_task(void *arg);

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
            .Security = {
                .EncryptedConnection = 1,
                .IOCapability = BLE_IOCAP_DISP_ONLY,
                .ProtectionType = BLE_PROTECTION_PASSKEY
            }
        }
    };

    if (BLE_Init(gAppState.hble) != BLE_ERROR_OK) {
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to initialize BLE! Error code: %d", ble_err);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "BLE initialized!");

    // Start LED Notify task
    SCHEDULER_Create(&gAppState.Tasks->LedNotifyTask);

    // Remove the config task
    SCHEDULER_Remove(&gConfigTask);
}

void led_notify_task(void *arg) {
    QueueHandle_t *led_queue = (QueueHandle_t*)arg;
    while (1) {
        // LED event is just one so we don't care about the value
        uint8_t led_light_state;
        xQueueReceive(*led_queue, &led_light_state, portMAX_DELAY);

        if (gAppState.hble->hconn == BLE_HS_CONN_HANDLE_NONE || !gAppState.hble->NotificationsEnabled) continue;

        uint8_t conn_enc;
        BLE_ErrorTypeDef ble_err;
        if ((ble_err = BLE_CheckConnEncrypted(gAppState.hble, &conn_enc)) != BLE_ERROR_OK) {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to check if connection is encrypted! Error code: %d", ble_err);
            continue;
        }

        if (!conn_enc) continue;

        char *active_light = LED_ActiveLightLabel(led_light_state);
        struct os_mbuf *om = ble_hs_mbuf_from_flat(active_light, strlen(active_light) + 1);
        if (om == NULL) {
            LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to allocate mbuf for notification!");
            continue;
        }

        uint8_t err = 0;

        if ((err = ble_gatts_notify_custom(gAppState.hble->hconn, gBleBspChrs.LedStateChrHandle, om)) != 0) {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to send notification! %d", err);
            continue;
        };
    }

    SCHEDULER_Remove(&gAppState.Tasks->LedNotifyTask);
}

