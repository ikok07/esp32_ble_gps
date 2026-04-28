//
// Created by Kok on 3/5/26.
//

#include <stdlib.h>
#include "gnss.h"

#include <esp_timer.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "driver/uart.h"

#include "app_state.h"
#include "bt.h"
#include "m10.h"
#include "task_scheduler.h"
#include "tasks_common.h"
#include "log.h"
#include "status_led.h"
#include "telemetry-parser.h"

#define UART_PORT                       UART_NUM_1
#define UART_TX_PIN                     GPIO_NUM_15
#define UART_RX_PIN                     GPIO_NUM_16
#define UART_TX_BUF_SIZE                2048
#define UART_RX_BUF_SIZE                6144
#define UART_QUEUE_SIZE                 10
#define GNSS_UBX_QUEUE_SIZE             5
#define UART_CONFIG_TIMEOUT             1000
#define GNSS_EXPORT_DATA_BUF_LEN        12288        // 12KB
#define GNSS_NAV_DATA_FILE_PATH         "/gnss"
#define GNSS_NAV_DATA_LEN_FILE_PATH     "/gnss-len"
#define GNSS_LAST_POS_FILE_PATH         "/last-pos"

uint8_t uart_init(uint32_t BaudRate);
uint8_t uart_send(uint8_t *Payload, uint32_t Size);
uint8_t uart_set_br(uint32_t BaudRate);
uint8_t uart_flush_rx();
uint8_t flush_ubx_queue();

uint8_t wait_for_msg(UBX_MessageTypeDef *Message, uint32_t TimeoutMs);
uint8_t add_msg(UBX_MessageTypeDef *Message, uint32_t TimeoutMs);
void conn_established_cb(M10_ConnectionInfoTypeDef *ConnInfo);

static void gnss_config_task(void *arg);
static void gnss_uart_task(void *arg);
static void gnss_save_data_task(void *arg);
static void gnss_date_time_upd_task(void *arg);
static void check_gnss_fix_task(void *arg);

static void drain_uart_rx_buf(uint8_t *data, uint32_t data_buff_len, uint32_t *curr_data_len);
static uint8_t parse_uart_nmea_message(uint8_t *data, uint8_t *start_ptr, uint32_t *curr_data_len);
static uint8_t parse_uart_ubx_message(uint8_t *data, uint8_t *start_ptr, uint32_t *curr_data_len);

static uint8_t handle_ubx_msg(uint8_t *Data);
static uint8_t handle_nmea_msg(uint8_t *Data, uint32_t Len);

static uint8_t handle_gnss_data_msg_cb(M10_ExportDataChunkTypeDef *ExportDataChunk);

static void save_gnss_data_timer_cb(TimerHandle_t xTimer);
static void check_gnss_fix_timer_cb(TimerHandle_t xTimer);

static QueueHandle_t gUartQueue;
static QueueHandle_t gGnssUBXQueue;

static SemaphoreHandle_t gUartTaskReady;

static TimerHandle_t gSaveGnssDataTimer;
static TimerHandle_t gCheckGnssFixTimer;

static uint8_t gExportDataBuffer[GNSS_EXPORT_DATA_BUF_LEN] = {0};
static uint32_t gExportDataLen = 0;

static uint8_t gAuxUtcUpdateData[7];

