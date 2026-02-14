#include "app_state.h"
#include "ble.h"
#include "led_strip.h"
#include "driver/gptimer.h"

#include "tasks_common.h"
#include "power.h"
#include "timer.h"
#include "led.h"
#include "log.h"

bool IRAM_ATTR tim_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);

void app_main(void) {

    // Configure power
    ESP_ERROR_CHECK(POWER_Config());

    *gAppState.htim = (TIMER_HandleTypeDef){
        .htim = (gptimer_handle_t){},
        .Cfg = (TIMER_ConfigTypeDef){
            .Clk = GPTIMER_CLK_SRC_APB,
            .Direction = GPTIMER_COUNT_UP,
            .Resolution_Hz = 1000000,
            .InterruptPrio = 1,
            .Alarm = {
                .TriggerCount = 1000000,
                .AutoReloadOnAlarm = 1,
                .AlarmTriggerCb = tim_callback
            }
        }
    };

    if (TIMER_Init(gAppState.htim) != TIMER_ERROR_OK) {
        while (1);
    };

    if (TIMER_Start(gAppState.htim) != TIMER_ERROR_OK) {
        while (1);
    }

    if (LED_Config() != ESP_OK) {
        while (1);
    }

    LOGGER_Init();
    LOGGER_Enable();
    LOGGER_SetLevel(LOGGER_LEVEL_DEBUG);

    LED_StartTask();

    gAppState.Tasks->BleTask = (SCHEDULER_TaskTypeDef){
        .CoreID = BLE_TASK_CORE_ID,
        .Name = "NimBLE Task",
        .Priority = BLE_TASK_PRIORITY,
        .StackDepth = BLE_TASK_STACK_DEPTH,
        .Args = NULL,
    };

    BLE_HandleTypeDef hble = {
        .BLE_Task = &gAppState.Tasks->BleTask,
        .Config = {
            .DeviceName = "LED Sensor",
            .GapAppearance = 0x02C0, // Sensor appearance
            .AdvertisingIntervalMS = 500,
            .GapRole = BLE_GAP_ROLE_PERIPHERAL,
            .PrivateAddressEnabled = 0
        }
    };

    if (BLE_Init(&hble) != BLE_ERROR_OK) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to initialize BLE!");
        while (1);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "BLE initialized!");

    while (!BLE_CanAdvertise()) {
        LOGGER_Log(LOGGER_LEVEL_INFO, "Waiting for advertising enable!");
    }

    LOGGER_Log(LOGGER_LEVEL_INFO, "BLE can advertise!");

    if (BLE_StartAdvertising() != BLE_ERROR_OK) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to start advertising!");
        while (1);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "BLE started advertising!");
}

bool IRAM_ATTR tim_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    vTaskGenericNotifyGiveFromISR(gAppState.Tasks->LedTask.OsTask, 0, NULL);
    return pdFALSE;
}