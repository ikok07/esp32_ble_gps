//
// Created by Kok on 3/28/26.
//

#ifndef ESP32_BLE_GPS_BT_CONFIG_H
#define ESP32_BLE_GPS_BT_CONFIG_H

#include "ble.h"

typedef struct {
    uint16_t LNFeatureChrHandle;
    uint16_t LocationAndSpeedChrHandle;
    uint16_t NavDataChrHandle;
    uint16_t GnssFixQualityChrHandle;
    uint16_t AltitudeChrHandle;
    uint16_t DateTimeChrHandle;
} BLE_AttributesTypeDef;

extern BLE_AttributesTypeDef gBleAttributes;

void BT_Configure(BLE_HandleTypeDef *hble);

#endif //ESP32_BLE_GPS_BT_CONFIG_H