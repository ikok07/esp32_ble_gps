//
// Created by Kok on 4/21/26.
//

#include "bt-config.h"

#include "app_state.h"
#include "bt-chars.h"
#include "log.h"
#include "shared_values.h"

#define BLE_DEVICE_PASSWORD                                             123456

BLE_AttributesTypeDef gBleAttributes;

static const ble_uuid16_t location_and_navigation_service_uuid = BLE_UUID16_INIT(0x1819);
static const ble_uuid16_t current_time_service_uuid = BLE_UUID16_INIT(0x1805);

static const ble_uuid16_t location_and_speed_char_uuid = BLE_UUID16_INIT(0x2A67);
// static const ble_uuid16_t nav_data_char_uuid = BLE_UUID16_INIT(0x2A68);
// static const ble_uuid16_t altitude_char_uuid = BLE_UUID16_INIT(0x2A6C);
// static const ble_uuid16_t gnss_fix_quality_char_uuid = BLE_UUID16_INIT(0x2A69);
static const ble_uuid16_t ln_feature_char_uuid = BLE_UUID16_INIT(0x2A6A);
// static const ble_uuid16_t date_time_char_uuid = BLE_UUID16_INIT(0x2A08);

/* ------ Driver CBs ------ */

void on_gap_event(BLE_GapEventTypeDef Event, struct ble_gap_event *GapEvent, void *Arg);
void on_gatt_reg_event(BLE_GattRegisterEventTypeDef Event, struct ble_gatt_register_ctxt *EventCtxt, void *Arg);
uint8_t on_gatt_subscribe_event(struct ble_gap_event *event);
void on_error(BLE_ErrorTypeDef Error);

/* ------ Services Access CBs ------ */

int ln_features_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

int location_and_speed_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
//
// int nav_data_access_cb(uint16_t conn_handle, uint16_t attr_handle,
//                           struct ble_gatt_access_ctxt *ctxt, void *arg);
//
// int gnss_fix_quality_access_cb(uint16_t conn_handle, uint16_t attr_handle,
//                           struct ble_gatt_access_ctxt *ctxt, void *arg);
//
// int altitude_access_cb(uint16_t conn_handle, uint16_t attr_handle,
//                           struct ble_gatt_access_ctxt *ctxt, void *arg);
//
// int date_time_access_cb(uint16_t conn_handle, uint16_t attr_handle,
//                           struct ble_gatt_access_ctxt *ctxt, void *arg);

int description_dsc_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);

static struct ble_gatt_svc_def gGattServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &location_and_navigation_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
                    {
                        .uuid = &ln_feature_char_uuid.u,
                        .flags = BLE_GATT_CHR_F_READ,
                        .val_handle = &gBleAttributes.LNFeatureChrHandle,
                        .access_cb = ln_features_access_cb,
                    },
                    {
                        .uuid = &location_and_speed_char_uuid.u,
                        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                        .val_handle = &gBleAttributes.LocationAndSpeedChrHandle,
                        .access_cb = location_and_speed_access_cb,
                    },
                    // {
                    //     .uuid = &nav_data_char_uuid.u,
                    //     .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                    //     .val_handle = &gBleAttributes.NavDataChrHandle,
                    //     .access_cb = nav_data_access_cb,
                    // },
                    // {
                    //     .uuid = &altitude_char_uuid.u,
                    //     .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                    //     .val_handle = &gBleAttributes.NavDataChrHandle,
                    //     .access_cb = altitude_access_cb,
                    // },
                    // {
                    //     .uuid = &gnss_fix_quality_char_uuid.u,
                    //     .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                    //     .val_handle = &gBleAttributes.GnssFixQualityChrHandle,
                    //     .access_cb = gnss_fix_quality_access_cb,
                    // },
                {0}
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &current_time_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
                            // {
                            //     .uuid = &date_time_char_uuid.u,
                            //     .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                            //     .val_handle = &gBleAttributes.LocationAndSpeedChrHandle,
                            //     .access_cb = date_time_access_cb,
                            // },
                        {0}
        },
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
            // STATUSLED_SetState(STATUSLED_STATE_CONNECTED);
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
            // if (!BLE_CheckConnectionsAvailable(gAppState.hble)) STATUSLED_SetState(STATUSLED_STATE_READY_TO_CONNECT);
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

