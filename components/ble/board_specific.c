//
// Created by Kok on 2/14/26.
//

#include "board_specific.h"
#include "ble.h"
#include "log.h"
#include "app_state.h"

#define BLE_DEVICE_PASSWORD                     123456

static const ble_uuid128_t led_service_uuid = BLE_UUID128_INIT(0x40, 0xbb, 0x7c, 0xed, 0x00, 0x4b, 0x2c, 0xbb,
                                                           0x14, 0x41, 0xd8, 0x55, 0x77, 0x3c, 0xbc, 0xb7);

static const ble_uuid128_t led_chr_state_uuid = BLE_UUID128_INIT(0xd6, 0x80, 0x20, 0x84, 0x70, 0xa3, 0xae, 0x86,
                                                                  0xe1, 0x45, 0x6d, 0x24, 0xc0, 0xbc, 0xc0, 0xb1);

static const ble_uuid128_t led_chr_set_state_uuid = BLE_UUID128_INIT(0x89, 0x5a, 0x96, 0xfd, 0xa3, 0x1d, 0x4e, 0x78,
                                                                     0x9f, 0xdc, 0x55, 0x54, 0xdb, 0xc2, 0x9a, 0x64);

static const ble_uuid128_t led_chr_cycle_uuid = BLE_UUID128_INIT(0x6d, 0xca, 0xa5, 0x0b, 0x66, 0x3f, 0x4e, 0x01,
                                                                     0x80, 0x5f, 0x2a, 0xe5, 0xcb, 0x58, 0x3c, 0x6b);

static const ble_uuid16_t description_dsc_uuid = BLE_UUID16_INIT(0x2901);

#define BLE_DSC_OBJ_DESCRIPTION(AccessCB, Description)             {\
                                                                        .uuid = &description_dsc_uuid.u,\
                                                                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,\
                                                                        .access_cb = AccessCB,\
                                                                        .arg = Description\
                                                                    }\


BLE_BspChrsTypeDef gBleBspChrs;

int led_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int led_set_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int led_cycle_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

int description_dsc_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

char *led_active_light_label(uint8_t ActiveLight);

struct ble_gatt_svc_def gGattServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &led_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = &led_chr_state_uuid.u,
                    .flags = BLE_GATT_CHR_F_READ  | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                    // .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,            // Non-secure version
                    .val_handle = &gBleBspChrs.LedStateChrHandle,
                    .access_cb = led_state_access_cb,
                    .descriptors = (struct ble_gatt_dsc_def[]){
                            BLE_DSC_OBJ_DESCRIPTION(description_dsc_access_cb, "LED color"),
                        {0}
                    }
                },
            {
                    .uuid = &led_chr_set_state_uuid.u,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                    .val_handle = &gBleBspChrs.LEDSetStateChrHandle,
                    .access_cb = led_set_state_access_cb,
                    .descriptors = (struct ble_gatt_dsc_def[]){
                        BLE_DSC_OBJ_DESCRIPTION(description_dsc_access_cb, "Set LED color"),
                        {0}
                    }
                },
            {
                    .uuid = &led_chr_cycle_uuid.u,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                    .val_handle = &gBleBspChrs.LEDCycleChrHandle,
                    .access_cb = led_cycle_access_cb,
                    .descriptors = (struct ble_gatt_dsc_def[]){
                        BLE_DSC_OBJ_DESCRIPTION(description_dsc_access_cb, "LED auto color cycle enabled"),
                        {0}
                    }
            },
            {0}
        },
    },
    {0}
};

