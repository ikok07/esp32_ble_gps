//
// Created by Kok on 2/14/26.
//

#include "gap.h"

#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"

int on_gap_event(struct ble_gap_event *event, void *arg) {
    uint8_t err = 0;
    struct ble_gap_conn_desc desc;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                if ((err = ble_gap_conn_find(event->connect.conn_handle, &desc)) != 0) {
                    BLE_GapEventCB(BLE_GAP_EVENT_CONN_FAILED, event, NULL);
                    return err;
                }
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_SUCCESS, event, &desc);
            } else {
                // Connection failed, restart advertising
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_FAILED, event,  NULL);
                BLE_StartAdvertising();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            BLE_GapEventCB(BLE_GAP_EVENT_CONN_DISCONNECT, event,  NULL);
            BLE_StartAdvertising();
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:
            if ((err = ble_gap_conn_find(event->connect.conn_handle, &desc)) != 0) {
                BLE_GapEventCB(BLE_GAP_EVENT_CONN_FAILED, event,  NULL);
                return err;
            }
            BLE_GapEventCB(BLE_GAP_EVENT_CONN_UPD, event,  &desc);
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            BLE_GapEventCB(BLE_GAP_EVENT_SUB, event, NULL);
            break;
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

BLE_ErrorTypeDef gap_start_adv(BLE_HandleTypeDef *hble) {
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

    // Set response fields
    if ((err = ble_gap_adv_rsp_set_fields(&rsp_fields)) != 0) return BLE_ERROR_RSP_FIELDS;

    /* ------ Response params ------ */

    // Make device discoverable and connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;

    // Set advertising interval
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(hble->Config.AdvertisingIntervalMS);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(hble->Config.AdvertisingIntervalMS + 10);

    // Start advertising
    if ((err = ble_gap_adv_start(hble->AddressType, NULL, BLE_HS_FOREVER, &adv_params, on_gap_event, NULL))) return BLE_ERROR_ADV_START;

    return BLE_ERROR_OK;
}