//
// Created by Kok on 2/12/26.
//

#include "led.h"

#include "led_strip.h"
#include "esp_task.h"

#include "app_state.h"
#include "log.h"
#include "tasks_common.h"

static led_strip_handle_t hled;
static volatile uint8_t gColorIndex = 0;

static void led_config_task(void *arg);
static void led_task(void *arg);

bool IRAM_ATTR tim_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);

void LED_Init() {
    SCHEDULER_TaskTypeDef config_task = {
        .CoreID = LED_CFG_TASK_CORE_ID,
        .Name = "LED Config Task",
        .Priority = LED_CFG_TASK_PRIORITY,
        .StackDepth = LED_CFG_TASK_STACK_DEPTH,
        .Args = NULL,
        .Function = led_config_task
    };
    SCHEDULER_Create(&config_task);
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

    gAppState.Tasks->LedTask = (SCHEDULER_TaskTypeDef){
        .Args = NULL,
        .CoreID = LED_TASK_CORE_ID,
        .Name = "LED Task",
        .Priority = LED_TASK_PRIORITY,
        .StackDepth = LED_TASK_STACK_DEPTH,
        .Function = led_task
    };
    SCHEDULER_Create(&gAppState.Tasks->LedTask);

    vTaskSuspend(NULL);
}

void led_task(void *arg) {
    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            uint32_t value = 50;
            ESP_ERROR_CHECK(led_strip_set_pixel(
                hled,
                0,
                gColorIndex == 0 ? value : 0,
                gColorIndex == 1 ? value : 0,
                gColorIndex == 2 ? value : 0
            ));
            ESP_ERROR_CHECK(led_strip_refresh(hled));

            gColorIndex++;
            if (gColorIndex > 2) gColorIndex = 0;
        }
    }
}

bool IRAM_ATTR tim_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    vTaskGenericNotifyGiveFromISR(gAppState.Tasks->LedTask.OsTask, 0, NULL);
    return pdFALSE;
}