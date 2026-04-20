#include <driver/uart.h>

#include "app_state.h"

#include "power.h"
#include "log.h"
#include "gnss.h"
#include "log-config.h"
#include "telemetry-parser.h"

// TODO: Replace NMEA messages with UBX-NAV-PVT message

void app_main(void) {
    // Initialize app state
    APP_Init();

    // Configure logger
    LOG_Configure();

    // Configure power
    if (POWER_Config() != ESP_OK) {
        LOGGER_Log(LOGGER_LEVEL_FATAL, "Failed to configure board power!");
        return;
    }

    // Start telemetry parser task
    TELPARSER_Init();

    // Start GPS task
    GNSS_Init();

    // Configure and start BLE task
    // BLE driver to be installed...
    // BT_Init();
}