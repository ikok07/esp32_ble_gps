//
// Created by Kok on 2/12/26.
//

#include "led.h"

#include "led_strip.h"
#include "esp_task.h"

#include "app_state.h"
#include "log.h"
#include "tasks_common.h"

#define LED_SUBSCRIBERS_QUEUE_LEN                                   1
#define LED_BRIGHTNESS                                              50

static led_strip_handle_t hled;

static void led_config_task(void *arg);
static void led_task(void *arg);

bool IRAM_ATTR tim_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);

static SCHEDULER_TaskTypeDef gConfigTask = {
    .Active = 0,
    .CoreID = LED_CFG_TASK_CORE_ID,
    .Name = "LED Config Task",
    .Priority = LED_CFG_TASK_PRIORITY,
    .StackDepth = LED_CFG_TASK_STACK_DEPTH,
    .Args = NULL,
    .Function = led_config_task
};

void LED_Init() {
    SCHEDULER_Create(&gConfigTask);
}

void led_config_task(void *arg) {
    *gAppState.htimled = (TIMER_HandleTypeDef){
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

    if (TIMER_Init(gAppState.htimled) != TIMER_ERROR_OK) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to initialize led timer!");
        return;
    };

    if (TIMER_Start(gAppState.htimled) != TIMER_ERROR_OK) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to start led timer!");
        return;
    }

    led_strip_config_t LED_Config = {
        .strip_gpio_num = LED_PIN,
        .max_leds = 1
    };
    led_strip_rmt_config_t RMT_Config = {
        .resolution_hz = 10 * 1000 * 1000 // 10 MHz
    };

    if (led_strip_new_rmt_device(&LED_Config, &RMT_Config, &hled) != ESP_OK) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to initialize LED device!");
        return;
    };

    gAppState.SharedValues->LedLightState = SHVAL_Init(&(SHVAL_ConfigTypeDef){
        .InitialValue = 0,
        .SubscribersQueueSize = 1
    });

    gAppState.SharedValues->LedAutoCycleEnabled = SHVAL_Init(&(SHVAL_ConfigTypeDef){
        .InitialValue = 1,
        .SubscribersQueueSize = 0
    });

    gAppState.Tasks->LedTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .Args = NULL,
        .CoreID = LED_TASK_CORE_ID,
        .Name = "LED Task",
        .Priority = LED_TASK_PRIORITY,
        .StackDepth = LED_TASK_STACK_DEPTH,
        .Function = led_task
    };
    SCHEDULER_Create(&gAppState.Tasks->LedTask);

    // Remove the config task
    SCHEDULER_Remove(&gConfigTask);
}

void led_task(void *arg) {
    LED_ActiveLightTypeDef active_light = LED_ACTIVE_RED;

    while (1) {
        uint32_t auto_cycle = 0;
        SHVAL_ErrorTypeDef shval_err;

        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            if ((shval_err = SHVAL_GetValue(&gAppState.SharedValues->LedAutoCycleEnabled, &auto_cycle, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared LED auto cycle variable! Error code: %d", shval_err);
                continue;
            }

            if ((shval_err = SHVAL_GetValue(&gAppState.SharedValues->LedLightState, (uint32_t*)&active_light, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared LED light state variable! Error code: %d", shval_err);
                continue;
            }

            ESP_ERROR_CHECK(led_strip_set_pixel(
                hled,
                0,
                active_light == LED_ACTIVE_RED ? LED_BRIGHTNESS : 0,
                active_light == LED_ACTIVE_GREEN ? LED_BRIGHTNESS : 0,
                active_light == LED_ACTIVE_BLUE ? LED_BRIGHTNESS : 0
            ));
            ESP_ERROR_CHECK(led_strip_refresh(hled));

            if (auto_cycle) {
                active_light++;
                if (active_light > LED_ACTIVE_BLUE) active_light = LED_ACTIVE_RED;

                if ((shval_err = SHVAL_SetValue(&gAppState.SharedValues->LedLightState, active_light, 1000)) != SHVAL_ERROR_OK) {
                    LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to set shared LED light state variable! Error code: %d", shval_err);
                }
            }
        }
    }

    SCHEDULER_Remove(&gAppState.Tasks->LedTask);
}

bool tim_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    vTaskGenericNotifyGiveFromISR(gAppState.Tasks->LedTask.OsTask, 0, NULL);
    return pdFALSE;
}

char *LED_ActiveLightLabel(LED_ActiveLightTypeDef ActiveLight) {
    switch (ActiveLight) {
        case LED_ACTIVE_RED:
            return "Red";
        case LED_ACTIVE_GREEN:
            return "Green";
        case LED_ACTIVE_BLUE:
            return "Blue";
        default:
            return "Unknown";
    }
}