void GNSS_Init() {
    gUartTaskReady = xSemaphoreCreateBinary();

    // Save gnss data every 3 hours
    gSaveGnssDataTimer = xTimerCreate("Save GNSS data timer", pdMS_TO_TICKS(1000 * 60 * 60 * 3), pdTRUE, NULL, save_gnss_data_timer_cb);
    xTimerStart(gSaveGnssDataTimer, pdMS_TO_TICKS(100));

    gAppState.Tasks->GnssConfigTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = GNSS_CFG_TASK_CORE_ID,
        .Name = "GNSS Config task",
        .Priority = GNSS_CFG_TASK_PRIORITY,
        .StackDepth = GNSS_CFG_TASK_STACK_DEPTH,
        .Args = NULL,
        .Function = gnss_config_task
    };

    gAppState.Tasks->GnssSaveDataTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = GNSS_SAVE_DATA_TASK_CORE_ID,
        .Name = "GNSS Save Data Task",
        .Priority = GNSS_SAVE_DATA_TASK_PRIORITY,
        .StackDepth = GNSS_SAVE_DATA_TASK_STACK_DEPTH,
        .Args = NULL,
        .Function = gnss_save_data_task
    };

    gAppState.Tasks->GnssDateTimeUpdTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = GNSS_DATE_TIME_UPD_TASK_CORE_ID,
        .Name = "Module date time update task",
        .Priority = GNSS_DATE_TIME_UPD_TASK_PRIORITY,
        .StackDepth = GNSS_DATE_TIME_UPD_TASK_STACK_DEPTH,
        .Args = NULL,
        .Function = gnss_date_time_upd_task
    };

    gAppState.Tasks->CheckGnssFixTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = GNSS_CHECK_FIX_TASK_CORE_ID,
        .Name = "Check GNSS fix task",
        .Priority = GNSS_CHECK_FIX_TASK_PRIORITY,
        .StackDepth = GNSS_CHECK_FIX_TASK_STACK_DEPTH,
        .Args = NULL,
        .Function = check_gnss_fix_task
    };

    SHVAL_PointerConfigTypeDef shval_config = {
        .InitialValue = gAuxUtcUpdateData,
        .ValueLen = 7,
        .SubscribersEventBits = BT_AUX_UTC_UPDATE_DATA_EVT_BIT
    };
    gAppState.SharedValues->GnssAuxUtcUpdateData = SHVAL_PointerInit(&shval_config);

    SCHEDULER_Create(&gAppState.Tasks->GnssConfigTask);
    SCHEDULER_Create(&gAppState.Tasks->GnssSaveDataTask);
    SCHEDULER_Create(&gAppState.Tasks->GnssDateTimeUpdTask);
    SCHEDULER_Create(&gAppState.Tasks->CheckGnssFixTask);
}

/* ------ Tasks ------ */

