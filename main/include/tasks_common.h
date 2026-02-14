//
// Created by Kok on 2/12/26.
//

#ifndef ESP32S3_TASKS_COMMON_H
#define ESP32S3_TASKS_COMMON_H

/* ------ CORE 0 ------ */

#define LED_TASK_CORE_ID                    0
#define LED_TASK_STACK_DEPTH                1024
#define LED_TASK_PRIORITY                   4

#define LED_CFG_TASK_CORE_ID                0
#define LED_CFG_TASK_STACK_DEPTH            2048
#define LED_CFG_TASK_PRIORITY               4

/* ------ CORE 1 ------ */

// Driver task
#define BLE_TASK_CORE_ID                    1
#define BLE_TASK_STACK_DEPTH                4096
#define BLE_TASK_PRIORITY                   5

// Application task (config task)
#define BT_CFG_TASK_CORE_ID                 1
#define BT_CFG_TASK_STACK_DEPTH             4096
#define BT_CFG_TASK_PRIORITY                4

#endif //ESP32S3_TASKS_COMMON_H