//
// Created by Kok on 3/3/26.
//

#ifndef ESP32_BLE_GPS_UBX_H
#define ESP32_BLE_GPS_UBX_H

#include <stdint.h>

#define _weak __attribute__((weak))


#define UBX_MAX_MSG_PAYLOAD_SIZE                        1024
#define UBX_MSG_PAYLOAD_POOL_SIZE                       5
#define UBX_DEFAULT_TIMEOUT                             3000

#define UBX_SYNC_CHAR_ONE                               0xB5
#define UBX_SYNC_CHAR_TWO                               0x62

#define UBX_ACKNACK_MSG_CLASS                           0x05
#define UBX_CFG_ACK_MSG_ID                              0x01
#define UBX_CFG_NACK_MSG_ID                             0x00

typedef enum {
    UBX_BR_4800     =           4800,
    UBX_BR_9600     =           9600,
    UBX_BR_19200    =          19200,
    UBX_BR_38400    =          38400,
    UBX_BR_57600    =          57600,
    UBX_BR_115200   =         115200,
    UBX_BR_230400   =         230400,
    UBX_BR_460800   =         460800,
    UBX_BR_921600   =         921600
} UBX_BaudRateTypeDef;

typedef enum {
    UBX_ERROR_OK,
    UBX_ERROR_TIMEOUT,
    UBX_ERROR_MSG_NULL,
    UBX_ERROR_PAYLOAD_NULL,
    UBX_ERROR_TX,
    UBX_ERROR_PAYLOAD_OVERFLOW,
    UBX_ERROR_CFG_NOACK,
    UBX_ERROR_POOL_FULL,
    UBX_ERROR_POOL_INVALID_IDX,
    UBX_ERROR_INVALID_CHECKSUM,
    UBX_ERROR_UART_USED,
    UBX_ERROR_UART_CONFIG,
    UBX_ERROR_UART_PIN,
    UBX_ERROR_UART_DRIVER_INSTALL,
    UBX_ERROR_UART_BAUD_RATE,
} UBX_ErrorTypeDef;

typedef struct {
    uint32_t Cka;
    uint32_t Ckb;
} UBX_MsgChecksumTypeDef;

typedef struct {
    uint8_t Payload[UBX_MAX_MSG_PAYLOAD_SIZE];
    uint32_t Length;
    uint8_t InUse;
} UBX_PayloadPoolItem;

typedef struct {
    uint8_t Class;
    uint8_t MessageId;
    uint16_t Length;
    UBX_PayloadPoolItem *PayloadPoolItem;
    uint8_t PayloadPoolItemIdx;
    UBX_MsgChecksumTypeDef Checksum;
} UBX_MessageTypeDef;

typedef struct {
    UBX_BaudRateTypeDef BaudRate;
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
    uint8_t TxBuffer[8 + UBX_MAX_MSG_PAYLOAD_SIZE];
    UBX_PayloadPoolItem PayloadPool[UBX_MSG_PAYLOAD_POOL_SIZE];

    /** @return 0 - OK; 1 - timed out**/
    uint8_t (*WaitForMsg)(UBX_MessageTypeDef *Message, uint32_t TimeoutMs);
    uint8_t (*SignalNewMsg)(UBX_MessageTypeDef *Message, uint32_t TimeoutMs);
} UBX_HandleTypeDef;

UBX_ErrorTypeDef UBX_UartInit(UBX_HandleTypeDef *hubx);

UBX_ErrorTypeDef UBX_SendMsg(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);
UBX_ErrorTypeDef UBX_SendMsgRaw(UBX_HandleTypeDef *hubx, uint8_t *MessageRaw);
UBX_ErrorTypeDef UBX_SendMsgConfig(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);
UBX_ErrorTypeDef UBX_Poll(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message, UBX_MessageTypeDef *Output);

UBX_ErrorTypeDef UBX_ParseMessage(UBX_HandleTypeDef *hubx, uint8_t *MessageRaw, UBX_MessageTypeDef *Message);
void UBX_HandleNewMessage(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);
UBX_ErrorTypeDef UBX_AssignMessagePayloadPoolItem(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);
UBX_ErrorTypeDef UBX_ReleaseMessage(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message);
UBX_ErrorTypeDef UBX_WaitForMessage(UBX_HandleTypeDef *hubx, UBX_MsgFilterTypeDef *Filters, uint8_t FiltersLen, uint32_t TimeoutMs, UBX_MessageTypeDef *Message);


uint32_t UBX_GetTickMsCB();

#endif //ESP32_BLE_GPS_UBX_H
