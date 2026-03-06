//
// Created by Kok on 3/5/26.
//

#include "gnss.h"

#include <esp_timer.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "driver/uart.h"

#include "app_state.h"
#include "m10.h"
#include "task_scheduler.h"
#include "tasks_common.h"
#include "log.h"
uint8_t uart_init(uint32_t BaudRate);
uint8_t uart_send(uint8_t *Payload, uint32_t Size);
uint8_t uart_set_br(uint32_t BaudRate);
uint8_t uart_flush_rx();

uint8_t wait_for_msg(UBX_MessageTypeDef *Message, uint32_t TimeoutMs);
uint8_t add_msg(UBX_MessageTypeDef *Message, uint32_t TimeoutMs);

static void gnss_config_task(void *arg);
static void gnss_uart_task(void *arg);

static uint8_t handle_ubx_msg(uint8_t *Data, uint32_t Len);
static uint8_t handle_nmea_msg(uint8_t *Data, uint32_t Len);

static uart_port_t gUartPort = UART_NUM_1;
static uint8_t gUartTxPin = UART_PIN_NO_CHANGE;
static uint8_t gUartRxPin = UART_PIN_NO_CHANGE;
static int gUartTxBufferSize = 2048;
static int gUartRxBufferSize = 4096;
static int gUartQueueSize = 10;
static QueueHandle_t gUartQueue;

static QueueHandle_t gGnssQueue;
static uint8_t gGnssQueueSize = 10;

SCHEDULER_TaskTypeDef gConfigTask = {
    .Active = 0,
    .CoreID = GNSS_CFG_TASK_CORE_ID,
    .Name = "GNSS Config task",
    .Priority = GNSS_CFG_TASK_PRIORITY,
    .StackDepth = GNSS_CFG_TASK_STACK_DEPTH,
    .Args = NULL,
    .Function = gnss_config_task
};

SCHEDULER_TaskTypeDef gUartTask = {
    .Active = 0,
    .CoreID = GNSS_UART_TASK_CORE_ID,
    .Name = "GNSS UART Task",
    .Priority = GNSS_UART_TASK_PRIORITY,
    .StackDepth = GNSS_CFG_TASK_STACK_DEPTH,
    .Args = NULL,
    .Function = gnss_uart_task
};

void GPS_Init() {
    SCHEDULER_Create(&gConfigTask);
}

/* ------ Tasks ------ */

