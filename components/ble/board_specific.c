//
// Created by Kok on 2/14/26.
//

#include "board_specific.h"
#include "ble.h"
#include "log.h"

static const ble_uuid128_t led_service_uuid = BLE_UUID128_INIT(0x40, 0xbb, 0x7c, 0xed, 0x00, 0x4b, 0x2c, 0xbb,
                                                           0x14, 0x41, 0xd8, 0x55, 0x77, 0x3c, 0xbc, 0xb9);

static const ble_uuid128_t led_chr_state_uuid = BLE_UUID128_INIT(0xd6, 0x80, 0x20, 0x84, 0x70, 0xa3, 0xae, 0x86,
                                                                  0xe1, 0x45, 0x6d, 0x24, 0xc0, 0xbc, 0xc0, 0xb1);

static uint16_t led_chr_state_handle;

int led_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

struct ble_gatt_svc_def gGattServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &led_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    .uuid = &led_chr_state_uuid.u,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &led_chr_state_handle,
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
        default:
            LOGGER_Log(LOGGER_LEVEL_INFO, "Unhandled event!");
            break;
    }
}

int led_state_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {

    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (attr_handle == led_chr_state_handle) {
                err = os_mbuf_append(ctxt->om, "Green", strlen("Green") + 1);
                return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            break;
        default:
            break;
    }

    return BLE_ATT_ERR_UNLIKELY;
}