//
// Created by Kok on 4/20/26.
//

#include "app_state.h"
#include "telemetry-parser.h"

#include "bt.h"
#include "log.h"
#include "tasks_common.h"

#define TELPARSER_QUEUE_SIZE                         5

static BT_GnssBaseDataTypeDef gGnssBaseData = {0};
static BT_GnssPrecisionDataTypeDef gGnssPrecisionData = {0};

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

    SHVAL_PointerConfigTypeDef shval_config = {
        .InitialValue = &gGnssBaseData,
        .SubscribersEventBits = (
            BT_BASE_DATA_LOC_AND_SPD_EVT_BIT |
            BT_BASE_DATA_ELEVATION_EVT_BIT |
            BT_BASE_DATA_DATE_TIME_EVT_BIT
        ),
        .ValueLen = sizeof(gGnssBaseData)
    };
    gAppState.SharedValues->GnssBaseData = SHVAL_PointerInit(&shval_config);

    shval_config.InitialValue = &gGnssPrecisionData;
    shval_config.SubscribersEventBits = BT_PRECISION_DATA_GNSS_FIX_EVT_BIT;
    shval_config.ValueLen = sizeof(gGnssPrecisionData);
    gAppState.SharedValues->GnssPrecisionData = SHVAL_PointerInit(&shval_config);

    SCHEDULER_Create(&gAppState.Tasks->TelemetryParserTask);
}

void tel_parser_task(void *arg) {
    TELPARSER_InputTypeDef input;
    M10_DeviceFixTypeDef last_fix = M10_DEV_STATUS_NO_FIX;
    uint8_t last_vehicle_count = 0;

    while (1) {
        if (xQueueReceive(gAppState.Queues->TelemetryParserQueue, &input, portMAX_DELAY)) {
            if (input.Class == M10_UBX_CLASS_NAV) {
                if (input.MessageId == M10_UBX_ID_NAV_PVT) {
                    uint16_t year = (input.Payload[5] << 8) | input.Payload[4];
                    uint8_t month = input.Payload[6];
                    uint8_t day = input.Payload[7];
                    uint8_t hour = input.Payload[8];
                    uint8_t min = input.Payload[9];
                    uint8_t sec = input.Payload[10];
                    last_vehicle_count = input.Payload[23];
                    // uint32_t time_acc = (input.Payload[15] << 24) | (input.Payload[14] << 16) | (input.Payload[13] << 8) | input.Payload[12];
                    last_fix = input.Payload[20];
                    int32_t lon = ((input.Payload[27] << 24) | (input.Payload[26] << 16) | (input.Payload[25] << 8) | input.Payload[24]);
                    int32_t lat = ((input.Payload[31] << 24) | (input.Payload[30] << 16) | (input.Payload[29] << 8) | input.Payload[28]);
                    int32_t alt = ((input.Payload[39] << 24) | (input.Payload[38] << 16) | (input.Payload[37] << 8) | input.Payload[36]);
                    uint32_t h_acc = (input.Payload[43] << 24) | (input.Payload[42] << 16) | (input.Payload[41] << 8) | input.Payload[40];
                    uint32_t speed = ((input.Payload[63] << 24) | (input.Payload[62] << 16) | (input.Payload[61] << 8) | input.Payload[60]);
                    // float pdop = ((input.Payload[77] << 8) | input.Payload[76]) / 100.0;

                    // LOGGER_LogF(LOGGER_LEVEL_INFO, "Year: %d; Month: %d; Day: %d; Hour: %d; Minute: %d; Seconds: %d", year, month, day, hour, min, sec);
                    // LOGGER_LogF(LOGGER_LEVEL_INFO, "Time accuracy: %d", time_acc);
                    // LOGGER_LogF(LOGGER_LEVEL_INFO, "Fix type: %d", last_fix);
                    // LOGGER_LogF(LOGGER_LEVEL_INFO, "Longitude: %.7f; Latitude: %.7f; Altitude: %.7f m", lon / 10000000.0, lat / 10000000.0, alt / 1000.0);
                    // LOGGER_LogF(LOGGER_LEVEL_INFO, "Ground speed: %.2f m/s", speed / 1000.0);
                    // LOGGER_LogF(LOGGER_LEVEL_INFO, "PDOP: %.2f; Space Vehicles: %d", pdop, last_vehicle_count);

                    BT_GnssBaseDataTypeDef base_data = {
                        .GnssFix = last_fix,
                        .VelocityCmPerS = speed / 10,
                        .Latitude = lat,
                        .Longitude = lon,
                        .AltitudeCm = alt / 10,
                        .HorizontalAccuracyCm = h_acc / 10,
                        .DateTime = {
                            .Year = year,
                            .Month = month,
                            .Day = day,
                            .Hours = hour,
                            .Minutes = min,
                            .Seconds = sec
                        }
                    };

                    SHVAL_ErrorTypeDef shval_err;
                    if ((shval_err = SHVAL_PointerSetValue(&gAppState.SharedValues->GnssBaseData, &base_data, 1000)) != SHVAL_ERROR_OK) {
                        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to set shared gnss base data! Error code: %d", shval_err);
                    }
                }

                if (input.MessageId == M10_UBX_ID_NAV_DOP) {
                    SHVAL_ErrorTypeDef shval_err;

                    uint16_t raw_hdop = ((input.Payload[13] << 8) | input.Payload[12]) / 10;
                    uint16_t raw_vdop = (input.Payload[11] << 8) | input.Payload[10];

                    float hdop_f = raw_hdop / 100.0f;
                    float vdop_f = raw_vdop / 100.0f;

                    uint8_t hdop = (uint8_t)(hdop_f * 10.0f);
                    uint8_t vdop = (uint8_t)(vdop_f * 10.0f);

                    BT_GnssPrecisionDataTypeDef precision_data = {
                        .GnssFix = last_fix,
                        .VehicleCount = last_vehicle_count,
                        .Hdop = hdop,
                        .Vdop = vdop
                    };

                    if ((shval_err = SHVAL_PointerSetValue(&gAppState.SharedValues->GnssPrecisionData, &precision_data, 1000)) != SHVAL_ERROR_OK) {
                        LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to set shared gnss precision data! Error code: %d", shval_err);
                    }
                }
            }
        }
    }
}