void gnss_config_task(void *arg) {
    gGnssUBXQueue = xQueueCreate(GNSS_UBX_QUEUE_SIZE, sizeof(UBX_MessageTypeDef));

    gAppState.Tasks->GnssUartTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .CoreID = GNSS_UART_TASK_CORE_ID,
        .Name = "GNSS UART Task",
        .Priority = GNSS_UART_TASK_PRIORITY,
        .StackDepth = GNSS_CFG_TASK_STACK_DEPTH,
        .Args = NULL,
        .Function = gnss_uart_task
    };

    *gAppState.hm10 = (M10_HandleTypeDef){
        .hubx = {
            .UartConfig = {
                .UartInit = uart_init,
                .UartSend = uart_send,
                .UartSetBaudRate = uart_set_br,
                .UartFlush = uart_flush_rx,
                .UBXFlush = flush_ubx_queue,
                .BaudRate = UBX_BR_115200
            },
            .WaitForMsg = wait_for_msg,
            .SignalNewMsg = add_msg
        },
        .DeviceConfig = {
            .NavModel = M10_NAVMODEL_AUTOMOT,
            .ConfigLayers = M10_CONFIG_LAYER_RAM,
            .Constellations = M10_CONSTELLATION_GPS | M10_CONSTELLATION_GALILEO,
            .UBXOutputMessages = M10_UBX_MSG_NAV_PVT | M10_UBX_MSG_NAV_DOP,
            .UpdateRate = M10_URATE_1HZ,
            .MeasSolutionRatio = 1,
            .PowerConfiguration = M10_PWR_CFG_FULL,
            .PDOP = 250,
            .PositionUpdatePeriodSeconds = 0,                                       // Not used when FULL power mode
            .TimePulse = {
                .Enabled = 1,
                .RisingEdgePolarity = 1,
                .SyncWithGNSS = 1,
                .PeriodMicroSeconds = 1000000,                                      // 1 second
                .PeriodLockedMicroSeconds = 1000000,                                // 1 second
                .PulseLengthMicroSeconds = 250000,                                  // 0.25 seconds
                .PulseLengthLockedMicroSeconds = 250000                             // 0.25 seconds
            }
        }
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "Starting M10 GNSS module initialization...");

    M10_ErrorTypeDef m10_err;
    if ((m10_err = M10_InitUART(gAppState.hm10)) != M10_ERROR_OK) {
        STATUSLED_SetState(STATUSLED_STATE_ERROR_GNSS_CFG);
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to initialize UBX UART! Error code: %d", m10_err);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "M10 GNSS UART configured!");

    SCHEDULER_Create(&gAppState.Tasks->GnssUartTask);

    // Wait for the UART RX Task to settle down
    if (xSemaphoreTake(gUartTaskReady, pdMS_TO_TICKS(UART_CONFIG_TIMEOUT)) == pdFALSE) {
        STATUSLED_SetState(STATUSLED_STATE_ERROR_GNSS_CFG);
        LOGGER_Log(LOGGER_LEVEL_FATAL, "M10 UART config timeout!");
    }

    if ((m10_err = M10_Init(gAppState.hm10)) != M10_ERROR_OK) {
        STATUSLED_SetState(STATUSLED_STATE_ERROR_GNSS_CFG);
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to initialize M10 GNSS module! Error code: %d", m10_err);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "M10 GNSS initialized successfully!");

    // Save baud rate permanently in FLASH config
    if ((m10_err = M10_SetBaudRate(gAppState.hm10, UBX_BR_115200, M10_CONFIG_LAYER_FLASH)) != M10_ERROR_OK) {
        STATUSLED_SetState(STATUSLED_STATE_ERROR_GNSS_CFG);
        LOGGER_LogF(LOGGER_LEVEL_FATAL, "Failed to permanently set baud rate in FLASH config! Error code: %d", m10_err);
    };

    LOGGER_Log(LOGGER_LEVEL_INFO, "M10 GNSS Baud Rate saved in module's flash memory!");
    LOGGER_Log(LOGGER_LEVEL_INFO, "M10 GNSS module configured successfully!");

    gCheckGnssFixTimer = xTimerCreate("Import GNSS data timer", pdMS_TO_TICKS(5000), pdFALSE, NULL, check_gnss_fix_timer_cb);
    xTimerStart(gCheckGnssFixTimer, pdMS_TO_TICKS(100));

    SCHEDULER_Remove(&gAppState.Tasks->GnssConfigTask);
}

void gnss_uart_task(void *arg) {
    uart_event_t event;
    uint32_t curr_data_len = 0;
    uint32_t data_len = UART_RX_BUF_SIZE;
    uint8_t *data = malloc(data_len);

    if (data == NULL) {
        LOGGER_Log(LOGGER_LEVEL_FATAL, "Failed to allocate memory for UART RX buffer!");
        free(data);
        vTaskDelete(NULL);
        return;
    }

    xSemaphoreGive(gUartTaskReady);

    LOGGER_Log(LOGGER_LEVEL_INFO, "M10 GNSS UART task started!");

    while (1) {
        // Wait for incoming messages
        if (xQueueReceive(gUartQueue, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_BUFFER_FULL:
                    // LOGGER_Log(LOGGER_LEVEL_WARNING, "UART RX buffer full!");
                    // break;
                case UART_DATA:
                    // Get RX data
                    drain_uart_rx_buf(data, data_len, &curr_data_len);

                    uint8_t processed = 1;
                    while (processed && curr_data_len > 0) {
                        processed = 0;

                        // Look for the start of frame
                        uint8_t *nmea_ptr = memchr(data, '$', curr_data_len);
                        uint8_t ubx_pattern[2] = {UBX_SYNC_CHAR_ONE, UBX_SYNC_CHAR_TWO};
                        uint8_t *ubx_ptr = memmem(data, curr_data_len, ubx_pattern, sizeof(ubx_pattern));

                        if (nmea_ptr && (!ubx_ptr || nmea_ptr < ubx_ptr)) {
                            uint8_t res = parse_uart_nmea_message(data, nmea_ptr, &curr_data_len);
                            // If the message is finished or corrupted, try with the next stored data. If only partial, read more data
                            if (res == 0 || res == 2) processed = 1;
                        }
                        else if (ubx_ptr) {
                            uint8_t res = parse_uart_ubx_message(data, ubx_ptr, &curr_data_len);
                            // If the message is finished or corrupted, try with the next stored data. If only partial, read more data
                            if (res == 0 || res == 2) processed = 1;
                        }
                        else {
                            // Clear buffer if no valid message start patterns found
                            curr_data_len = 0;
                        }
                    }
                    break;
                default:
                    LOGGER_LogF(LOGGER_LEVEL_WARNING, "Unhandled UART event (type %d). Clearing UART buffers...", event.type);
                    uart_flush_input(UART_PORT);
                    curr_data_len = 0;
                    vTaskDelay(pdMS_TO_TICKS(100));      // Allow other tasks to run
            }
        }
    }

}

void gnss_save_data_task(void *arg) {
    while (1) {
        if (xTaskNotifyWait(0x00, 0xFF, NULL, portMAX_DELAY)) {
            FS_ErrorTypeDef fs_err = FS_ERROR_OK;
            M10_ErrorTypeDef m10_err = M10_ERROR_OK;
            SHVAL_ErrorTypeDef shval_err = SHVAL_ERROR_OK;
            gExportDataLen = 0;

            BT_GnssBaseDataTypeDef base_data;
            if ((shval_err = SHVAL_PointerGetValue(&gAppState.SharedValues->GnssBaseData, &base_data, NULL, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared M10 GNSS base data! Error code: %d", shval_err);
                continue;
            }

            uint8_t last_pos[16];

            last_pos[0] = base_data.Latitude & 0xFF;
            last_pos[1] = (base_data.Latitude >> 8) & 0xFF;
            last_pos[2] = (base_data.Latitude >> 16) & 0xFF;
            last_pos[3] = (base_data.Latitude >> 24) & 0xFF;

            last_pos[4] = base_data.Longitude & 0xFF;
            last_pos[5] = (base_data.Longitude >> 8) & 0xFF;
            last_pos[6] = (base_data.Longitude >> 16) & 0xFF;
            last_pos[7] = (base_data.Longitude >> 24) & 0xFF;

            last_pos[8] = base_data.AltitudeCm & 0xFF;
            last_pos[9] = (base_data.AltitudeCm >> 8) & 0xFF;
            last_pos[10] = (base_data.AltitudeCm >> 16) & 0xFF;
            last_pos[11] = (base_data.AltitudeCm >> 24) & 0xFF;

            last_pos[12] = base_data.HorizontalAccuracyCm & 0xFF;
            last_pos[13] = (base_data.HorizontalAccuracyCm >> 8) & 0xFF;
            last_pos[14] = (base_data.HorizontalAccuracyCm >> 16) & 0xFF;
            last_pos[15] = (base_data.HorizontalAccuracyCm >> 24) & 0xFF;

            if ((fs_err = FS_Write(gAppState.hfs, GNSS_LAST_POS_FILE_PATH, last_pos, 16)) != FS_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to store M10 GNSS last position data in flash storage! Error code: %d", fs_err);
                continue;
            }

            LOGGER_Log(LOGGER_LEVEL_INFO, "Successfully transferred M10 GNSS last position data to flash storage!");

            uint32_t total_chunks = 0;
            if ((m10_err = M10_ExportNavData(gAppState.hm10, handle_gnss_data_msg_cb, &total_chunks, 10000)) != M10_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to export M10 GNSS nav data! Error code: %d", m10_err);
                continue;
            }

            LOGGER_LogF(LOGGER_LEVEL_INFO, "Successfully transferred M10 GNSS nav data to external buffer! Chunks count: %d; Buffer length: %d", total_chunks, gExportDataLen);

            if ((fs_err = FS_Write(gAppState.hfs, GNSS_NAV_DATA_FILE_PATH, gExportDataBuffer, gExportDataLen)) != FS_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to store M10 GNSS nav data in flash storage! Error code: %d", fs_err);
                continue;
            }

            uint8_t data_len[4];
            data_len[0] = gExportDataLen & 0xFF;
            data_len[1] = (gExportDataLen >> 8) & 0xFF;
            data_len[2] = (gExportDataLen >> 16) & 0xFF;
            data_len[3] = (gExportDataLen >> 24) & 0xFF;

            if ((fs_err = FS_Write(gAppState.hfs, GNSS_NAV_DATA_LEN_FILE_PATH, data_len, 4)) != FS_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to store M10 GNSS nav data length in flash storage! Error code: %d", fs_err);
                continue;
            }

            LOGGER_Log(LOGGER_LEVEL_INFO, "Successfully transferred M10 GNSS nav data and its length to flash storage!");
        }
    }
}

void gnss_date_time_upd_task(void *arg) {
    SHVAL_ErrorTypeDef shval_err = SHVAL_ERROR_OK;
    M10_ErrorTypeDef m10_err = M10_ERROR_OK;

    uint8_t date_time_buf[7];
    while (1) {
        if ((shval_err = SHVAL_PointerWaitForValue(&gAppState.SharedValues->GnssAuxUtcUpdateData, BT_AUX_UTC_UPDATE_DATA_EVT_BIT, date_time_buf, NULL, portMAX_DELAY)) == SHVAL_ERROR_OK) {
            M10_DateTimeTypeDef date_time = {
                .Year = (date_time_buf[0]) | (date_time_buf[1] << 8),
                .Month = date_time_buf[2],
                .Day = date_time_buf[3],
                .Hour = date_time_buf[4],
                .Minute = date_time_buf[5],
                .Second = date_time_buf[6],
                .Nanosecond = 0
            };

            LOGGER_LogF(LOGGER_LEVEL_INFO, "New GNSS aux UTC update data received (%d %d %d %d %d %d)", date_time.Year, date_time.Month, date_time.Day, date_time.Hour, date_time.Minute, date_time.Second);

            if (M10_HasValidFix(gAppState.hm10)) {
                LOGGER_Log(LOGGER_LEVEL_INFO, "The module has however correct 3D fix. The data will be discarded.");
                continue;
            }

            if ((m10_err = M10_SetUTC(gAppState.hm10, &date_time, 0, 0, 1000)) != M10_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Failed to set M10's UTC date time! Error code: %d", m10_err);
                continue;
            };

            LOGGER_Log(LOGGER_LEVEL_INFO, "M10's UTC date time successfully set!");
        } else {
            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to wait for shared GNSS aux UTC update data! Error code: %d", shval_err);
        }
    }
}

void check_gnss_fix_task(void *arg) {
    while (1) {
        if (xTaskNotifyWait(0x00, 0xFF, NULL, portMAX_DELAY)) {
            if (M10_HasValidFix(gAppState.hm10)) continue;

            M10_ErrorTypeDef m10_err = M10_ERROR_OK;
            FS_ErrorTypeDef fs_err = FS_ERROR_OK;
            uint8_t gnss_nav_data_exists = FS_Exists(gAppState.hfs, GNSS_NAV_DATA_FILE_PATH);
            uint8_t gnss_last_pos_exists = FS_Exists(gAppState.hfs, GNSS_LAST_POS_FILE_PATH);

            if (gnss_last_pos_exists || gnss_nav_data_exists) {
                M10_GnssStop(gAppState.hm10);

                if (gnss_last_pos_exists) {
                    LOGGER_Log(LOGGER_LEVEL_INFO, "Restoring GNSS last position data from flash storage...");
                    uint8_t last_pos_data[16];
                    if ((fs_err = FS_Read(gAppState.hfs, GNSS_LAST_POS_FILE_PATH, last_pos_data, 16, NULL)) != FS_ERROR_OK) {
                        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to read GNSS last position data from flash! Error code: %d", fs_err);
                    } else {
                        int32_t latitude = (int32_t)((uint32_t)last_pos_data[0] | ((uint32_t)last_pos_data[1] << 8) | ((uint32_t)last_pos_data[2] << 16) | ((uint32_t)last_pos_data[3] << 24));
                        int32_t longitude = (int32_t)((uint32_t)last_pos_data[4] | ((uint32_t)last_pos_data[5] << 8) | ((uint32_t)last_pos_data[6] << 16) | ((uint32_t)last_pos_data[7] << 24));
                        int32_t altitude = (int32_t)((uint32_t)last_pos_data[8] | ((uint32_t)last_pos_data[9] << 8) | ((uint32_t)last_pos_data[10] << 16) | ((uint32_t)last_pos_data[11] << 24));
                        uint32_t h_acc = (int32_t)((uint32_t)last_pos_data[12] | ((uint32_t)last_pos_data[13] << 8) | ((uint32_t)last_pos_data[14] << 16) | ((uint32_t)last_pos_data[15] << 24));

                        if ((m10_err = M10_ImportLastKnownPos(gAppState.hm10, latitude, longitude, altitude, h_acc, 1000)) != M10_ERROR_OK) {
                            LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to import GNSS last position data from flash! Error code: %d", m10_err);
                        } else {
                            LOGGER_Log(LOGGER_LEVEL_INFO, "GNSS last position data restored successfully from flash!");
                        }
                    }
                }

                if (gnss_nav_data_exists) {
                    LOGGER_Log(LOGGER_LEVEL_INFO, "Restoring GNSS nav data from flash storage...");

                    if (!FS_Exists(gAppState.hfs, GNSS_NAV_DATA_LEN_FILE_PATH)) {
                        LOGGER_Log(LOGGER_LEVEL_WARNING, "Nav data length file missing, skipping nav data restore...");
                        goto import_end;
                    }

                    uint8_t nav_data_len_buf[4];
                    if ((fs_err = FS_Read(gAppState.hfs, GNSS_NAV_DATA_LEN_FILE_PATH, nav_data_len_buf, 4, NULL)) != FS_ERROR_OK) {
                        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to read GNSS nav data length from flash! Error code: %d", fs_err);
                        goto import_end;
                    }

                    uint32_t nav_data_len = (nav_data_len_buf[0]) | (nav_data_len_buf[1] << 8) | (nav_data_len_buf[2] << 16) | (nav_data_len_buf[3] << 24);

                    if ((fs_err = FS_Read(gAppState.hfs, GNSS_NAV_DATA_FILE_PATH, gExportDataBuffer, nav_data_len, NULL)) != FS_ERROR_OK) {
                        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to read GNSS nav data from flash! Error code: %d", fs_err);
                        goto import_end;
                    }

                    if ((m10_err = M10_ImportNavData(gAppState.hm10, gExportDataBuffer, nav_data_len, 1000)) != M10_ERROR_OK) {
                        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to import GNSS nav data from flash! Error code: %d", m10_err);
                        goto import_end;
                    }

                    LOGGER_Log(LOGGER_LEVEL_INFO, "GNSS nav data restored successfully from flash!");
                }

        import_end:
                M10_GnssStart(gAppState.hm10);
            }
        }
    }
}

/* ------ Utilities ------ */

/**
 * @brief Transfers all data from the UART registers to the specified buffer
 * @param data Data buffer
 * @param data_buff_len Length of the data buffer
 * @param curr_data_len Length of the filled buffer
 */
void drain_uart_rx_buf(uint8_t *data, uint32_t data_buff_len, uint32_t *curr_data_len) {
    size_t buffered_len;
    do {
        uart_get_buffered_data_len(UART_PORT, &buffered_len);

        if (buffered_len == 0) break;

        size_t space_left = data_buff_len - *curr_data_len;

        size_t to_read = (buffered_len < space_left) ? buffered_len : space_left;

        if (to_read == 0) {
            if (space_left == 0) *curr_data_len = 0;
            else break;
        }

        int read = uart_read_bytes(UART_PORT, data + *curr_data_len, to_read, pdMS_TO_TICKS(20));
        if (read > 0) {
            *curr_data_len += read;
        }
    } while (buffered_len > 0);
}

/**
 * @brief Parses UART data into an NMEA message
 * @param data Data buffer
 * @param start_ptr Pointer to the start of the NMEA message
 * @param curr_data_len Length of the filled buffer
 * @return 0 - Message Parsed; 1 - Not the whole message is in the buffer; 2 - Message in the buffer is corrupted
 */
uint8_t parse_uart_nmea_message(uint8_t *data, uint8_t *start_ptr, uint32_t *curr_data_len) {
    // Move the start of the message to the start of the buffer
    if (start_ptr != data) {
        size_t offset = start_ptr - data;
        memmove(data, start_ptr, *curr_data_len - offset);
        *curr_data_len -= offset;
    }

    uint8_t *end = memmem(data, *curr_data_len, "\r\n", 2);
    if (end == NULL) {
        // NMEA Message isn't finished
        uint8_t *next_start = memchr(data + 1, '$', *curr_data_len - 1);
        if (*curr_data_len > 100 || next_start != NULL) {
            // Discard the current '$' and loop again to try the next one
            memmove(data, data + 1, *curr_data_len - 1);
            (*curr_data_len)--;
            return 2;
        }

        return 1;
    }

    uint32_t nmea_len = (end - data) + 2; // include the \r\n part (2 bytes)
    handle_nmea_msg(data, nmea_len);
    memmove(data, data + nmea_len, *curr_data_len - nmea_len);
    *curr_data_len -= nmea_len;
    return 0;
}

/**
 * @brief Parses UART data into an UBX message
 * @param data Data buffer
 * @param start_ptr Pointer to the start of the UBX message
 * @param curr_data_len Length of the filled buffer
 * @return 0 - Message Parsed; 1 - Not the whole message is in the buffer; 2 - Message in the buffer is corrupted
 */
uint8_t parse_uart_ubx_message(uint8_t *data, uint8_t *start_ptr, uint32_t *curr_data_len) {
    // Move the start of the message to the start of the buffer
    if (start_ptr != data) {
        size_t offset = start_ptr - data;
        memmove(data, start_ptr, *curr_data_len - offset);
        *curr_data_len -= offset;
    }

    uint32_t payload_len = data[4] | (data[5] << 8);
    uint32_t full_msg_len = 8 + payload_len;

    if (*curr_data_len < full_msg_len) {
        return 1;   // Message incomplete
    }

    if (handle_ubx_msg(data) != 0) {
        // UBX Message isn't finished
        if (*curr_data_len > (UART_RX_BUF_SIZE * 75) / 100) {
            // Invalid UBX message
            memmove(data, data + 1, *curr_data_len - 1);
            (*curr_data_len)--;
            return 2;
        }

        return 1;
    }

    memmove(data, data + full_msg_len, *curr_data_len - full_msg_len);
    *curr_data_len -= full_msg_len;
    return 0;
}

/**
 * @param Data UART message
 * @param Len Length of UART message
 * @return 0 - OK; 1 - Invalid message
 */
uint8_t handle_ubx_msg(uint8_t *Data) {
    UBX_ErrorTypeDef ubx_err;
    UBX_MessageTypeDef ubx_message;

    if ((ubx_err = UBX_ParseMessage(&gAppState.hm10->hubx, Data, &ubx_message)) != UBX_ERROR_OK) {
        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to parse UBX message! Error code: %d", ubx_err);
        return 1;
    };

    TELPARSER_InputTypeDef telparser_input = {
        .Class = ubx_message.Class,
        .MessageId = ubx_message.MessageId,
        .Length = ubx_message.Length
    };

    uint32_t telparser_payload_size = sizeof(telparser_input.Payload) / sizeof(telparser_input.Payload[0]);
    if (telparser_payload_size < telparser_input.Length) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Telemetry parser's payload doesn't have enough space for the ubx message!");
        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Class: %d; Message ID: %d; Length: %d;", telparser_input.Class, telparser_input.MessageId, telparser_input.Length);
        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Telemetry parser's payload size: %d", telparser_payload_size);
    } else if (ubx_message.PayloadPoolItem == NULL) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "UBX Message's payload is NULL");
    } else {
        memcpy(telparser_input.Payload, ubx_message.PayloadPoolItem->Payload, telparser_input.Length);
    }

    if (!xQueueSend(gAppState.Queues->TelemetryParserQueue, &telparser_input, pdMS_TO_TICKS(10))) {
        LOGGER_Log(LOGGER_LEVEL_ERROR, "Failed to send telemetry parser input!");
    }

    M10_SignalMessageReceived(gAppState.hm10, M10_MSG_TYPE_UBX, &ubx_message);
    return 0;
}

