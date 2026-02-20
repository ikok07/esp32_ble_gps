//
// Created by Kok on 2/14/26.
//

#include "ble.h"
#include "gap.h"

#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "host/util/util.h"

BLE_ErrorTypeDef start_adv(BLE_HandleTypeDef *hble);
BLE_ErrorTypeDef conn_add(BLE_HandleTypeDef *hble, uint16_t hconn);
void conn_remove(BLE_HandleTypeDef *hble, uint16_t hconn);
BLE_ErrorTypeDef conn_toggle_notifications(BLE_HandleTypeDef *hble, uint16_t hconn, uint8_t Enabled);
uint8_t conn_get_space_avail(BLE_HandleTypeDef *hble);
void format_addr(char *AddrStr, uint8_t Len, uint8_t Address[]);
BLE_ErrorTypeDef set_random_addr();

int on_gap_event(struct ble_gap_event *event, void *arg) {
    uint8_t err = 0;
    struct ble_gap_conn_desc desc;
    BLE_HandleTypeDef *hble = (BLE_HandleTypeDef*)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                if ((err = ble_gap_conn_find(event->connect.conn_handle, &desc)) != 0) {
                    BLE_GapEventCB(BLE_GAP_EVENT_CONN_FAILED, event, NULL);
                    return err;
                }

                if (conn_add(hble, event->connect.conn_handle) != BLE_ERROR_OK) {
                    BLE_GapEventCB(BLE_GAP_EVENT_CONN_STORE_FAILED, event, NULL);
                    return BLE_HS_EUNKNOWN;
                };

                BLE_GapEventCB(BLE_GAP_EVENT_CONN_SUCCESS, event, &desc);
            } else {
                // Connection failed, restart advertising
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_FAILED, event,  NULL);
            }
            if (conn_get_space_avail(hble)) {
                gap_start_adv(hble);
            };
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            BLE_GapEventCB(BLE_GAP_EVENT_CONN_DISCONNECT, event,  NULL);
            conn_remove(hble, event->disconnect.conn.conn_handle);
            if (conn_get_space_avail(hble)) {
                gap_start_adv(hble);
            };
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:
            if ((err = ble_gap_conn_find(event->connect.conn_handle, &desc)) != 0) {
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_FAILED, event,  NULL);
                return err;
            }
            BLE_GapEventCB(BLE_GAP_EVENT_CONN_UPD, event,  &desc);
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (conn_toggle_notifications(hble, event->subscribe.conn_handle, event->subscribe.cur_notify) != BLE_ERROR_OK) {
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_STORE_NOTIFY_TOGGLE_FAILED, event,  NULL);
                return BLE_HS_EUNKNOWN;
            }

            if (event->subscribe.cur_notify || event->subscribe.cur_indicate) {
                if (!BLE_GattSubscribeCB(event)) {
                    // Attribute requires encryption
                    if ((err = ble_gap_security_initiate(event->subscribe.conn_handle)) != 0) {
                        BLE_GapEventCB(BLE_GAP_EVENT_CONN_ENC_FAILED, event,  NULL);
                        return err;
                    }
                }
                BLE_GapEventCB(BLE_GAP_EVENT_SUB, event, NULL);
            } else {
                BLE_GapEventCB(BLE_GAP_EVENT_UNSUB, event, NULL);
            }
            break;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            BLE_GapEventCB(BLE_GAP_EVENT_PASSKEY, event, NULL);
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_ENC, event, NULL);
            } else {
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_ENC_FAILED, event, NULL);
            }
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            uint8_t err = 0;
            if ((err = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc))) {
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_FAILED, event, NULL);
                return err;
            }
            ble_store_util_delete_peer(&desc.peer_id_addr);

            return BLE_GAP_REPEAT_PAIRING_RETRY;
        default:
            break;
    }

    return err;
}

BLE_ErrorTypeDef gap_init(BLE_HandleTypeDef *hble) {
    if (hble == NULL) return BLE_ERROR_MISSING_HANDLE;
    uint8_t err = 0;

    // Initialize GAP service
    ble_svc_gap_init();

    // Set device name
    if ((err = ble_svc_gap_device_name_set(hble->Config.DeviceName)) != 0) return BLE_ERROR_GAP_NAME;

    // Set GAP appearance
    if ((err = ble_svc_gap_device_appearance_set(hble->Config.GapAppearance)) != 0) return BLE_ERROR_GAP_APPEARANCE;

    return BLE_ERROR_OK;
}

