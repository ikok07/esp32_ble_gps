//
// Created by Kok on 2/13/26.
//

#include "esp_log.h"
#include "esp_system.h"

#include "log.h"

uint8_t LOGGER_LogCB(LOGGER_EventTypeDef *Event) {
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

uint8_t LOGGER_FatalCB(LOGGER_EventTypeDef *Event) {
    ESP_LOGE("logger", "%s", Event->msg);
    esp_restart();
    return 0;
}

uint8_t LOGGER_FormatCB(LOGGER_EventTypeDef *Event, char *Buffer, uint16_t Len) {
    return snprintf(Buffer, Len, "%s", Event->msg) >= Len;
}