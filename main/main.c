#include <esp_timer.h>
#include <driver/uart.h>

#include "app_state.h"

#include "power.h"
#include "log.h"
#include "ubx.h"
#include "gnss.h"
#include "ble.h"
#include "bt.h"

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

    // Start GPS task
    GNSS_Init();

    // Configure and start BLE task
    // BT_Init();
}
