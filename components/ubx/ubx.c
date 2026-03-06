//
// Created by Kok on 3/3/26.
//

#include "ubx.h"

#include <string.h>

static void checksum_calc(uint8_t *Buffer, uint8_t ChecksumLen, uint8_t *CkA, uint8_t *CkB);
static UBX_ErrorTypeDef wait_for_msg(UBX_HandleTypeDef *hubx, UBX_MsgFilterTypeDef *Filters, uint8_t FiltersLen, uint32_t TimeoutMs, UBX_MessageTypeDef *Message);

static UBX_ErrorTypeDef payload_pool_get_item(UBX_HandleTypeDef *hubx, UBX_PayloadPoolItem **PoolItem, uint8_t *ItemIdx);
static UBX_ErrorTypeDef payload_pool_release_item(UBX_HandleTypeDef *hubx, uint8_t ItemIdx);

/**
 * @brief Initializes the UART peripheral
 * @param hubx UBX Handle
 */
UBX_ErrorTypeDef UBX_UartInit(UBX_HandleTypeDef *hubx) {
    if (hubx->UartConfig.UartInit(hubx->UartConfig.BaudRate) != 0) {
        return UBX_ERROR_UART_CONFIG;
    };
    return UBX_ERROR_OK;
}

/**
 * @brief Parses raw uart message buffer into driver specific message structure
 * @param Message Uart message buffer
 * @return UBX raw message
 */
UBX_ErrorTypeDef UBX_ParseMessage(UBX_HandleTypeDef *hubx, uint8_t *MessageRaw, UBX_MessageTypeDef *Message) {
    UBX_ErrorTypeDef ubx_err;
    if (Message == NULL) return UBX_ERROR_MSG_NULL;
    if ((ubx_err = payload_pool_get_item(hubx, &Message->PayloadPoolItem, &Message->PayloadPoolItemIdx)) != UBX_ERROR_OK) {
        return ubx_err;
    }

    Message->Class = MessageRaw[2];
    Message->MessageId = MessageRaw[3];
    Message->Length = MessageRaw[4] | (MessageRaw[5] << 8);

    if (Message->Length > Message->PayloadPoolItem->Length) return UBX_ERROR_PAYLOAD_OVERFLOW;
    memcpy(Message->PayloadPoolItem->Payload, &(MessageRaw[6]), Message->Length);

    return UBX_ERROR_OK;
}

/**
 * @brief Sends message to device
 * @param hubx UBX handle
 * @param Message Message to send to device
 * @note If you send configuration message you SHOULD use UBX_SendMsgConfig()
 */
