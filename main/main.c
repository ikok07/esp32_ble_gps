#include <esp_timer.h>
#include <driver/uart.h>

#include "app_state.h"
#include "ble.h"
#include "bt.h"
#include "led_strip.h"

#include "power.h"
#include "log.h"

#include "ubx.h"

// void uart_init(uint32_t BaudRate) {
//     esp_err_t err;
//     if (uart_is_driver_installed(UART_NUM_1)) return UBX_ERROR_UART_USED;
//
//     uart_config_t UART_Config = {
//         .baud_rate = BaudRate,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
//     };
//     if ((err = uart_param_config(hubx->UartConfig.UartPort, &UART_Config)) != ESP_OK) return UBX_ERROR_UART_CONFIG;
//
//     if ((err = uart_set_pin(hubx->UartConfig.UartPort, hubx->UartConfig.TxPin, hubx->UartConfig.RxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) != ESP_OK) {
//         return UBX_ERROR_UART_PIN;
//     }
//
//     if ((err = uart_driver_install(
//         hubx->UartConfig.UartPort,
//         hubx->UartConfig.RxBufferSize,
//         hubx->UartConfig.TxBufferSize,
//         hubx->UartConfig.UartQueueSize,
//         &hubx->UartConfig.UartQueue,
//         0
//     )) != ESP_OK) {
//         return UBX_ERROR_UART_DRIVER_INSTALL;
//     }
//
//     return UBX_ERROR_OK;
// }

// uint8_t uart_send(uint8_t *Payload, uint32_t Size) {
//     uint8_t err;
//     if ((err = uart_write_bytes(hubx->UartConfig.UartPort, Payload, Size)) < 0) {
//         return UBX_ERROR_TX;
//     };
// }

// uint8_t uart_set_br(uint32_t BaudRate) {
//     return uart_set_baudrate(UART_NUM_1, BaudRate);
// };

uint8_t uart_flush_rx() {
    return uart_flush(UART_NUM_1);
}

void app_main(void) {
    // Initialize app state
    APP_Init();

    // Configure logger
    LOGGER_Init();
    LOGGER_Enable();
    LOGGER_SetLevel(LOGGER_LEVEL_DEBUG);

    // Configure power
    if (POWER_Config() != ESP_OK) {
        LOGGER_Log(LOGGER_LEVEL_FATAL, "Failed to configure board power!");
        return;
    }

    // Configure and start BLE task
    // BT_Init();
}

uint32_t UBX_GetTickMsCB() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
