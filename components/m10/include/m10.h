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
    char SwVersion[30];
    char HwVersion[30];
} M10_DeviceVersionTypeDef;

typedef struct {
    uint16_t Year;
    uint8_t Month;
    uint8_t Day;
    uint8_t Hour;
    uint8_t Minute;
    uint8_t Second;
    uint32_t Nanosecond;
} M10_ParsedTimestampMsTypeDef;

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

typedef  struct {
    UBX_BaudRateTypeDef BaudRate;
    M10_DeviceVersionTypeDef Version;
} M10_ConnectionInfoTypeDef;

typedef struct {
    M10_UpdateRateTypeDef UpdateRate;
    uint8_t Constellations;                                         // Set bits with M10_CONSTELLATION_XXX
    uint32_t NMEAOutputMessages;                                    // Set bits with M10_NMEA_MSG_XXX_XXX
    M10_PowerConfigurationTypeDef PowerConfiguration;
    uint32_t PositionUpdatePeriodSeconds;                           // Used if M10_PWR_CFG_PSMOO is selected. 5 <= <value> <= number of seconds in a week
    M10_NavModelTypeDef NavModel;
    uint16_t PDOP;                                                  // Position Dilution of Precision (Lower is more restrictive.). Default 250 (250 / 10 = 25)
    uint16_t TDOP;                                                  // Time Dilution of Precision (Lower is more restrictive.). Default 250 (250 / 10 = 25)
    uint8_t ConfigLayers;                                           // Set bits with M10_CONFIG_LAYER_XXX
} M10_ConfigTypeDef;

typedef struct {
    UBX_HandleTypeDef hubx;
    M10_ConfigTypeDef DeviceConfig;
} M10_HandleTypeDef;

M10_ErrorTypeDef M10_InitUART(M10_HandleTypeDef *hm10);
M10_ErrorTypeDef M10_Init(M10_HandleTypeDef *hm10);
void M10_SignalMessageReceived(M10_HandleTypeDef *hm10, M10_MessageTypeTypeDef MessageType, void *Payload);

/** ------ Commands ------ */
M10_ErrorTypeDef M10_SendConfig(M10_HandleTypeDef *hm10, M10_ConfigDataTypeDef *CfgData, uint32_t CfgDataLen, uint8_t Layers, uint8_t SkipAck);

/** ------ Status info ------ */
M10_ErrorTypeDef M10_GetStatus(M10_HandleTypeDef *hm10, M10_DeviceStatusTypeDef *Status);
M10_ErrorTypeDef M10_GetVersion(M10_HandleTypeDef *hm10, M10_DeviceVersionTypeDef *Version);
uint8_t M10_HasValidFix(M10_HandleTypeDef *hm10);

/** ------ Controls ------ */
M10_ErrorTypeDef M10_Reset(M10_HandleTypeDef *hm10, M10_NavBbrMaskTypeDef BbrMask, M10_ResetModeTypeDef ResetMode);
M10_ErrorTypeDef M10_GnssStop(M10_HandleTypeDef *hm10);
M10_ErrorTypeDef M10_GnssStart(M10_HandleTypeDef *hm10);
M10_ErrorTypeDef M10_SetBaudRate(M10_HandleTypeDef *hm10, UBX_BaudRateTypeDef BaudRate, uint8_t Layers);

/* ------ MGA Messages ------ */
/*
 * Before calling any of the MGA messages, UTC must be set with M10_SetUTC()
*/
M10_ErrorTypeDef M10_SetUTC(M10_HandleTypeDef *hm10, uint64_t TimestampMs, uint16_t SAccuracy, uint32_t NSAccuracy, uint32_t TimeoutMs);
M10_ErrorTypeDef M10_ExportNavData(M10_HandleTypeDef *hm10, uint8_t(*HandleDataMessage)(uint8_t *ChunkContent, uint32_t Len), uint32_t *DataLen, uint32_t TimeoutMs);
M10_ErrorTypeDef M10_ImportNavData(M10_HandleTypeDef *hm10, uint8_t *Data, uint32_t DataLen, uint32_t TimeoutMs);

#endif //ESP32_BLE_GPS_M10_H