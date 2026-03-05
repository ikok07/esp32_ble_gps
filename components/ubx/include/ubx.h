//
// Created by Kok on 3/3/26.
//

#ifndef ESP32_BLE_GPS_UBX_H
#define ESP32_BLE_GPS_UBX_H

#include <stdint.h>

#define __weak                                          __attribute__((__weak__))

#define UBX_ACKNACK_MSG_CLASS                           0x05
#define UBX_ACK_MSG_ID                                  0x01
#define UBX_NACK_MSG_ID                                 0x00

#define UBX_MAX_MSG_PAYLOAD_SIZE                        1024
#define UBX_DEFAULT_TIMEOUT                             3000

typedef enum {
    UBX_BaudRate4800 =           4800,
    UBX_BaudRate9600 =           9600,
    UBX_BaudRate19200 =          19200,
    UBX_BaudRate38400 =          38400,
    UBX_BaudRate57600 =          57600,
    UBX_BaudRate115200 =         115200,
    UBX_BaudRate230400 =         230400,
    UBX_BaudRate460800 =         460800,
    UBX_BaudRate921600 =         921600
} UBX_BaudRateTypeDef;

typedef enum {
    UBX_ERROR_OK,
    UBX_ERROR_TIMEOUT,
    UBX_ERROR_UART_USED,
    UBX_ERROR_UART_CONFIG,
    UBX_ERROR_UART_PIN,
    UBX_ERROR_UART_DRIVER_INSTALL,
    UBX_ERROR_UART_BAUD_RATE,
    UBX_ERROR_TX,
    UBX_ERROR_CFG_NOACK
} UBX_ErrorTypeDef;

typedef struct {
    uint8_t Class;
    uint8_t MessageId;
    uint16_t Length;
    uint8_t Payload[UBX_MAX_MSG_PAYLOAD_SIZE];
} UBX_MessageTypeDef;

typedef struct {
    UBX_BaudRateTypeDef BaudRate;
    uint8_t TxPin;                                          // UART_PIN_NO_CHANGE for default pin
    uint8_t RxPin;                                          // UART_PIN_NO_CHANGE for default pin
    uint8_t(*UartInit)(uint32_t BaudRate);                  // Function to initialize the UART driver
    uint8_t(*UartSetBaudRate)(uint32_t BaudRate);           // Function to set UART baud rate
    uint8_t(*UartSend)(uint8_t *Payload, uint32_t Size);    // Function to send data over UART
    uint8_t(*UartFlush)();                                  // Function to flush UART RX buffer
} UBX_UartConfigTypeDef;

typedef struct {
    uint8_t Class;
    uint8_t MessageId;
} UBX_MsgFilterTypeDef;

typedef struct {
    UBX_UartConfigTypeDef UartConfig;
    volatile uint8_t AwaitingMessage;
    volatile uint8_t NewMsgAvailable;
    UBX_MessageTypeDef LatestMessage;
} UBX_HandleTypeDef;

UBX_ErrorTypeDef UBX_UartInit(UBX_HandleTypeDef *hubx);
UBX_MessageTypeDef UBX_ParseMessage(uint8_t *Message);

UBX_ErrorTypeDef UBX_SendMsg(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);
UBX_ErrorTypeDef UBX_SendMsgConfig(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);
UBX_ErrorTypeDef UBX_Poll(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message, UBX_MessageTypeDef *Output);
void UBX_HandleNewMessage(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);

uint32_t UBX_GetTickMsCB();

#endif //ESP32_BLE_GPS_UBX_H
