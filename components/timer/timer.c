//
// Created by Kok on 2/12/26.
//

#include "include/timer.h"

TIMER_ErrorTypeDef TIMER_Init(TIMER_HandleTypeDef *htimer) {
    if (htimer->Initialized) TIMER_Stop(htimer);
    htimer->Enabled = 0;

    gptimer_config_t TIMER_Config = {
        .clk_src = htimer->Cfg.Clk,
        .resolution_hz = htimer->Cfg.Resolution_Hz,
        .intr_priority = htimer->Cfg.InterruptPrio,
        .direction = htimer->Cfg.Direction,
    };
    gptimer_alarm_config_t TIMER_AlarmConfig = {
        .reload_count = 0,
        .alarm_count = htimer->Cfg.Alarm.TriggerCount,
        .flags = {.auto_reload_on_alarm = htimer->Cfg.Alarm.AutoReloadOnAlarm}
    };
    gptimer_event_callbacks_t TIMER_Cbs = {
        .on_alarm = htimer->Cfg.Alarm.AlarmTriggerCb
    };

    esp_err_t err = ESP_OK;

    if ((err = gptimer_new_timer(&TIMER_Config, &htimer->htim)) != ESP_OK) return TIMER_ERROR_NOT_AVAIL;
    if ((err = gptimer_set_alarm_action(htimer->htim, &TIMER_AlarmConfig)) != ESP_OK) return TIMER_ERROR_ALARM_CFG;
    if ((err = gptimer_register_event_callbacks(htimer->htim, &TIMER_Cbs, NULL)) != ESP_OK) return TIMER_ERROR_ALARM_CB_CFG;

    htimer->Initialized = 1;

    return TIMER_ERROR_OK;
}

TIMER_ErrorTypeDef TIMER_Start(TIMER_HandleTypeDef *htimer) {
    esp_err_t err = ESP_OK;

    if (!htimer->Initialized) return TIMER_ERROR_UNINITIALIZED;
    if (htimer->Enabled) return err;

    if ((err = gptimer_enable(htimer->htim)) != ESP_OK) return TIMER_ERROR_ENABLE;
    if ((err = gptimer_start(htimer->htim)) != ESP_OK) return TIMER_ERROR_ENABLE;

    return TIMER_ERROR_OK;
}

TIMER_ErrorTypeDef TIMER_Stop(TIMER_HandleTypeDef *htimer) {
    esp_err_t err = ESP_OK;

    if (!htimer->Initialized) return TIMER_ERROR_UNINITIALIZED;
    if (!htimer->Enabled) return err;

    if ((err = gptimer_stop(htimer->htim)) != ESP_OK) return TIMER_ERROR_ENABLE;
    if ((err = gptimer_disable(htimer->htim)) != ESP_OK) return TIMER_ERROR_ENABLE;

    return TIMER_ERROR_OK;
}

TIMER_ErrorTypeDef TIMER_Reset(TIMER_HandleTypeDef *htimer) {
    return gptimer_set_raw_count(htimer->htim, 0) == ESP_OK ? TIMER_ERROR_OK : TIMER_ERROR_RESET;
}