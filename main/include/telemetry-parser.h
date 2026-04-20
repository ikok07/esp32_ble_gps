//
// Created by Kok on 4/20/26.
//

#ifndef ESP32_BLE_GPS_TELEMETRY_PARSER_H
#define ESP32_BLE_GPS_TELEMETRY_PARSER_H

#define TELPARSER_INPUT_PAYLOAD_SIZE                256

typedef struct {
    uint8_t Class;
    uint8_t MessageId;
    uint16_t Length;
    uint8_t Payload[TELPARSER_INPUT_PAYLOAD_SIZE];
} TELPARSER_InputTypeDef;

void TELPARSER_Init();

#endif //ESP32_BLE_GPS_TELEMETRY_PARSER_H