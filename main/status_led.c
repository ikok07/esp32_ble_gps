//
// Created by Kok on 3/10/26.
//

#include "status_led.h"

#include "app_state.h"
#include "task_scheduler.h"
#include "tasks_common.h"
#include "led_strip.h"
#include "log.h"

static void status_led_task(void *arg);

void get_color_for_state(STATUSLED_StateTypeDef State, uint8_t *Red, uint8_t *Green, uint8_t *Blue);

static QueueHandle_t gStateQueue;

/* ------ Public methods ------ */
void STATUSLED_Init() {
    esp_err_t esp_err;
    led_strip_config_t led_config = {
        .strip_gpio_num = STATUS_LED_GPIO,
        .max_leds = 1
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000 * 1000 * 10           // 10 MHz
    };

    if ((esp_err = led_strip_new_rmt_device(&led_config, &rmt_config, gAppState.hstatusled)) != ESP_OK) {
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to initialize status LED driver! Error code: %d", esp_err);
    };

    gAppState.Tasks->StatusLedTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = STATUS_LED_TASK_CORE_ID,
        .Name = "Status LED task",
        .Priority = STATUS_LED_TASK_PRIORITY,
        .StackDepth = STATUS_LED_TASK_STACK_DEPTH,
        .Function = status_led_task
    };

    gStateQueue = xQueueCreate(1, sizeof(STATUSLED_StateTypeDef));

    if ((esp_err = led_strip_clear(*gAppState.hstatusled)) != ESP_OK) {
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to clear LED strip! Error code: %d", esp_err);
    };

    SCHEDULER_Create(&gAppState.Tasks->StatusLedTask);
}

void STATUSLED_SetState(STATUSLED_StateTypeDef State) {
    if (gStateQueue) {
        xQueueOverwrite(gStateQueue, &State);
    }
}

/* ------ Tasks ------ */
void status_led_task(void *arg) {
    esp_err_t esp_err;
    uint8_t red, green, blue;
    STATUSLED_StateTypeDef state;

    LOGGER_Log(LOGGER_LEVEL_INFO, "Status LED Task started");

    while (1) {
        // Wait for status update
        xQueueReceive(gStateQueue, &state, portMAX_DELAY);
        get_color_for_state(state, &red, &green, &blue);

        if ((esp_err = led_strip_set_pixel(*gAppState.hstatusled, 0, (red * STATUS_LED_BRIGHTNESS) / 100, (green * STATUS_LED_BRIGHTNESS) / 100, (blue * STATUS_LED_BRIGHTNESS) / 100)) != ESP_OK) {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to set LED color! Error code: %d", esp_err);
            continue;
        };

        if ((esp_err = led_strip_refresh(*gAppState.hstatusled)) != ESP_OK) {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to refresh LED! Error code: %d", esp_err);
        };

        LOGGER_LogF(LOGGER_LEVEL_INFO, "Status LED set to R: %d G: %d B: %d", red, green, blue);
    }
}

/* ------ Utils ------ */
void get_color_for_state(STATUSLED_StateTypeDef State, uint8_t *Red, uint8_t *Green, uint8_t *Blue) {
    switch (State) {
        case STATUSLED_STATE_CONFIGURING:
            *Red = 255;
            *Green = 159;
            *Blue = 1;
            break;
        case STATUSLED_STATE_ERROR_GNSS_CFG:
            *Red = 255;
            *Green = 0;
            *Blue = 0;
            break;
        case STATUSLED_STATE_ERROR_BT_CFG:
            *Red = 97;
            *Green = 120;
            *Blue = 255;
            break;
        case STATUSLED_STATE_READY_TO_CONNECT:
            *Red = 255;
            *Green = 255;
            *Blue = 255;
            break;
        case STATUSLED_STATE_CONNECTED:
            *Red = 0;
            *Green = 255;
            *Blue = 0;
            break;
    }
}