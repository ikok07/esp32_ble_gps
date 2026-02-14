//
// Created by Kok on 2/13/26.
//

#include "ble.h"

#include "nvs_flash.h"
#include "task_scheduler.h"

#include "host/ble_hs.h"
#include "host/util/util.h"

#include "nimble/nimble_port.h"
#include "nimble/ble.h"

#include <sys/types.h>

#include "services/gap/ble_svc_gap.h"

/* ------ Global variables ------ */
BLE_HandleTypeDef *ghBle = NULL;

/* ------ Callbacks ------ */

static void on_stack_sync_cb(void);
static int on_gap_event(struct ble_gap_event *event, void *arg);

/* ------ Tasks ------ */

static void ble_task(void *arg);

/* ------ Private methods ------ */

static BLE_ErrorTypeDef gap_init();
static BLE_ErrorTypeDef start_adv();
static void format_addr(char *AddrStr, uint8_t Len, uint8_t Address[]);

BLE_ErrorTypeDef BLE_Init(BLE_HandleTypeDef *hble) {
    ghBle = hble;

    BLE_ErrorTypeDef ble_err = BLE_ERROR_OK;
    esp_err_t err = ESP_OK;

    uint8_t nvs_ready = 0;
    while (!nvs_ready) {
        // Initialize the flash memory
        if ((err = nvs_flash_init()) != ESP_OK) {
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                nvs_flash_erase();
            }
            return BLE_ERROR_NVS;
        }
        nvs_ready = 1;
    }

    // Initialize the NimBLE stack
    if ((err = nimble_port_init()) != ESP_OK) {
        return BLE_ERROR_INIT;
    }

    ble_hs_cfg.reset_cb = BLE_StackResetCB;
    ble_hs_cfg.sync_cb = on_stack_sync_cb;

    // Initialize GAP
    if ((ble_err = gap_init()) != BLE_ERROR_OK) return ble_err;

    hble->BLE_Task->Function = ble_task;
    SCHEDULER_Create(hble->BLE_Task);

    return BLE_ERROR_OK;
}

uint8_t BLE_CanAdvertise() {
    if (!ghBle) return 0;
    return ghBle->State == BLE_STATE_READY_FOR_ADV;
}

BLE_ErrorTypeDef BLE_StartAdvertising() {
    if (ghBle == NULL) return BLE_ERROR_MISSING_HANDLE;
    if (ghBle->State != BLE_STATE_READY_FOR_ADV) return BLE_ERROR_INVALID_STATE;

    uint8_t err = 0;

    // Make sure valid address is set
    if ((err = ble_hs_util_ensure_addr(ghBle->Config.PrivateAddressEnabled)) != 0) return BLE_ERROR_ADV_ADDR;

    // Find the best address
    if ((err = ble_hs_id_infer_auto(ghBle->Config.PrivateAddressEnabled, &ghBle->AddressType)) != 0) return BLE_ERROR_ADV_ADDR_CALC;

    // Copy address to handle
    if ((err = ble_hs_id_copy_addr(ghBle->AddressType, ghBle->Address, NULL)) != 0) return BLE_ERROR_ADV_ADDR_COPY;

    // Format address into string
    format_addr(ghBle->AddressStr, sizeof(ghBle->AddressStr) / sizeof(ghBle->AddressStr[0]), ghBle->Address);

    return start_adv();
}

__weak void BLE_StackResetCB(int reason) {}

BLE_ErrorTypeDef gap_init() {
    if (ghBle == NULL) return BLE_ERROR_MISSING_HANDLE;
    uint8_t err = 0;

    // Initialize GAP service
    ble_svc_gap_init();

    // Set device name
    if ((err = ble_svc_gap_device_name_set(ghBle->Config.DeviceName)) != 0) return BLE_ERROR_GAP_NAME;

    // Set GAP appearance
    if ((err = ble_svc_gap_device_appearance_set(ghBle->Config.GapAppearance)) != 0) return BLE_ERROR_GAP_APPEARANCE;

    return BLE_ERROR_OK;
}

BLE_ErrorTypeDef start_adv() {
    if (ghBle == NULL) return BLE_ERROR_MISSING_HANDLE;
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
    adv_fields.appearance = ghBle->Config.GapAppearance;
    adv_fields.appearance_is_present = 1;

    // Set device role
    adv_fields.le_role = ghBle->Config.GapRole;
    adv_fields.le_role_is_present = 1;

    // Set advertisement fields
    if ((err = ble_gap_adv_set_fields(&adv_fields)) != 0) return BLE_ERROR_ADV_FIELDS;

    /* ------ Response fields ------ */

    // Set device address
    rsp_fields.device_addr = ghBle->Address;
    rsp_fields.device_addr_type = ghBle->AddressType;
    rsp_fields.device_addr_is_present = 1;

    // Set advertising interval
    rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(ghBle->Config.AdvertisingIntervalMS);
    rsp_fields.adv_itvl_is_present = 1;

    // Set response fields
    if ((err = ble_gap_adv_rsp_set_fields(&rsp_fields)) != 0) return BLE_ERROR_RSP_FIELDS;

    /* ------ Response params ------ */

    // Make device discoverable and connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;

    // Set advertising interval
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(ghBle->Config.AdvertisingIntervalMS);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(ghBle->Config.AdvertisingIntervalMS + 10);

    // Start advertising
    if ((err = ble_gap_adv_start(ghBle->AddressType, NULL, BLE_HS_FOREVER, &adv_params, on_gap_event, NULL))) return BLE_ERROR_ADV_START;

    return BLE_ERROR_OK;
}

void on_stack_sync_cb(void) {
    ghBle->State = BLE_STATE_READY_FOR_ADV;
}

int on_gap_event(struct ble_gap_event *event, void *arg) {
    return 0;
}

void format_addr(char *AddrStr, uint8_t Len, uint8_t Address[]) {
    snprintf(AddrStr, Len, "%02X:%02X:%02X:%02X:%02X:%02X:", Address[0], Address[1], Address[2], Address[3], Address[4], Address[5]);
}

void ble_task(void *arg) {
    while (1) {
        vTaskDelay(1000);
    };
}
