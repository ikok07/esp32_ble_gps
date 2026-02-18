//
// Created by Kok on 2/14/26.
//

#include "board_specific.h"
#include "ble.h"
#include "log.h"
#include "app_state.h"

static const ble_uuid128_t led_service_uuid = BLE_UUID128_INIT(0x40, 0xbb, 0x7c, 0xed, 0x00, 0x4b, 0x2c, 0xbb,
                                                           0x14, 0x41, 0xd8, 0x55, 0x77, 0x3c, 0xbc, 0xb8);

static const ble_uuid128_t led_chr_state_uuid = BLE_UUID128_INIT(0xd6, 0x80, 0x20, 0x84, 0x70, 0xa3, 0xae, 0x86,
                                                                  0xe1, 0x45, 0x6d, 0x24, 0xc0, 0xbc, 0xc0, 0xb2);

BLE_BspChrsTypeDef gBleBspChrs;

int led_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

char *led_active_light_label(uint8_t ActiveLight);

struct ble_gatt_svc_def gGattServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &led_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = &led_chr_state_uuid.u,
                    .flags = BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &gBleBspChrs.LedStateChrHandle,
                    .access_cb = led_state_access_cb
                },
            {0}
        }
    },
    {0}
};

void BLE_GapEventCB(BLE_GapEventTypeDef Event, struct ble_gap_event *GapEvent, void *Arg) {
    switch (Event) {
        case BLE_GAP_EVENT_CONN_SUCCESS:
            LOGGER_Log(LOGGER_LEVEL_INFO, "BLE Device connected!");
            break;
        case BLE_GAP_EVENT_CONN_FAILED:
            LOGGER_Log(LOGGER_LEVEL_INFO, "BLE Device connection failed!");
            break;
        case BLE_GAP_EVENT_CONN_UPD:
            LOGGER_Log(LOGGER_LEVEL_INFO, "BLE Device connection updated!");
            break;
        case BLE_GAP_EVENT_CONN_DISCONNECT:
            LOGGER_Log(LOGGER_LEVEL_INFO, "BLE Device disconnected!");
            break;
        case BLE_GAP_EVENT_SUB:
            LOGGER_Log(LOGGER_LEVEL_INFO, "BLE Device subscribed!");
            break;
        case BLE_GAP_EVENT_UNSUB:
            LOGGER_Log(LOGGER_LEVEL_INFO, "BLE Device unsubscribed!");
            break;
        default:
            LOGGER_Log(LOGGER_LEVEL_INFO, "Unhandled event!");
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
        default:
            break;
    }
}

uint8_t BLE_GattSubscribeCB(struct ble_gap_event *event) {
    if (event->subscribe.attr_handle == gBleBspChrs.LedStateChrHandle) {
        uint8_t is_encrypted;
        if (BLE_CheckConnEncrypted(gAppState.hble, &is_encrypted) != BLE_ERROR_OK || !is_encrypted) {
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
        default:
            break;
    }

    return BLE_ATT_ERR_UNLIKELY;
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