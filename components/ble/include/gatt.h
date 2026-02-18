//
// Created by Kok on 2/14/26.
//

#ifndef ESP32S3_BLE_GATT_H
#define ESP32S3_BLE_GATT_H

#include "host/ble_gap.h"

BLE_ErrorTypeDef gatt_init();
int on_gatt_subscribe(struct ble_gap_event *event);
void on_gatt_event(struct ble_gatt_register_ctxt *ctxt,
                                  void *arg);

#endif //ESP32S3_BLE_GATT_H