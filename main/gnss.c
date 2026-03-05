//
// Created by Kok on 3/5/26.
//

#include "gnss.h"

#include <esp_timer.h>

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
void signal();

void gnss_config_task(void *arg);

static uart_port_t gUartPort = UART_NUM_1;
static uint8_t gUartTxPin = UART_PIN_NO_CHANGE;
static uint8_t gUartRxPin = UART_PIN_NO_CHANGE;
static int gUartTxBufferSize = 2048;
static int gUartRxBufferSize = 2048;
static int gUartQueueSize = 1024;
static QueueHandle_t gUartQueue;

static QueueHandle_t gGnssQueue;
static uint8_t gGnssQueueSize = 10;


SCHEDULER_TaskTypeDef gConfigTask = {
    .Active = 0,
    .CoreID = GPS_CFG_TASK_CORE_ID,
    .Name = "GNSS Config task",
    .Priority = GPS_CFG_TASK_PRIORITY,
    .StackDepth = GPS_CFG_TASK_STACK_DEPTH,
    .Args = NULL,
    .Function = gnss_config_task
};

void GPS_Init() {
    SCHEDULER_Create(&gConfigTask);
}

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

    SCHEDULER_Remove(&gConfigTask);
}

uint8_t uart_init(uint32_t BaudRate) {
    esp_err_t err;
    if (uart_is_driver_installed(UART_NUM_1)) return 1;

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
    return uart_set_baudrate(UART_NUM_1, BaudRate);
};

uint8_t uart_flush_rx() {
    return uart_flush(UART_NUM_1);
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
