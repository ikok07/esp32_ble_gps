//
// Created by Kok on 3/3/26.
//

#ifndef ESP32_BLE_GPS_M10_H
#define ESP32_BLE_GPS_M10_H

#include "ubx.h"
#include "m10_types.h"

typedef enum {
    M10_ERROR_OK,
    M10_ERROR_UBX,
    M10_ERROR_BAUD_RATE,
    M10_ERROR_CFG_TOO_MANY_ITEMS,
    M10_ERROR_CFG_INVALID_KEY,
} M10_ErrorTypeDef;

typedef struct {
    M10_CfgItemKeysTypeDef Key;
    uint32_t Value;
} M10_ConfigDataTypeDef;

typedef struct {
    M10_BaudRateTypeDef InitialBaudRate;                            // Baud rate which is set after chip reset
    M10_BaudRateTypeDef BaudRate;
    M10_UpdateRateTypeDef UpdateRate;
    uint8_t Constellations;                                         // Set bits with M10_CONSTELLATION_XXX
    uint32_t NMEAOutputMessages;                                    // Set bits with M10_NMEA_MSG_XXX_XXX
    uint8_t ConstellationsLen;
    M10_PowerConfigurationTypeDef PowerConfiguration;
    uint32_t PositionUpdatePeriodSeconds;                           // Used if M10_PWR_CFG_PSMOO is selected. 5 <= <value> <= number of seconds in a week
    M10_NavModelTypeDef NavModel;
    uint8_t ConfigLayers;                                           // Set bits with M10_CONFIG_LAYER_XXX
} M10_ConfigTypeDef;

typedef struct {
    UBX_HandleTypeDef hubx;
    M10_ConfigTypeDef DeviceConfig;
} M10_HandleTypeDef;

M10_ErrorTypeDef M10_Init(M10_HandleTypeDef *hm10);

#endif //ESP32_BLE_GPS_M10_H