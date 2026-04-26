//
// Created by Kok on 2/14/26.
//

#ifndef ESP32S3_BLE_BT_H
#define ESP32S3_BLE_BT_H

#include "m10.h"
#include "ble.h"
#include "log.h"

#define BT_BASE_DATA_LOC_AND_SPD_EVT_BIT            BIT0
#define BT_BASE_DATA_ELEVATION_EVT_BIT              BIT1
#define BT_BASE_DATA_DATE_TIME_EVT_BIT              BIT3

#define BT_PRECISION_DATA_GNSS_FIX_EVT_BIT          BIT0

typedef struct {
    uint16_t Year;
    uint8_t Month;
    uint8_t Day;
    uint8_t Hours;
    uint8_t Minutes;
    uint8_t Seconds;
} BT_DateTimeDataTypeDef;

typedef struct {
    M10_DeviceFixTypeDef GnssFix;
    uint32_t VelocityCmPerS;
    int32_t Latitude;
    int32_t Longitude;
    int32_t AltitudeCm;
    uint32_t Heading;
    uint32_t HorizontalAccuracyCm;
    BT_DateTimeDataTypeDef DateTime;
} BT_GnssBaseDataTypeDef;

typedef struct {
    M10_DeviceFixTypeDef GnssFix;
    uint8_t VehicleCount;
    uint8_t Hdop;
    uint8_t Vdop;
} BT_GnssPrecisionDataTypeDef;

typedef struct {
    uint16_t LNFeatureChrHandle;
    uint16_t LocationAndSpeedChrHandle;
    uint16_t LocationAndSpeedHumanChrHandle;
    uint16_t NavDataChrHandle;
    uint16_t GnssFixQualityChrHandle;
    uint16_t ElevationChrHandle;
    uint16_t DateTimeChrHandle;
} BT_AttributesTypeDef;

extern BT_AttributesTypeDef gBleAttributes;

void BT_Init();
void BT_Configure(BLE_HandleTypeDef *hble);
void BT_InitNotifications();

/* ------ Access Callbacks ------ */

int BT_LNFeaturesAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

int BT_LocAndSpdAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

#ifdef DEBUG_MODE_ENABLED
int BT_LocAndSpdHumanAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
#endif

// int BT_NavDataAccessCB(uint16_t conn_handle, uint16_t attr_handle,
//                           struct ble_gatt_access_ctxt *ctxt, void *arg);
//
int BT_GnssFixQltAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

int BT_AltitudeAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

int BT_DateTimeAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

int BT_DescriptionDescAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);

/* ------ Utilities ------ */

void BT_FormatLocAndSpdBuffer(BT_GnssBaseDataTypeDef *Data, uint8_t *Buffer);
void BT_FormatGnssFixQltBuffer(BT_GnssPrecisionDataTypeDef *Data, uint8_t *Buffer);
void BT_FormatDateTimeBuffer(BT_GnssBaseDataTypeDef *Data, uint8_t *Buffer);

#endif //ESP32S3_BLE_BT_H