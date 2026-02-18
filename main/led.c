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

static led_strip_handle_t hled;
static volatile LED_ActiveLightTypeDef currentLight = LED_ACTIVE_RED;

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
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to initialize led timer!");
        return;
    };

    if (TIMER_Start(gAppState.htim) != TIMER_ERROR_OK) {
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
    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            uint32_t value = 50;
            ESP_ERROR_CHECK(led_strip_set_pixel(
                hled,
                0,
                currentLight == LED_ACTIVE_RED ? value : 0,
                currentLight == LED_ACTIVE_GREEN ? value : 0,
                currentLight == LED_ACTIVE_BLUE ? value : 0
            ));
            ESP_ERROR_CHECK(led_strip_refresh(hled));

            SHVAL_ErrorTypeDef shval_err;
            if ((shval_err = SHVAL_SetValue(&gAppState.SharedValues->LedLightState, currentLight, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to set shared led light state! Error code: %d", shval_err);
            };

            currentLight++;
            if (currentLight > LED_ACTIVE_BLUE) currentLight = LED_ACTIVE_RED;
        }
    }

    SCHEDULER_Remove(&gAppState.Tasks->LedTask);
}

bool IRAM_ATTR tim_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
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