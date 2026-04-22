//
// Created by Kok on 4/22/26.
//

#include "app_state.h"
#include "bt.h"
#include "tasks_common.h"

void loc_and_spd_nfty_task(void *arg);
void elevation_nfty_task(void *arg);
void gnss_fix_qlt_task(void *arg);
void date_time_task(void *arg);

void BT_InitNotifications() {
    SCHEDULER_TaskTypeDef notify_task = {
        .Active = 0,
        .CoreID = BT_NOTIFY_TASK_CORE_ID,
        .Priority = BT_NOTIFY_TASK_PRIORITY,
        .StackDepth = BT_NOTIFY_TASK_STACK_DEPTH,
    };

    notify_task.Name = "BT Location and Speed NTFY";
    notify_task.Function = loc_and_spd_nfty_task;
    gAppState.Tasks->BTLocAndSpdNotifyTask = notify_task;

    notify_task.Name = "BT Elevation NTFY";
    notify_task.Function = elevation_nfty_task;
    gAppState.Tasks->ElevationNotifyTask = notify_task;

    notify_task.Name = "BT GNSS Fix Qlt NTFY";
    notify_task.Function = gnss_fix_qlt_task;
    gAppState.Tasks->GnssFixQltNotifyTask = notify_task;

    notify_task.Name = "BT Date Time NTFY";
    notify_task.Function = date_time_task;
    gAppState.Tasks->DateTimeNotifyTask = notify_task;

    SCHEDULER_Create(&gAppState.Tasks->BTLocAndSpdNotifyTask);
    SCHEDULER_Create(&gAppState.Tasks->ElevationNotifyTask);
    SCHEDULER_Create(&gAppState.Tasks->GnssFixQltNotifyTask);
    SCHEDULER_Create(&gAppState.Tasks->DateTimeNotifyTask);
}

void loc_and_spd_nfty_task(void *arg) {
    BT_GnssBaseDataTypeDef gnss_base_data;
    SHVAL_ErrorTypeDef shval_err;
    while (1) {
        if ((shval_err = SHVAL_PointerWaitForValue(&gAppState.SharedValues->GnssBaseData, BT_BASE_DATA_LOC_AND_SPD_EVT_BIT, &gnss_base_data, NULL, portMAX_DELAY)) == SHVAL_ERROR_OK) {
            uint8_t conn_count = sizeof(gAppState.hble->Connections) / sizeof(gAppState.hble->Connections[0]);
            uint16_t buff_len = 24;
            uint8_t buffer[buff_len];

            BT_FormatLocAndSpdBuffer(&gnss_base_data, buffer);

            BLE_ErrorTypeDef ble_err = BLE_ERROR_OK;
            if ((ble_err = BLE_SendNotification(gAppState.hble->Connections, conn_count, gBleAttributes.LocationAndSpeedChrHandle, buffer, buff_len, gAppState.hble->Config.Security.EncryptedConnection)) != BLE_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Failed to send gnss base data notification! Error code: %d", ble_err);
            }
        } else {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to wait for shared gnss base data value! Error code: %d", shval_err);
        }
    }
}

void elevation_nfty_task(void *arg) {
    BT_GnssBaseDataTypeDef gnss_base_data;
    SHVAL_ErrorTypeDef shval_err;
    while (1) {
        if ((shval_err = SHVAL_PointerWaitForValue(&gAppState.SharedValues->GnssBaseData, BT_BASE_DATA_ELEVATION_EVT_BIT, &gnss_base_data, NULL, portMAX_DELAY)) == SHVAL_ERROR_OK) {
            uint8_t conn_count = sizeof(gAppState.hble->Connections) / sizeof(gAppState.hble->Connections[0]);
            uint8_t altitude[3];
            altitude[0] = gnss_base_data.AltitudeCm & 0xFF;
            altitude[1] = (gnss_base_data.AltitudeCm >> 8) & 0xFF;
            altitude[2] = (gnss_base_data.AltitudeCm >> 16) & 0xFF;

            BLE_ErrorTypeDef ble_err = BLE_ERROR_OK;
            if ((ble_err = BLE_SendNotification(gAppState.hble->Connections, conn_count, gBleAttributes.ElevationChrHandle, altitude, 3, gAppState.hble->Config.Security.EncryptedConnection)) != BLE_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Failed to send elevation notification! Error code: %d", ble_err);
            }
        } else {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to wait for shared gnss base data value! Error code: %d", shval_err);
        }
    }
}

void gnss_fix_qlt_task(void *arg) {
    BT_GnssPrecisionDataTypeDef gnss_precision_data;
    SHVAL_ErrorTypeDef shval_err;
    while (1) {
        if ((shval_err = SHVAL_PointerWaitForValue(&gAppState.SharedValues->GnssPrecisionData, BT_PRECISION_DATA_GNSS_FIX_EVT_BIT, &gnss_precision_data, NULL, portMAX_DELAY)) == SHVAL_ERROR_OK) {
            uint8_t conn_count = sizeof(gAppState.hble->Connections) / sizeof(gAppState.hble->Connections[0]);
            uint16_t buff_len = 5;
            uint8_t buffer[buff_len];

            BT_FormatGnssFixQltBuffer(&gnss_precision_data, buffer);

            BLE_ErrorTypeDef ble_err = BLE_ERROR_OK;
            if ((ble_err = BLE_SendNotification(gAppState.hble->Connections, conn_count, gBleAttributes.GnssFixQualityChrHandle, buffer, buff_len, gAppState.hble->Config.Security.EncryptedConnection)) != BLE_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Failed to send shared precision data notification! Error code: %d", ble_err);
            }
        } else {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to wait for shared precision data value! Error code: %d", shval_err);
        }
    }
}

void date_time_task(void *arg) {
    BT_GnssBaseDataTypeDef gnss_base_data;
    SHVAL_ErrorTypeDef shval_err;
    while (1) {
        if ((shval_err = SHVAL_PointerWaitForValue(&gAppState.SharedValues->GnssBaseData, BT_BASE_DATA_LOC_AND_SPD_EVT_BIT, &gnss_base_data, NULL, portMAX_DELAY)) == SHVAL_ERROR_OK) {
            uint8_t conn_count = sizeof(gAppState.hble->Connections) / sizeof(gAppState.hble->Connections[0]);
            uint16_t buff_len = 7;
            uint8_t buffer[buff_len];

            BT_FormatDateTimeBuffer(&gnss_base_data, buffer);

            BLE_ErrorTypeDef ble_err = BLE_ERROR_OK;
            if ((ble_err = BLE_SendNotification(gAppState.hble->Connections, conn_count, gBleAttributes.DateTimeChrHandle, buffer, buff_len, gAppState.hble->Config.Security.EncryptedConnection)) != BLE_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Failed to send date time notification! Error code: %d", ble_err);
            }
        } else {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to wait for shared gnss base data value! Error code: %d", shval_err);
        }
    }
}