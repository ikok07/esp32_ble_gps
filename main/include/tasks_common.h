//
// Created by Kok on 2/12/26.
//

#ifndef ESP32S3_TASKS_COMMON_H
#define ESP32S3_TASKS_COMMON_H

/* ------ CORE 0 ------ */

#define GNSS_UART_TASK_CORE_ID                0
#define GNSS_UART_TASK_STACK_DEPTH            8096
#define GNSS_UART_TASK_PRIORITY               5

#define GNSS_CFG_TASK_CORE_ID                0
#define GNSS_CFG_TASK_STACK_DEPTH            2048
#define GNSS_CFG_TASK_PRIORITY               4

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