/**
 * @param Data UART message
 * @param Len Length of UART message
 * @return 0 - OK; 1 - Invalid message
 */
uint8_t handle_nmea_msg(uint8_t *Data, uint32_t Len) {
    if (Data[Len - 2] != '\r' || Data[Len - 1] != '\n') return 1;

    LOGGER_LogF(LOGGER_LEVEL_INFO, "NMEA Message: %.*s", (int)Len - 2, Data);

    M10_SignalMessageReceived(gAppState.hm10, M10_MSG_TYPE_NMEA, NULL);
    return 0;
}

uint8_t handle_gnss_data_msg_cb(M10_ExportDataChunkTypeDef *ExportDataChunk) {
    if ((gExportDataLen + ExportDataChunk->Len) > GNSS_EXPORT_DATA_BUF_LEN) {
        LOGGER_LogF(LOGGER_LEVEL_ERROR, "GNSS Export data could not fit into gExportDataBuffer! Current length: %d; Length after append: %d", gExportDataLen, gExportDataLen + ExportDataChunk->Len);
        return 1;
    }

    memcpy(&gExportDataBuffer[gExportDataLen], ExportDataChunk->Data, ExportDataChunk->Len);
    gExportDataLen += ExportDataChunk->Len;
    return 0;
}

void save_gnss_data_timer_cb(TimerHandle_t xTimer) {
    xTaskNotifyGive(gAppState.Tasks->GnssSaveDataTask.OsTask);
}

