//
// Created by Kok on 4/21/26.
//

#include "bt.h"

#include "app_state.h"
#include "log.h"
#include "status_led.h"

#define BLE_DEVICE_PASSWORD                                             123456

BT_AttributesTypeDef gBleAttributes;

static const ble_uuid16_t location_and_navigation_service_uuid = BLE_UUID16_INIT(0x1819);
static const ble_uuid16_t current_time_service_uuid = BLE_UUID16_INIT(0x1805);

static const ble_uuid16_t location_and_speed_char_uuid = BLE_UUID16_INIT(0x2A67);
// static const ble_uuid16_t nav_data_char_uuid = BLE_UUID16_INIT(0x2A68);
static const ble_uuid16_t elevation_char_uuid = BLE_UUID16_INIT(0x2A6C);
static const ble_uuid16_t gnss_fix_quality_char_uuid = BLE_UUID16_INIT(0x2A69);
static const ble_uuid16_t ln_feature_char_uuid = BLE_UUID16_INIT(0x2A6A);
static const ble_uuid16_t date_time_char_uuid = BLE_UUID16_INIT(0x2A08);

#ifdef DEBUG_MODE_ENABLED
static const ble_uuid128_t location_and_navigation_human_service_uuid = BLE_UUID128_INIT(
    0x88, 0xc8, 0x5b, 0xc1, 0xfe, 0xec, 0x4c, 0x35,
    0xaa, 0x49, 0x74, 0xae, 0x36, 0x35, 0x51, 0x65
);

static const ble_uuid128_t location_and_speed_human_char_uuid = BLE_UUID128_INIT(
    0x61, 0x7C, 0xF2, 0x0C, 0x20, 0x32, 0x48, 0x54,
    0xB2, 0xCE, 0xCE, 0x2C, 0xB7, 0xA2, 0xB3, 0x03
);
#endif


/* ------ Driver CBs ------ */

void on_gap_event(BLE_GapEventTypeDef Event, struct ble_gap_event *GapEvent, void *Arg);
void on_gatt_reg_event(BLE_GattRegisterEventTypeDef Event, struct ble_gatt_register_ctxt *EventCtxt, void *Arg);
uint8_t on_gatt_subscribe_event(struct ble_gap_event *event);
void on_error(BLE_ErrorTypeDef Error);

static struct ble_gatt_svc_def gGattServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &location_and_navigation_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
                    {
                        .uuid = &ln_feature_char_uuid.u,
                        .flags = BLE_GATT_CHR_F_READ,
                        .val_handle = &gBleAttributes.LNFeatureChrHandle,
                        .access_cb = BT_LNFeaturesAccessCB,
                    },
                    {
                        .uuid = &location_and_speed_char_uuid.u,
                        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                        .val_handle = &gBleAttributes.LocationAndSpeedChrHandle,
                        .access_cb = BT_LocAndSpdAccessCB,
                    },
                    // {
                    //     .uuid = &nav_data_char_uuid.u,
                    //     .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                    //     .val_handle = &gBleAttributes.NavDataChrHandle,
                    //     .access_cb = nav_data_access_cb,
                    // },
                    {
                        .uuid = &elevation_char_uuid.u,
                        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                        .val_handle = &gBleAttributes.ElevationChrHandle,
                        .access_cb = BT_AltitudeAccessCB,
                    },
                    {
                        .uuid = &gnss_fix_quality_char_uuid.u,
                        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                        .val_handle = &gBleAttributes.GnssFixQualityChrHandle,
                        .access_cb = BT_GnssFixQltAccessCB,
                    },
                {0}
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &current_time_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
                            {
                                .uuid = &date_time_char_uuid.u,
                                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                                .val_handle = &gBleAttributes.DateTimeChrHandle,
                                .access_cb = BT_DateTimeAccessCB,
                            },
                        {0}
        },
    },
#ifdef DEBUG_MODE_ENABLED
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &location_and_navigation_human_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = &location_and_speed_human_char_uuid.u,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &gBleAttributes.LocationAndSpeedHumanChrHandle,
                    .access_cb = BT_LocAndSpdHumanAccessCB,
                }
        }
#endif
    },
    {0}
};

void BT_Configure(BLE_HandleTypeDef *hble) {
    hble->Callbacks = (BLE_CallbacksTypeDef) {
        .on_gap_event = on_gap_event,
        .on_gatt_reg_event = on_gatt_reg_event,
        .on_gatt_subscribe_event = on_gatt_subscribe_event,
        .on_error = on_error
    };

    hble->Config.GattServices = gGattServices;
}