void BLE_GapEventCB(BLE_GapEventTypeDef Event, struct ble_gap_event *GapEvent, void *Arg) {
    switch (Event) {
        case BLE_GAP_EVENT_CONN_SUCCESS:
            LOGGER_LogF(LOGGER_LEVEL_INFO, "BLE Device %d connected!", GapEvent->connect.conn_handle);
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

void BLE_GattRegEventCB(BLE_GattRegisterEventTypeDef Event, struct ble_gatt_register_ctxt *EventCtxt, void *Arg) {
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

uint8_t BLE_GattSubscribeCB(struct ble_gap_event *event) {
    if (event->subscribe.attr_handle == gBleBspChrs.LedStateChrHandle) {
        uint8_t is_encrypted;
        if (BLE_CheckConnEncrypted(event->subscribe.conn_handle, &is_encrypted) != BLE_ERROR_OK || !is_encrypted) {
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }
    }
    return 0;
}

void BLE_ErrorCB(BLE_ErrorTypeDef Error) {
    LOGGER_LogF(LOGGER_LEVEL_ERROR, "An error occurred in BLE driver! Error code: %d", Error);
}

void BLE_AdvertiseSvcsCB(struct ble_hs_adv_fields *Fields) {
    Fields->uuids128 = (ble_uuid128_t[]){led_service_uuid};
    Fields->num_uuids128 = 1;
    Fields->uuids128_is_complete = 1;
}

int led_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (attr_handle == gBleBspChrs.LedStateChrHandle) {
                SHVAL_ErrorTypeDef shval_err;
                uint32_t led_light_num;
                if ((shval_err = SHVAL_GetValue(&gAppState.SharedValues->LedLightState, &led_light_num, 1000)) != SHVAL_ERROR_OK) {
                    LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared LED light state! Error code: %d", shval_err);
                    return BLE_ATT_ERR_UNLIKELY;
                }

                char *active_light = led_active_light_label(led_light_num);
                err = os_mbuf_append(ctxt->om, active_light, strlen(active_light) + 1);

                return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            break;
        default:
            break;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

int led_set_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;
    SHVAL_ErrorTypeDef shval_err;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            if (attr_handle == gBleBspChrs.LEDSetStateChrHandle) {
                if (ctxt->om->om_len != 1) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
                uint8_t write_val = *ctxt->om->om_data;
                if (write_val < 0 || write_val > 2) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;

                if ((shval_err = SHVAL_SetValue(&gAppState.SharedValues->LedLightState, write_val, 1000)) != SHVAL_ERROR_OK) {
                    LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to set shared LED light state variable! Error code: %d", shval_err);
                    return BLE_ATT_ERR_UNLIKELY;
                };

                return 0;
            }
            break;
        default:
            break;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

int led_cycle_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err;
    SHVAL_ErrorTypeDef shval_err;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (attr_handle == gBleBspChrs.LEDCycleChrHandle) {
                uint32_t cycle_enabled;
                if ((shval_err = SHVAL_GetValue(&gAppState.SharedValues->LedAutoCycleEnabled, &cycle_enabled, 1000)) != SHVAL_ERROR_OK) {
                    LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared LED cycle variable! Error code: %d", shval_err);
                    return BLE_ATT_ERR_UNLIKELY;
                };

                err = os_mbuf_append(ctxt->om, &cycle_enabled, sizeof(uint8_t));
                return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            break;
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            if (attr_handle == gBleBspChrs.LEDCycleChrHandle) {
                if (ctxt->om->om_len != 1) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
                uint8_t write_val = *ctxt->om->om_data;
                if (write_val < 0 || write_val > 1) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;

                if ((shval_err = SHVAL_SetValue(&gAppState.SharedValues->LedAutoCycleEnabled, write_val, 1000)) != SHVAL_ERROR_OK) {
                    LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to set shared LED cycle variable! Error code: %d", shval_err);
                    return BLE_ATT_ERR_UNLIKELY;
                };
                return 0;
            }
            break;
        default:
            break;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

int description_dsc_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC) return BLE_ATT_ERR_UNLIKELY;
    const char *name = (const char*)arg;
    return os_mbuf_append(ctxt->om, name, strlen(name));
}

char *led_active_light_label(uint8_t ActiveLight) {
    switch (ActiveLight) {
        case 0:
            return "Red";
        case 1:
            return "Green";
        case 2:
            return "Blue";
        default:
            return "Unknown";
    }
}