void check_gnss_fix_timer_cb(TimerHandle_t xTimer) {
    xTaskNotifyGive(gAppState.Tasks->CheckGnssFixTask.OsTask);
}

/* ------ Application specific methods ------ */

uint8_t uart_init(uint32_t BaudRate) {
    esp_err_t err;
    if (uart_is_driver_installed(UART_PORT)) {
        uart_driver_delete(UART_PORT);
    };

    if ((err = uart_driver_install(
        UART_PORT,
        UART_RX_BUF_SIZE,
        UART_TX_BUF_SIZE,
        UART_QUEUE_SIZE,
        &gUartQueue,
        0
    )) != ESP_OK) {
        return 1;
    }

    uart_config_t UART_Config = {
        .baud_rate = BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_XTAL
    };
    if ((err = uart_param_config(UART_PORT, &UART_Config)) != ESP_OK) return 1;

    if ((err = uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) != ESP_OK) {
        return 1;
    }

    return 0;
}

uint8_t uart_send(uint8_t *Payload, uint32_t Size) {
    int err;
    if ((err = uart_write_bytes(UART_PORT, Payload, Size)) < 0) {
        return 1;
    };
    return 0;
}

uint8_t uart_set_br(uint32_t BaudRate) {
    return uart_set_baudrate(UART_PORT, BaudRate) == 0 ? 0 : 1;
};

