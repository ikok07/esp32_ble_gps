//
// Created by Kok on 2/14/26.
//

#include "ble.h"
#include "gatt.h"

#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

BLE_ErrorTypeDef gatt_init() {
    uint8_t err = 0;

    // Initialize GATT service
    ble_svc_gatt_init();

    // Update GATT services counter
    if ((err = ble_gatts_count_cfg(gGattServices)) != 0) return BLE_ERROR_GATTS_COUNT;

    // Add GATT services
    if ((err = ble_gatts_add_svcs(gGattServices)) != 0) return BLE_ERROR_GATTS_ADD_SVCS;

    return BLE_ERROR_OK;
}
