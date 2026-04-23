#include <driver/uart.h>

#include "app_state.h"
#include "log.h"
#include "power.h"

#include "gnss.h"
#include "log-config.h"
#include "telemetry-parser.h"
#include "bt.h"
#include "status_led.h"

// TODO: Store GNSS data in non-volatile storage
// TODO: Integrate QMC5883L compass module

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

    POWER_LightSleepControl(pdFALSE);

    // Configure Status LED
    STATUSLED_Init();

    // Start telemetry parser task
    TELPARSER_Init();

    // Start GPS task
    GNSS_Init();

    // Configure and start BLE task
    BT_Init();

    while (
        !gAppState.Tasks->GnssUartTask.Active ||
        !gAppState.Tasks->TelemetryParserTask.Active ||
        !gAppState.Tasks->BleTask.Active
    ) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    POWER_LightSleepControl(pdTRUE);
}