void gnss_config_task(void *arg) {
    gGnssQueue = xQueueCreate(gGnssQueueSize, sizeof(UBX_MessageTypeDef));

    *gAppState.hm10 = (M10_HandleTypeDef){
        .hubx = {
            .UartConfig = {
                .UartInit = uart_init,
                .UartSend = uart_send,
                .UartSetBaudRate = uart_set_br,
                .UartFlush = uart_flush_rx
            },
            .WaitForMsg = wait_for_msg,
            .SignalNewMsg = add_msg
        },
        .DeviceConfig = {
            .BaudRate = M10_BRATE_115200,
            .NavModel = M10_NAVMODEL_AUTOMOT,
            .ConfigLayers = M10_CONFIG_LAYER_BBR,
            .Constellations = M10_CONSTELLATION_GPS,
            .NMEAOutputMessages = M10_NMEA_MSG_STD_RMC,
            .UpdateRate = M10_URATE_10HZ,
            .PowerConfiguration = M10_PWR_CFG_FULL
        }
    };

    M10_ErrorTypeDef m10_err;
    if ((m10_err = M10_Init(gAppState.hm10)) != M10_ERROR_OK) {
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to initialize M10 GNSS module! Error code: %d", m10_err);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "M10 GNSS module configured successfully!");

    SCHEDULER_Create(&gUartTask);
    SCHEDULER_Remove(&gConfigTask);
}

void gnss_uart_task(void *arg) {
    uart_event_t event;
    uint32_t data_len = gUartRxBufferSize / 4;
    uint32_t curr_data_len = 0;
    uint8_t data[data_len];

    while (1) {
        // Wait for incoming message
        if (xQueueReceive(gUartQueue, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    if (curr_data_len + event.size > data_len) {
                        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Received UART message could not fit in the allocated buffer. Buffer size: %d. Message size: %d", data_len, event.size);
                        break;
                    }
                    uint32_t received = uart_read_bytes(gUartPort, data + curr_data_len, event.size, portMAX_DELAY);
                    curr_data_len += received;

                    if (curr_data_len >= 2 && data[0] == UBX_SYNC_CHAR_ONE && data[1] == UBX_SYNC_CHAR_TWO) {
                        // UBX message
                        if (handle_ubx_msg(data, curr_data_len) == 0) {
                            // Message was valid
                            curr_data_len = 0;
                        };
                    } else if (data[0] == '$') {
                        // NMEA message
                        if (handle_nmea_msg(data, curr_data_len) == 0) {
                            // Message was valid
                            curr_data_len = 0;
                        };
                    } else if (curr_data_len >= 2) {
                        // Invalid message - discard
                        curr_data_len = 0;
                    }

                    break;
                default:
                    LOGGER_LogF(LOGGER_LEVEL_INFO, "Unhandled event: %d", event.type);
            }
        }
    }
}


/* ------ Utilities ------ */

/**
 * @param Data UART message
 * @param Len Length of UART message
 * @return 0 - OK; 1 - Invalid message
 */
uint8_t handle_ubx_msg(uint8_t *Data, uint32_t Len) {
    UBX_ErrorTypeDef ubx_err;
    UBX_MessageTypeDef ubx_message;

    uint32_t payload_len = Data[4] | (Data[5] << 8);
    uint32_t full_msg_len = 8 + payload_len;

    if (Len != full_msg_len) return 1;

    if ((ubx_err = UBX_ParseMessage(&gAppState.hm10->hubx, Data, &ubx_message)) != UBX_ERROR_OK) {
        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to parse UBX message! Error code: %d", ubx_err);
        return 1;
    };
    UBX_HandleNewMessage(&gAppState.hm10->hubx, &ubx_message);
    return 0;
}

/**
 * @param Data UART message
 * @param Len Length of UART message
 * @return 0 - OK; 1 - Invalid message
 */
uint8_t handle_nmea_msg(uint8_t *Data, uint32_t Len) {
    // TODO: Send NMEA via BLE...
    if (Data[Len - 2] != '\r' || Data[Len - 1] != '\n') return 1;

    LOGGER_LogF(LOGGER_LEVEL_INFO, "NMEA Message: %.*s", (int)Len, Data);
    return 0;
}

/* ------ Application specific methods ------ */

uint8_t uart_init(uint32_t BaudRate) {
    esp_err_t err;
    if (uart_is_driver_installed(UART_NUM_1)) {
        uart_driver_delete(gUartPort);
    };

    uart_config_t UART_Config = {
        .baud_rate = BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    if ((err = uart_param_config(gUartPort, &UART_Config)) != ESP_OK) return 1;

    if ((err = uart_set_pin(gUartPort, gUartTxPin, gUartRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) != ESP_OK) {
        return 1;
    }

    if ((err = uart_driver_install(
        gUartPort,
        gUartRxBufferSize,
        gUartTxBufferSize,
        gUartQueueSize,
        &gUartQueue,
        0
    )) != ESP_OK) {
        return 1;
    }

    return 0;
}

uint8_t uart_send(uint8_t *Payload, uint32_t Size) {
    uint8_t err;
    if ((err = uart_write_bytes(gUartPort, Payload, Size)) < 0) {
        return 1;
    };
    return 0;
}

uint8_t uart_set_br(uint32_t BaudRate) {
    return uart_set_baudrate(gUartPort, BaudRate) == 0 ? 0 : 1;
};

uint8_t uart_flush_rx() {
    return uart_flush(gUartPort) == 0 ? 0 : 1;
}

uint8_t wait_for_msg(UBX_MessageTypeDef *Message, uint32_t TimeoutMs) {
    return xQueueReceive(gGnssQueue, Message, pdMS_TO_TICKS(TimeoutMs)) == pdFALSE;
}

uint8_t add_msg(UBX_MessageTypeDef *Message, uint32_t TimeoutMs) {
    return xQueueSend(gGnssQueue, Message, pdMS_TO_TICKS(TimeoutMs)) == pdFALSE;
}

uint32_t UBX_GetTickMsCB() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}