//
// Created by Kok on 3/3/26.
//

#ifndef ESP32_BLE_GPS_UBX_H
#define ESP32_BLE_GPS_UBX_H

#include "driver/uart.h"

typedef enum {
    UBX_BaudRate4800 =           4800,
    UBX_BaudRate9600 =           9600,
    UBX_BaudRate19200 =          19200,
    UBX_BaudRate38400 =          38400,
    UBX_BaudRate57600 =          57600,
    UBX_BaudRate115200 =         115200,
    UBX_BaudRate230400 =         230400,
    UBX_BaudRate460800 =         460800
} UBX_BaudRateTypeDef;

typedef enum {
    UBX_ERROR_OK,
    UBX_ERROR_UART_USED,
    UBX_ERROR_UART_CONFIG,
    UBX_ERROR_UART_PIN,
    UBX_ERROR_UART_DRIVER_INSTALL,
    UBX_ERROR_TX,
    UBX_ERROR_CFG_NOACK
} UBX_ErrorTypeDef;

typedef struct {
    uint8_t Class;
    uint8_t MessageId;
    uint16_t Length;
    uint8_t *Payload;
} UBX_MessageTypeDef;

typedef struct {
    UBX_BaudRateTypeDef BaudRate;
    uart_port_t UartPort;
    QueueHandle_t UartQueue;
    uint8_t UartQueueSize;
    uint8_t TxPin;                                          // UART_PIN_NO_CHANGE for default pin
    uint8_t RxPin;                                          // UART_PIN_NO_CHANGE for default pin
    uint16_t TxBufferSize;
    uint16_t RxBufferSize;
} UBX_UartConfigTypeDef;

typedef struct {
    uint8_t AckNackEvent;
    uint8_t AckReceived;
} UBX_ConfigMsgAckNackTypeDef;

typedef struct {
    UBX_UartConfigTypeDef UartConfig;
    UBX_ConfigMsgAckNackTypeDef ConfigMsgAckNack;
} UBX_HandleTypeDef;

UBX_ErrorTypeDef UBX_UartInit(UBX_HandleTypeDef *hubx);
UBX_MessageTypeDef UBX_ParseMessage(uint8_t *Message);

UBX_ErrorTypeDef UBX_SendMsg(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);
UBX_ErrorTypeDef UBX_SendMsgConfig(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);

#endif //ESP32_BLE_GPS_UBX_H