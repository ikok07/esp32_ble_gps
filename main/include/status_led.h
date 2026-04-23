//
// Created by Kok on 3/10/26.
//

#ifndef ESP32S3_BLE_STATUS_LIGHT_H
#define ESP32S3_BLE_STATUS_LIGHT_H

#include "led_strip.h"
#include "soc/gpio_num.h"

#define STATUS_LED_GPIO                             (GPIO_NUM_48)
#define STATUS_LED_BRIGHTNESS                       (10)                   // 0 - 100

typedef enum {
    STATUSLED_STATE_CONFIGURING,
    STATUSLED_STATE_ERROR_GNSS_CFG,
    STATUSLED_STATE_ERROR_BT_CFG,
    STATUSLED_STATE_READY_TO_CONNECT,
    STATUSLED_STATE_CONNECTED
} STATUSLED_StateTypeDef;

void STATUSLED_Init();
void STATUSLED_SetState(STATUSLED_StateTypeDef State);

#endif //ESP32S3_BLE_STATUS_LIGHT_H