//
// Created by Kok on 3/3/26.
//

#ifndef ESP32_BLE_GPS_M10_H
#define ESP32_BLE_GPS_M10_H

#include "ubx.h"
#include "m10_enums.h"

typedef struct __attribute__((packed)) {
    uint32_t Itow;
    uint8_t  GpsFix;         // byte 4
    uint8_t  Flags;          // byte 5 — bit 0 = gpsFixOk
    uint8_t  FixStat;        // byte 6
    uint8_t  Flags2;         // byte 7
    uint32_t Ttff;           // bytes 8-11
    uint32_t Msss;           // bytes 12-15
} M10_NavStatusPayloadTypeDef;

typedef struct {
    M10_DeviceFixTypeDef Fix;
    uint8_t FixOk;
    uint8_t WknValid;                               // Week number valid
    uint8_t ToWValid;                               // Time of week valid
    uint32_t Ttff;                                  // Time to first fix
    uint32_t Msss;                                  // Time since the last receiver startup or GNSS restart
} M10_DeviceStatusTypeDef;

typedef struct {
    M10_CfgItemKeysTypeDef Key;
    uint32_t Value;
} M10_ConfigDataTypeDef;

typedef struct {
    M10_BaudRateTypeDef BaudRate;
    M10_UpdateRateTypeDef UpdateRate;
    uint8_t Constellations;                                         // Set bits with M10_CONSTELLATION_XXX
    uint32_t NMEAOutputMessages;                                    // Set bits with M10_NMEA_MSG_XXX_XXX
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
M10_ErrorTypeDef M10_GetStatus(M10_HandleTypeDef *hm10, M10_DeviceStatusTypeDef *Status);
M10_ErrorTypeDef M10_Reset(M10_HandleTypeDef *hm10, M10_NavBbrMaskTypeDef BbrMask, M10_ResetModeTypeDef ResetMode);

#endif //ESP32_BLE_GPS_M10_H