UBX_ErrorTypeDef UBX_SendMsg(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message) {
    if (Message->PayloadPoolItem == NULL) return UBX_ERROR_PAYLOAD_NULL;
    if (Message->Length > UBX_MAX_MSG_PAYLOAD_SIZE) return UBX_ERROR_PAYLOAD_OVERFLOW;

    int err;
    uint32_t payload_size = 8 + Message->Length;

    // Add the sync characters
    hubx->TxBuffer[0] = 0xB5;
    hubx->TxBuffer[1] = 0x62;

    // Add message class and id
    hubx->TxBuffer[2] = Message->Class;
    hubx->TxBuffer[3] = Message->MessageId;

    // Add payload length and the payload
    hubx->TxBuffer[4] = Message->Length & 0xFF;
    hubx->TxBuffer[5] = (Message->Length >> 8) & 0xFF;
    memcpy(&(hubx->TxBuffer[6]), Message->PayloadPoolItem->Payload, Message->Length);

    // Add checksum
    uint32_t checksum_start = payload_size - 2;
    checksum_calc(hubx->TxBuffer, 4 + Message->Length, &(hubx->TxBuffer[checksum_start]), &(hubx->TxBuffer[checksum_start + 1]));

    if ((err = hubx->UartConfig.UartSend(hubx->TxBuffer, payload_size)) != 0) {
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

    UBX_MessageTypeDef resp;
    UBX_MsgFilterTypeDef msg_filters[2] = {
        {.Class = UBX_ACKNACK_MSG_CLASS, .MessageId = UBX_ACK_MSG_ID},
        {.Class = UBX_ACKNACK_MSG_CLASS, .MessageId = UBX_NACK_MSG_ID}
    };
    if ((error = wait_for_msg(hubx, msg_filters, 2, UBX_DEFAULT_TIMEOUT, &resp)) != UBX_ERROR_OK) {
        return UBX_ERROR_CFG_NOACK;
    }

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

    hubx->UartConfig.UartFlush();
    if ((ubx_err = UBX_SendMsg(hubx, Message)) != UBX_ERROR_OK) return ubx_err;

    UBX_MessageTypeDef resp;
    UBX_MsgFilterTypeDef msg_filter = {
        .Class = Message->Class,
        .MessageId = Message->MessageId
    };
    if ((ubx_err = wait_for_msg(hubx, &msg_filter, 1, UBX_DEFAULT_TIMEOUT, &resp)) != UBX_ERROR_OK) {
        return ubx_err;
    }

    *Output = resp;
    return UBX_ERROR_OK;
}

/**
 * @brief After receiving UBX message it should be passed to this method in order to notify the driver
 * @param hubx UBX handle
 * @param Message Received message
 */
void UBX_HandleNewMessage(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message) {
    // Signal the waiting task that a message is ready
    if (hubx->SignalNewMsg) hubx->SignalNewMsg(Message, UBX_DEFAULT_TIMEOUT);
}

/**
 * @brief Returns the current systick in ms. This is a mandatory callback without which the driver will not work properly.
 * @return Ticks in ms
 */
__weak uint32_t UBX_GetTickMsCB() {return 0;}

/**
 * @brief Calculates the checksum which is appended to the end of each transferred UBX Message
 * @param Buffer UBX Message buffer
 * @param ChecksumLen Length of the section of the buffer for which checksum is calculated
 * @param CkA The fist checksum value
 * @param CkB The second checksum value
 */
void checksum_calc(uint8_t *Buffer, uint8_t ChecksumLen, uint8_t *CkA, uint8_t *CkB) {
    uint8_t checksum_start = 2;
    uint8_t cka = 0, ckb = 0;
    for (int i = checksum_start; i < ChecksumLen; i++) {
        cka += Buffer[i];
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
    uint32_t start_ms = UBX_GetTickMsCB();

    while (1) {
        uint32_t elapsed = UBX_GetTickMsCB() - start_ms;
        if (elapsed > TimeoutMs) return UBX_ERROR_TIMEOUT;

        uint32_t remaining = TimeoutMs - elapsed;

        UBX_MessageTypeDef latest_msg;
        uint8_t timed_out = hubx->WaitForMsg(&latest_msg, remaining);

        if (timed_out) return UBX_ERROR_TIMEOUT;

        uint8_t match_found = 0;

        for (uint8_t i = 0; i < FiltersLen; i++) {
            if (Filters[i].Class == latest_msg.Class && Filters[i].MessageId == latest_msg.MessageId) {
                match_found = 1;
                break;
            };
        }

        if (match_found) {
            *Message = latest_msg;
            break;
        }

        UBX_ReleaseMessage(hubx, &latest_msg);
    }
    return UBX_ERROR_OK;
}

UBX_ErrorTypeDef payload_pool_get_item(UBX_HandleTypeDef *hubx, UBX_PayloadPoolItem **PoolItem, uint8_t *ItemIdx) {
    for (uint8_t i = 0; i < UBX_MSG_PAYLOAD_POOL_SIZE; i++) {
        if (!hubx->PayloadPool[i].InUse) {
            hubx->PayloadPool[i].InUse = 1;
            hubx->PayloadPool[i].Length = UBX_MAX_MSG_PAYLOAD_SIZE;

            *PoolItem = &hubx->PayloadPool[i];
            *ItemIdx = i;
            return UBX_ERROR_OK;
        }
    }
    return UBX_ERROR_POOL_FULL;
}

UBX_ErrorTypeDef payload_pool_release_item(UBX_HandleTypeDef *hubx, uint8_t ItemIdx) {
    if (ItemIdx + 1 > UBX_MSG_PAYLOAD_POOL_SIZE) return UBX_ERROR_POOL_INVALID_IDX;

    hubx->PayloadPool[ItemIdx].InUse = 0;
    hubx->PayloadPool[ItemIdx].Length = 0;
    memset(hubx->PayloadPool[ItemIdx].Payload, 0, sizeof(hubx->PayloadPool[ItemIdx].Payload));

    return UBX_ERROR_OK;
}

UBX_ErrorTypeDef UBX_ReleaseMessage(UBX_HandleTypeDef *hubx, UBX_MessageTypeDef *Message) {
    if (Message == NULL) return UBX_ERROR_MSG_NULL;
    if (Message->PayloadPoolItem == NULL) return UBX_ERROR_PAYLOAD_NULL;
    
    UBX_ErrorTypeDef err = payload_pool_release_item(hubx, Message->PayloadPoolItemIdx);
    if (err == UBX_ERROR_OK) {
        Message->PayloadPoolItem = NULL;
    }
    return err;
}
