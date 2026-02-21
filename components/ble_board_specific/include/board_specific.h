//
// Created by Kok on 2/14/26.
//

#ifndef ESP32S3_BLE_BOARD_SPECIFIC_H
#define ESP32S3_BLE_BOARD_SPECIFIC_H

#include <stdint.h>

typedef struct {
    uint16_t LedStateChrHandle;
    uint16_t LEDSetStateChrHandle;
    uint16_t LEDCycleChrHandle;
} BLE_BspChrsTypeDef;

extern BLE_BspChrsTypeDef gBleBspChrs;

#endif //ESP32S3_BLE_BOARD_SPECIFIC_H