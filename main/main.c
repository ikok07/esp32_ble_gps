#include <driver/uart.h>

#include "app_state.h"
#include "log.h"
#include "power.h"

#include "gnss.h"
#include "log-config.h"
#include "telemetry-parser.h"
#include "bt.h"
#include "status_led.h"
#include "../components/fs/include/fs.h"

// TODO: Store GNSS data in non-volatile storage
// TODO: Integrate QMC5883L compass module

#define LITTLEFS_PARTITION_NAME             "storage"
#define LITTLEFS_MOUNT_POINT                "/lfs"
#define LITTLE_FS_MAX_PATH_LEN              64

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

    // Configure file system
    gAppState.hfs->Config = (FS_ConfigTypeDef){
        .PartitionName = LITTLEFS_PARTITION_NAME,
        .BasePath = LITTLEFS_MOUNT_POINT,
        .DontMount = 0,
        .FormatIfMountFailed = 1,
        .GrowOnMount = 1,
        .MaxPathLen = LITTLE_FS_MAX_PATH_LEN,
    };
    if (FS_Init(gAppState.hfs)) {
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to mount LittleFS partition!");
        return;
    }

    LOGGER_Log(LOGGER_LEVEL_INFO, "LittleFS partition mounted successfully!");

    size_t total_size = 0, used_size = 0;
    if (FS_GetInfo(gAppState.hfs, &total_size, &used_size)) {
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to get LittleFS usage info!");
        return;
    }

    LOGGER_LogF(LOGGER_LEVEL_INFO, "Usage info => Total: %zu; Used: %zu", total_size, used_size);

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