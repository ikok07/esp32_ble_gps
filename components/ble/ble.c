//
// Created by Kok on 2/13/26.
//

#include "ble.h"
#include "gap.h"
#include "gatt.h"

#include "nvs_flash.h"
#include "task_scheduler.h"

#include "host/ble_hs.h"
#include "host/util/util.h"

#include "nimble/nimble_port.h"
#include "nimble/ble.h"

/* ------ Global variables ------ */
BLE_HandleTypeDef *gHble = NULL;

/* ------ Callbacks ------ */

static void on_stack_sync_cb(void);

/* ------ Tasks ------ */

static void ble_task(void *arg);

/* ------ Private methods ------ */

static void format_addr(char *AddrStr, uint8_t Len, uint8_t Address[]);

/**
 * @brief Initializes the BLE driver by enabling NVS, GAP and GATT
 * @param hble BLE Handle
 */
BLE_ErrorTypeDef BLE_Init(BLE_HandleTypeDef *hble) {
    gHble = hble;

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
    if ((ble_err = gap_init(gHble)) != BLE_ERROR_OK) return ble_err;

    // Initialize GATT
    if ((ble_err = gatt_init()) != BLE_ERROR_OK) return ble_err;

    // Create BLE task
    hble->BLE_Task->Function = ble_task;
    SCHEDULER_Create(hble->BLE_Task);

    return BLE_ERROR_OK;
}

/**
 * @brief Check whether the device can start advertising
 * @note This method MUST return true before calling  BLE_StartAdvertising()
 * @return 0 - The device is not ready to start advertising
 */
uint8_t BLE_CanAdvertise() {
    return !!gHble && gHble->State == BLE_STATE_READY_FOR_ADV;
}

/**
 * @brief Starts the GAP advertising
 */
BLE_ErrorTypeDef BLE_StartAdvertising() {
    if (gHble == NULL) return BLE_ERROR_MISSING_HANDLE;
    if (gHble->State != BLE_STATE_READY_FOR_ADV) return BLE_ERROR_INVALID_STATE;

    uint8_t err = 0;

    // Make sure valid address is set
    if ((err = ble_hs_util_ensure_addr(gHble->Config.PrivateAddressEnabled)) != 0) return BLE_ERROR_ADV_ADDR;

    // Find the best address
    if ((err = ble_hs_id_infer_auto(gHble->Config.PrivateAddressEnabled, &gHble->AddressType)) != 0) return BLE_ERROR_ADV_ADDR_CALC;

    // Copy address to handle
    if ((err = ble_hs_id_copy_addr(gHble->AddressType, gHble->Address, NULL)) != 0) return BLE_ERROR_ADV_ADDR_COPY;

    // Format address into string
    format_addr(gHble->AddressStr, sizeof(gHble->AddressStr) / sizeof(gHble->AddressStr[0]), gHble->Address);

    return gap_start_adv(gHble);
}

/**
 * @brief This callback will be executed whenever the BLE stack gets reset by an error
 */
__weak void BLE_StackResetCB(int Reason) {}

/**
 * @brief This callback will be executed when there is new GAP event.
 * @param event GAP event
 */
// void BLE_GapEventCB(BLE_GapEventTypeDef Event, void *Arg) {}

void on_stack_sync_cb(void) {
    if (!gHble) return;
    gHble->State = BLE_STATE_READY_FOR_ADV;
}

void format_addr(char *AddrStr, uint8_t Len, uint8_t Address[]) {
    snprintf(AddrStr, Len, "%02X:%02X:%02X:%02X:%02X:%02X:", Address[0], Address[1], Address[2], Address[3], Address[4], Address[5]);
}

void ble_task(void *arg) {
    nimble_port_run();
    vTaskDelete(NULL);
}
