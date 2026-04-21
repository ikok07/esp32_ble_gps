//
// Created by Kok on 4/21/26.
//

#ifndef ESP32_BLE_GPS_BT_CHARS_H
#define ESP32_BLE_GPS_BT_CHARS_H

#include "m10.h"

typedef struct {
    uint16_t Year;
    uint8_t Month;
    uint8_t Day;
    uint8_t Hours;
    uint8_t Minutes;
    uint8_t Seconds;
} BTCHAR_DateTimeDataTypeDef;

typedef struct {
    M10_DeviceFixTypeDef GnssFix;
    uint32_t Velocity;                  // 1/100 from m/s
    int32_t Latitude;
    int32_t Longitude;
    int32_t Altitude;                   // 1/100 from m
    uint32_t Heading;
    BTCHAR_DateTimeDataTypeDef DateTime;
} BTCHAR_LocationAndSpeedDataTypeDef;

#endif //ESP32_BLE_GPS_BT_CHARS_H