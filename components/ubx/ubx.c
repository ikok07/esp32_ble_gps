//
// Created by Kok on 3/3/26.
//

#include "ubx.h"

#include <string.h>

static void checksum_calc(UBX_MessageTypeDef *Message, uint8_t *CkA, uint8_t *CkB);
static UBX_ErrorTypeDef wait_for_msg(UBX_HandleTypeDef *hubx, UBX_MsgFilterTypeDef *Filters, uint8_t FiltersLen, uint32_t TimeoutMs, UBX_MessageTypeDef *Message);
static uint8_t wait_with_timeout(uint8_t Condition, uint32_t Timeout);

/**
 * @brief Initializes the UART peripheral
 * @param hubx UBX Handle
 */
UBX_ErrorTypeDef UBX_UartInit(UBX_HandleTypeDef *hubx) {
    esp_err_t err;
    if (uart_is_driver_installed(hubx->UartConfig.UartPort)) return UBX_ERROR_UART_USED;

    uart_config_t UART_Config = {
        .baud_rate = hubx->UartConfig.BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    if ((err = uart_param_config(hubx->UartConfig.UartPort, &UART_Config)) != ESP_OK) return UBX_ERROR_UART_CONFIG;

    if ((err = uart_set_pin(hubx->UartConfig.UartPort, hubx->UartConfig.TxPin, hubx->UartConfig.RxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) != ESP_OK) {
        return UBX_ERROR_UART_PIN;
    }

    if ((err = uart_driver_install(
        hubx->UartConfig.UartPort,
        hubx->UartConfig.RxBufferSize,
        hubx->UartConfig.TxBufferSize,
        hubx->UartConfig.UartQueueSize,
        &hubx->UartConfig.UartQueue,
        0
    )) != ESP_OK) {
        return UBX_ERROR_UART_DRIVER_INSTALL;
    }

    return UBX_ERROR_OK;
}

/**
 * @brief Parses raw uart message buffer into driver specific message structure
 * @param Message Uart message buffer
 * @return UBX raw message
 */
UBX_MessageTypeDef UBX_ParseMessage(uint8_t *Message) {
    UBX_MessageTypeDef message;
    message.Class = Message[2];
    message.MessageId = Message[3];
    message.Length = Message[4] | (Message[5] << 8);
    memcpy(message.Payload, &(Message[6]), message.Length);
    return message;
}

/**
 * @brief Sends message to device
 * @param hubx UBX handle
 * @param Message Message to send to device
 * @note If you send configuration message you SHOULD use UBX_SendMsgConfig()
 */
UBX_ErrorTypeDef UBX_SendMsg(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message) {
    int err;
    uint32_t payload_size = 8 + Message->Length;
    uint8_t uart_payload[payload_size];

    // Add the sync characters
    uart_payload[0] = 0xB5;
    uart_payload[1] = 0x62;

    // Add message class and id
    uart_payload[2] = Message->Class;
    uart_payload[3] = Message->MessageId;

    // Add payload length and the payload
    uart_payload[4] = Message->Length & 0xFF;
    uart_payload[5] = (Message->Length >> 8) & 0xFF;
    memcpy(&(uart_payload[6]), Message->Payload, Message->Length);

    // Add checksum
    uint32_t checksum_start = payload_size - 2;
    checksum_calc(Message, &(uart_payload[checksum_start]), &(uart_payload[checksum_start + 1]));

    if ((err = uart_write_bytes(hubx->UartConfig.UartPort, uart_payload, payload_size)) < 0) {
        return UBX_ERROR_TX;
    };

    return UBX_ERROR_OK;
}

/**
 * @brief Sends configuration message to device (main difference with UBX_SendMsg() is the blocking while waiting for ACK/NACK message)
 * @param hubx UBX handle
 * @param Message Message to send to device
 */
UBX_ErrorTypeDef UBX_SendMsgConfig(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message) {
    UBX_ErrorTypeDef error;
    if ((error = UBX_SendMsg(hubx, Message)) != UBX_ERROR_OK) return error;

    if (wait_with_timeout(!hubx->AwaitingMessage, UBX_DEFAULT_TIMEOUT) != 0) {
        return UBX_ERROR_TIMEOUT;
    }
    hubx->AwaitingMessage = 1;

    UBX_MessageTypeDef resp;
    UBX_MsgFilterTypeDef msg_filters[2] = {
        {.Class = UBX_ACKNACK_MSG_CLASS, .MessageId = UBX_ACK_MSG_ID},
        {.Class = UBX_ACKNACK_MSG_CLASS, .MessageId = UBX_NACK_MSG_ID}
    };
    if ((error = wait_for_msg(hubx, msg_filters, 2, UBX_DEFAULT_TIMEOUT, &resp)) != UBX_ERROR_OK) {
        hubx->AwaitingMessage = 0;
        return UBX_ERROR_CFG_NOACK;
    }

    hubx->AwaitingMessage = 0;

    return UBX_ERROR_OK;
}

/**
 * @brief Sends UBX message and polls for response
 * @param hubx UBX Handle
 * @param Message Message to send to device
 * @param Output Response message from device
 */
UBX_ErrorTypeDef UBX_Poll(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message, UBX_MessageTypeDef *Output) {\
    UBX_ErrorTypeDef ubx_err = UBX_ERROR_OK;

    // TODO: Implement timeout...
    while (hubx->AwaitingMessage) {}
    hubx->AwaitingMessage = 1;

    uart_flush(hubx->UartConfig.UartPort);
    if ((ubx_err = UBX_SendMsg(hubx, Message)) != UBX_ERROR_OK) return ubx_err;

    UBX_MessageTypeDef resp;
    UBX_MsgFilterTypeDef msg_filter = {
        .Class = Message->Class,
        .MessageId = Message->MessageId
    };
    if ((ubx_err = wait_for_msg(hubx, &msg_filter, 1, UBX_DEFAULT_TIMEOUT, &resp)) != UBX_ERROR_OK) {
        hubx->AwaitingMessage = 0;
        return ubx_err;
    }

    hubx->AwaitingMessage = 0;

    *Output = resp;
    return UBX_ERROR_OK;
}

/**
 * @brief After receiving UBX message it should be passed to this method in order to notify the driver
 * @param hubx UBX handle
 * @param Message Received message
 */
void UBX_HandleNewMessage(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message) {
    if (!hubx->AwaitingMessage) return;
    hubx->NewMsgAvailable = 1;
    hubx->LatestMessage = *Message;
}

/**
 * @brief Returns the current systick in ms. This is a mandatory callback without which the driver will not work properly.
 * @return Ticks in ms
 */
__weak uint32_t UBX_GetTickMsCB() {return 0;}

/**
 * @brief Calculates the checksum which is appended to the end of each transferred UBX Message
 * @param Message UBX Message
 * @param CkA The fist checksum value
 * @param CkB The second checksum value
 */
void checksum_calc(UBX_MessageTypeDef *Message, uint8_t *CkA, uint8_t *CkB) {
    uint32_t buffer_size = 5 + Message->Length;
    uint8_t buffer[buffer_size];

    // Add data for which the checksum will be generated
    buffer[0] = Message->Class;
    buffer[1] = Message->MessageId;
    buffer[2] = Message->Length & 0xFF;
    buffer[3] = (Message->Length >> 8) & 0xFF;
    memcpy(&(buffer[4]), Message->Payload, Message->Length);

    uint8_t cka = 0, ckb = 0;
    for (int i = 0; i < buffer_size; i++) {
        cka += buffer[i];
        ckb += cka;
    }

    *CkA = cka;
    *CkB = ckb;
}

/**
 * @brief Waits for specific message to be received
 * @param hubx UBX Handle
 * @param Filters Filters for the received message,
 * @param FiltersLen Length of the filters,
 * @param Message Received message
 */
UBX_ErrorTypeDef wait_for_msg(
    UBX_HandleTypeDef *hubx,
    UBX_MsgFilterTypeDef *Filters,
    uint8_t FiltersLen,
    uint32_t TimeoutMs,
    UBX_MessageTypeDef *Message
) {
    UBX_MessageTypeDef resp;

    uint32_t start_ms = UBX_GetTickMsCB();

    while (1) {
        uint32_t elapsed = UBX_GetTickMsCB() - start_ms;
        if (elapsed > TimeoutMs) return UBX_ERROR_TIMEOUT;

        uint32_t remaining = TimeoutMs - elapsed;
        if (wait_with_timeout(hubx->NewMsgAvailable, remaining) != 0) {
            return UBX_ERROR_TIMEOUT;
        };

        resp = hubx->LatestMessage;

        hubx->NewMsgAvailable = 0;
        hubx->LatestMessage = (UBX_MessageTypeDef){0};

        uint8_t match_found = 0;

        for (uint8_t i = 0; i < FiltersLen; i++) {
            if (Filters[i].Class == resp.Class && Filters[i].MessageId == resp.MessageId) {
                match_found = 1;
                break;
            };
        }

        if (match_found) {
            *Message = resp;
            break;
        }
    }
    return UBX_ERROR_OK;
}


uint8_t wait_with_timeout(uint8_t Condition, uint32_t Timeout) {
    uint32_t start = UBX_GetTickMsCB();       // ms
    while (!Condition) {
        if ((UBX_GetTickMsCB() - start) > Timeout) return 1;
    }
    return 0;
}