BLE_ErrorTypeDef start_adv(BLE_HandleTypeDef *hble) {
    if (hble == NULL) return BLE_ERROR_MISSING_HANDLE;
    uint8_t err = 0;

    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    /* ------ Advertising fields ------ */

    // Set advertising flags (Always discoverable and only BLE Supported)
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Set device name
    const char *name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t*)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    // Set device TX power
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;

    // Set device appearance
    adv_fields.appearance = hble->Config.GapAppearance;
    adv_fields.appearance_is_present = 1;

    // Set device role
    adv_fields.le_role = hble->Config.GapRole;
    adv_fields.le_role_is_present = 1;

    // Set advertisement fields
    if ((err = ble_gap_adv_set_fields(&adv_fields)) != 0) return BLE_ERROR_ADV_FIELDS;

    /* ------ Response fields ------ */

    // Set device address
    rsp_fields.device_addr = hble->Address;
    rsp_fields.device_addr_type = hble->AddressType;
    rsp_fields.device_addr_is_present = 1;

    // Set advertising interval
    rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(hble->Config.AdvertisingIntervalMS);
    rsp_fields.adv_itvl_is_present = 1;

    // Call callback (used to set the advertised services)
    BLE_AdvertiseSvcsCB(&rsp_fields);

    // Set response fields
    if ((err = ble_gap_adv_rsp_set_fields(&rsp_fields)) != 0) return BLE_ERROR_RSP_FIELDS;

    /* ------ Response params ------ */

    // Make device discoverable and connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;

    // Set advertising interval
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(hble->Config.AdvertisingIntervalMS);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(hble->Config.AdvertisingIntervalMS + 20);

    // Start advertising
    if ((err = ble_gap_adv_start(hble->AddressType, NULL, BLE_HS_FOREVER, &adv_params, on_gap_event, hble))) return BLE_ERROR_ADV_START;

    return BLE_ERROR_OK;
}

BLE_ErrorTypeDef conn_add(BLE_HandleTypeDef *hble, uint16_t hconn) {
    uint8_t conn_arr_size = sizeof(hble->Connections) / sizeof(hble->Connections[0]);

    for (int i = 0; i < conn_arr_size; i++) {
        BLE_ConnTypeDef *conn = &(hble->Connections[i]);
        if (conn->Active) continue;

        conn->Active = 1;
        conn->hconn = hconn;

        return BLE_ERROR_OK;
    }

    return BLE_ERROR_GAP_CONN_FULL;
}

void conn_remove(BLE_HandleTypeDef *hble, uint16_t hconn) {
    uint8_t conn_arr_size = sizeof(hble->Connections) / sizeof(hble->Connections[0]);
    for (int i = 0; i < conn_arr_size; i++) {
        BLE_ConnTypeDef *conn = &(hble->Connections[i]);
        if (conn->hconn == hconn) *conn = (BLE_ConnTypeDef){0};
    }
}

BLE_ErrorTypeDef conn_toggle_notifications(BLE_HandleTypeDef *hble, uint16_t hconn, uint8_t Enabled) {
    uint8_t conn_arr_size = sizeof(hble->Connections) / sizeof(hble->Connections[0]);

    for (int i = 0; i < conn_arr_size; i++) {
        BLE_ConnTypeDef *conn = &(hble->Connections[i]);
        if (conn->hconn != hconn) continue;

        conn->NotificationsEnabled = Enabled;

        return BLE_ERROR_OK;
    }

    return BLE_ERROR_GAP_CONN_NOT_FOUND;
}

uint8_t conn_get_space_avail(BLE_HandleTypeDef *hble) {
    uint8_t count = 0;
    uint8_t conn_arr_size = sizeof(hble->Connections) / sizeof(hble->Connections[0]);

    for (int i = 0; i < conn_arr_size; i++) {
        count += hble->Connections[i].Active;
    }

    return count < hble->Config.MaxConnections;
}

void format_addr(char *AddrStr, uint8_t Len, uint8_t Address[]) {
    snprintf(AddrStr, Len, "%02X:%02X:%02X:%02X:%02X:%02X:", Address[0], Address[1], Address[2], Address[3], Address[4], Address[5]);
}

BLE_ErrorTypeDef set_random_addr() {
    ble_addr_t addr;

    // Generate random address
    if (ble_hs_id_gen_rnd(0, &addr) != 0) return BLE_ERROR_GAP_ADDRESS;

    // Set address
    if (ble_hs_id_set_rnd(addr.val) != 0) return BLE_ERROR_GAP_ADDRESS;

    return BLE_ERROR_OK;
}

BLE_ErrorTypeDef gap_start_adv(BLE_HandleTypeDef *hble) {
    if (hble == NULL) return BLE_ERROR_MISSING_HANDLE;

    uint8_t err = 0;
    BLE_ErrorTypeDef ble_error = BLE_ERROR_OK;

    // Make sure valid address is set
    if (hble->Config.PrivateAddressEnabled) {
        if ((ble_error = set_random_addr()) != BLE_ERROR_OK) return ble_error;
    }
    if ((err = ble_hs_util_ensure_addr(hble->Config.PrivateAddressEnabled)) != 0) return BLE_ERROR_ADV_ADDR;

    // Find the best address
    if ((err = ble_hs_id_infer_auto(hble->Config.PrivateAddressEnabled, &hble->AddressType)) != 0) return BLE_ERROR_ADV_ADDR_CALC;

    // Copy address to handle
    if ((err = ble_hs_id_copy_addr(hble->AddressType, hble->Address, NULL)) != 0) return BLE_ERROR_ADV_ADDR_COPY;

    // Format address into string
    format_addr(hble->AddressStr, sizeof(hble->AddressStr) / sizeof(hble->AddressStr[0]), hble->Address);

    return start_adv(hble);
}