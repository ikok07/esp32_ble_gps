//
// Created by Kok on 2/12/26.
//

#ifndef ESP32S3_TIMER_H
#define ESP32S3_TIMER_H

#include "driver/gptimer.h"

typedef enum {
    TIMER_ERROR_OK,
    TIMER_ERROR_UNINITIALIZED,
    TIMER_ERROR_NOT_AVAIL,
    TIMER_ERROR_ALARM_CFG,
    TIMER_ERROR_ALARM_CB_CFG,
    TIMER_ERROR_ENABLE,
    TIMER_ERROR_DISABLE,
    TIMER_ERROR_RESET
} TIMER_ErrorTypeDef;

typedef struct {
    uint32_t TriggerCount;
    uint8_t AutoReloadOnAlarm;
    gptimer_alarm_cb_t AlarmTriggerCb;
} TIMER_AlarmConfigTypeDef;

typedef struct {
    gptimer_clock_source_t Clk;
    uint32_t Resolution_Hz;                    // Period = 1 / Resolution_Hz
    uint32_t InterruptPrio;
    gptimer_count_direction_t Direction;
    TIMER_AlarmConfigTypeDef Alarm;
} TIMER_ConfigTypeDef;

typedef struct {
    uint8_t Initialized;
    uint8_t Enabled;
    gptimer_handle_t htim;
    TIMER_ConfigTypeDef Cfg;
} TIMER_HandleTypeDef;

TIMER_ErrorTypeDef TIMER_Init(TIMER_HandleTypeDef *htimer);
TIMER_ErrorTypeDef TIMER_Start(TIMER_HandleTypeDef *htimer);
TIMER_ErrorTypeDef TIMER_Stop(TIMER_HandleTypeDef *htimer);
TIMER_ErrorTypeDef TIMER_Reset(TIMER_HandleTypeDef *htimer);

#endif //ESP32S3_TIMER_H