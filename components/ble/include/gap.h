//
// Created by Kok on 2/14/26.
//

#ifndef ESP32S3_BLE_GAP_H
#define ESP32S3_BLE_GAP_H

#include "ble.h"
#include "host/ble_gap.h"

int on_gap_event(struct ble_gap_event *event, void *arg);
BLE_ErrorTypeDef gap_init(BLE_HandleTypeDef *hble);
BLE_ErrorTypeDef gap_start_adv(BLE_HandleTypeDef *hble);

#endif //ESP32S3_BLE_GAP_H