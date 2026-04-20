//
// Created by Kok on 4/20/26.
//

#include "app_state.h"
#include "telemetry-parser.h"

#include "esp_log_buffer.h"
#include "log.h"
#include "tasks_common.h"

#define TELPARSER_QUEUE_SIZE                         5

void tel_parser_task(void *arg);

void TELPARSER_Init() {
    gAppState.Tasks->TelemetryParserTask = (SCHEDULER_TaskTypeDef){
        .Active = 0,
        .Name = "Telemetry Parser Task",
        .CoreID = TEL_PARSER_TASK_CORE_ID,
        .StackDepth = TEL_PARSER_TASK_STACK_DEPTH,
        .Priority = TEL_PARSER_TASK_PRIORITY,
        .Function = tel_parser_task
    };

    gAppState.Queues->TelemetryParserQueue = xQueueCreate(TELPARSER_QUEUE_SIZE, sizeof(TELPARSER_InputTypeDef));

    SCHEDULER_Create(&gAppState.Tasks->TelemetryParserTask);
}

void tel_parser_task(void *arg) {
    TELPARSER_InputTypeDef input;
    while (1) {
        if (xQueueReceive(gAppState.Queues->TelemetryParserQueue, &input, portMAX_DELAY)) {
            if (input.Class == M10_UBX_CLASS_NAV && input.MessageId == M10_UBX_ID_NAV_PVT) {
                uint16_t year = (input.Payload[5] << 8) | input.Payload[4];
                uint8_t month = input.Payload[6];
                uint8_t day = input.Payload[7];
                uint8_t hour = input.Payload[8];
                uint8_t min = input.Payload[9];
                uint8_t sec = input.Payload[10];
                uint32_t time_acc = (input.Payload[15] << 24) | (input.Payload[14] << 16) | (input.Payload[13] << 8) | input.Payload[12];
                uint8_t fix_type = input.Payload[20];
                double lon = ((input.Payload[27] << 24) | (input.Payload[26] << 16) | (input.Payload[25] << 8) | input.Payload[24]) / 10000000.0;
                double lat = ((input.Payload[31] << 24) | (input.Payload[30] << 16) | (input.Payload[29] << 8) | input.Payload[28]) / 10000000.0;
                double alt = ((input.Payload[39] << 24) | (input.Payload[38] << 16) | (input.Payload[37] << 8) | input.Payload[36]) / 1000.0;
                float speed = ((input.Payload[63] << 24) | (input.Payload[62] << 16) | (input.Payload[61] << 8) | input.Payload[60]) / 1000.0;
                float pdop = ((input.Payload[77] << 8) | input.Payload[76]) / 100.0;

                LOGGER_LogF(LOGGER_LEVEL_INFO, "Year: %d; Month: %d; Day: %d; Hour: %d; Minute: %d; Seconds: %d", year, month, day, hour, min, sec);
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Time accuracy: %d", time_acc);
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Fix type: %d", fix_type);
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Longitude: %.7f; Latitude: %.7f; Altitude: %.7f m", lon, lat, alt);
                LOGGER_LogF(LOGGER_LEVEL_INFO, "Ground speed: %.2f m/s", speed);
                LOGGER_LogF(LOGGER_LEVEL_INFO, "PDOP: %.2f", pdop);
                // TODO: Send over BLE...
            }
        }
    }
}