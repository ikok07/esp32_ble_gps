//
// Created by Kok on 3/14/26.
//

#include "log-config.h"

#include "esp_log.h"
#include "esp_system.h"

#include "log.h"

static uint8_t on_log(LOGGER_EventTypeDef *Event);
static uint8_t on_fatal(LOGGER_EventTypeDef *Event);
static uint8_t on_format(LOGGER_EventTypeDef *Event, char *Buffer, uint16_t Len);

static LOGGER_CallbacksTypeDef callbacks = {
    .on_log = on_log,
    .on_fatal_err = on_fatal,
    .optional_on_format = on_format
};

void LOG_Configure() {
    LOGGER_RegisterCB(&callbacks);
    LOGGER_Init();
    LOGGER_Enable();
    LOGGER_SetLevel(LOGGER_LEVEL_DEBUG);
}

uint8_t on_log(LOGGER_EventTypeDef *Event) {
    switch (Event->Level) {
        case LOGGER_LEVEL_INFO:
            ESP_LOGI("logger", "%s", Event->msg);
            break;
        case LOGGER_LEVEL_WARNING:
            ESP_LOGW("logger", "%s", Event->msg);
            break;
        case LOGGER_LEVEL_DEBUG:
            ESP_LOGD("logger", "%s", Event->msg);
            break;
        case LOGGER_LEVEL_ERROR:
            ESP_LOGE("logger", "%s", Event->msg);
            break;
        case LOGGER_LEVEL_FATAL:
            ESP_LOGE("logger", "%s", Event->msg);
            esp_restart();
            break;
        default:
            break;
    }
    return 0;
}

uint8_t on_fatal(LOGGER_EventTypeDef *Event) {
    ESP_LOGE("logger", "%s", Event->msg);
    esp_restart();
    return 0;
}

uint8_t on_format(LOGGER_EventTypeDef *Event, char *Buffer, uint16_t Len) {
    return snprintf(Buffer, Len, "%s", Event->msg) >= Len;
}