int ln_features_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            uint32_t supported_features = 0;
            supported_features |= (1 << 2);             // Location
            supported_features |= (1 << 3);             // Elevation
            supported_features |= (1 << 4);             // Heading
            supported_features |= (1 << 15);            // HDOP

            err = os_mbuf_append(ctxt->om, &supported_features, 1);

            return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            break;
        default:
            break;
    }

    return err;
}

int location_and_speed_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:

            SHVAL_ErrorTypeDef shval_err;
            BTCHAR_LocationAndSpeedDataTypeDef loc_and_spd_data;
            uint32_t data_len;
            if ((shval_err = SHVAL_PointerGetValue(&gAppState.SharedValues->LocationAndSpeedData, &loc_and_spd_data, &data_len, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared location and speed data! Error code: %d", shval_err);
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            uint16_t flags = 0;
            flags |= (1 << 0);                                                              // Speed
            flags |= (1 << 2);                                                              // Location
            flags |= (1 << 3);                                                              // Altitude
            flags |= (1 << 4);                                                              // Heading
            flags |= (1 << 6);                                                              // UTC Time
            flags &=~ (1 << 9);                                                             // 2D speed
            flags &=~ (0x03 << 10);                                                         // Elevation source: positioning system
            flags |= (1 << 12);                                                             // Heading based on magnetic compass

            uint8_t status = (loc_and_spd_data.GnssFix == M10_DEV_STATUS_READY) ? 0x01 : 0x00;
            flags |= (status << 7);                                                         // Position status

            uint8_t buffer[29];
            uint8_t offset = 0;

            // Flags
            memcpy(&buffer[offset], &flags, 2);
            offset += 2;

            // Speed
            memcpy(&buffer[offset], &loc_and_spd_data.Velocity, 4);
            offset += 4;

            // Latitude
            memcpy(&buffer[offset], &loc_and_spd_data.Latitude, 4);
            offset += 4;

            // Longitude
            memcpy(&buffer[offset], &loc_and_spd_data.Longitude, 4);
            offset += 4;

            // Altitude
            buffer[offset++] = (uint8_t)(loc_and_spd_data.Altitude & 0xFF);
            buffer[offset++] = (uint8_t)((loc_and_spd_data.Altitude >> 8) & 0xFF);
            buffer[offset++] = (uint8_t)((loc_and_spd_data.Altitude >> 16) & 0xFF);

            // Heading
            memcpy(&buffer[offset], &loc_and_spd_data.Heading, 2); offset += 2;

            // Date time
            memcpy(&buffer[offset], &loc_and_spd_data.DateTime.Year, 2); offset += 2;
            memcpy(&buffer[offset++], &loc_and_spd_data.DateTime.Month, 1);
            memcpy(&buffer[offset++], &loc_and_spd_data.DateTime.Day, 1);
            memcpy(&buffer[offset++], &loc_and_spd_data.DateTime.Hours, 1);
            memcpy(&buffer[offset++], &loc_and_spd_data.DateTime.Minutes, 1);
            memcpy(&buffer[offset], &loc_and_spd_data.DateTime.Seconds, 1);

            err = os_mbuf_append(ctxt->om, &buffer, sizeof(buffer) / sizeof(buffer[0]));

            return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            break;
        default:
            break;
    }

    return err;
}

int description_dsc_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC) return BLE_ATT_ERR_UNLIKELY;
    const char *name = (const char*)arg;
    return os_mbuf_append(ctxt->om, name, strlen(name));
}