uint8_t uart_flush_rx() {
    return uart_flush(UART_PORT) == 0 ? 0 : 1;
}

uint8_t flush_ubx_queue() {
    UBX_ErrorTypeDef ubx_err = UBX_ERROR_OK;
    UBX_MessageTypeDef message;
    while (xQueueReceive(gGnssUBXQueue, &message, 0)) {
        if ((ubx_err = UBX_ReleaseMessage(&gAppState.hm10->hubx, &message)) != UBX_ERROR_OK) return ubx_err;
    }
    return ubx_err;
}

uint8_t wait_for_msg(UBX_MessageTypeDef *Message, uint32_t TimeoutMs) {
    return xQueueReceive(gGnssUBXQueue, Message, pdMS_TO_TICKS(TimeoutMs)) == pdFALSE;
}

uint8_t add_msg(UBX_MessageTypeDef *Message, uint32_t TimeoutMs) {
    return xQueueSend(gGnssUBXQueue, Message, pdMS_TO_TICKS(TimeoutMs)) == pdFALSE;
}

void conn_established_cb(M10_ConnectionInfoTypeDef *ConnInfo) {
    LOGGER_Log(LOGGER_LEVEL_INFO, "Successfully connected to M10 GNSS module!");
    LOGGER_LogF(LOGGER_LEVEL_INFO, "Baud rate: %d", ConnInfo->BaudRate);
    LOGGER_LogF(LOGGER_LEVEL_INFO, "Hardware version: %d", ConnInfo->Version.HwVersion);
    LOGGER_LogF(LOGGER_LEVEL_INFO, "Software version: %d", ConnInfo->Version.SwVersion);
}

uint32_t UBX_GetTickMsCB() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void UBX_WaitForMsCB(uint32_t Ms) {
    vTaskDelay(pdMS_TO_TICKS(Ms));
}