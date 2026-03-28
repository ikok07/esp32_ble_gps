//
// Created by Kok on 3/28/26.
//

#ifndef ESP32_BLE_GPS_BT_CONFIG_H
#define ESP32_BLE_GPS_BT_CONFIG_H

#include <stdint.h>

typedef struct {
    uint16_t LedStateChrHandle;
    uint16_t LEDSetStateChrHandle;
    uint16_t LEDCycleChrHandle;
} BLE_AttributesTypeDef;

extern BLE_AttributesTypeDef gBleAttributes;

#endif //ESP32_BLE_GPS_BT_CONFIG_H