void on_gap_event(BLE_GapEventTypeDef Event, struct ble_gap_event *GapEvent, void *Arg) {
    switch (Event) {
        case BLE_GAP_EVENT_CONN_SUCCESS:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "BLE Device %d connected!", GapEvent->connect.conn_handle);
            STATUSLED_SetState(STATUSLED_STATE_CONNECTED);
            break;
        case BLE_GAP_EVENT_CONN_FAILED:
            LOGGER_Log(LOGGER_LEVEL_INFO, "BLE Device connection failed!");
            break;
        case BLE_GAP_EVENT_CONN_STORE_FAILED:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "BLE Device %d connection store failed!", GapEvent->connect.conn_handle);
            break;
        case BLE_GAP_EVENT_CONN_UPD:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "BLE Device %d connection updated!", GapEvent->conn_update.conn_handle);
            break;
        case BLE_GAP_EVENT_CONN_DISCONNECT:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "BLE Device %d disconnected!", GapEvent->disconnect.conn.conn_handle);
            if (!BLE_CheckConnectionsAvailable(gAppState.hble)) STATUSLED_SetState(STATUSLED_STATE_READY_TO_CONNECT);
            break;
        case BLE_GAP_EVENT_SUB:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "BLE Device %d subscribed!", GapEvent->subscribe.conn_handle);
            break;
        case BLE_GAP_EVENT_CONN_ENC:
            LOGGER_Log(LOGGER_LEVEL_INFO, "BLE Connection encrypted");
            break;
        case BLE_GAP_EVENT_CONN_ENC_FAILED:
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "BLE Connection could not be encrypted! Status code: %d", GapEvent->enc_change.status);
            break;
        case BLE_GAP_EVENT_UNSUB:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "BLE Device %d unsubscribed!", GapEvent->subscribe.conn_handle);
            break;
        case BLE_GAP_EVENT_PASSKEY:
            if (GapEvent->passkey.params.action == BLE_SM_IOACT_DISP) {
                struct ble_sm_io pkey= {0};
                pkey.action = GapEvent->passkey.params.action;
                pkey.passkey = BLE_DEVICE_PASSWORD;
                ble_sm_inject_io(GapEvent->passkey.conn_handle, &pkey);
            }
            break;
        case BLE_GAP_EVENT_CONN_UPD_FAILED:
            LOGGER_LogF(LOGGER_LEVEL_WARNING, "BLE Device %d connection update failed! Status code: %d", GapEvent->conn_update.conn_handle, GapEvent->conn_update.status);
            break;
        case BLE_GAP_EVENT_CONN_UPD_REQ_FAILED:
            LOGGER_LogF(LOGGER_LEVEL_WARNING, "BLE Device %d connection update request failed! Status code: %d", GapEvent->conn_update.conn_handle, GapEvent->conn_update.status);
            break;
        default:
            LOGGER_LogF(LOGGER_LEVEL_WARNING, "Unhandled GAP event %d!", Event);
            break;
    }
}

void on_gatt_reg_event(BLE_GattRegisterEventTypeDef Event, struct ble_gatt_register_ctxt *EventCtxt, void *Arg) {
    switch (Event) {
        case BLE_GATT_REG_EVENT_REG_SVC:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "New service registered! Handle: 0x%04X", EventCtxt->svc.handle);
            break;
        case BLE_GATT_REG_EVENT_REG_CHR:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "New service characteristic registered! Handle: 0x%04X", EventCtxt->svc.handle);
            break;
        case BLE_GATT_REG_EVENT_REG_DSC:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "New characteristic descriptor registered! Handle: 0x%04X", EventCtxt->svc.handle);
            break;
        default:
            break;
    }
}

uint8_t on_gatt_subscribe_event(struct ble_gap_event *event) {
    if (event->subscribe.attr_handle == gBleAttributes.LocationAndSpeedChrHandle) {
        uint8_t is_encrypted;
        if (BLE_CheckConnEncrypted(event->subscribe.conn_handle, &is_encrypted) != BLE_ERROR_OK || !is_encrypted) {
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }
    }
    return 0;
}

void on_error(BLE_ErrorTypeDef Error) {
    LOGGER_LogF(LOGGER_LEVEL_ERROR, "An error occurred in BLE driver! Error code: %d", Error);
}