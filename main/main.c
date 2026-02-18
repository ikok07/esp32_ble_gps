#include "app_state.h"
#include "ble.h"
#include "bt.h"
#include "led_strip.h"

#include "power.h"
#include "led.h"
#include "log.h"

// TODO 1: Implement BLE bonding
// TODO 1.1: Implement passkey event
// TODO 1.2: Configure private address

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

    // Configure and start LED task
    LED_Init();

    // Configure and start BLE task
    BT_Init();
}