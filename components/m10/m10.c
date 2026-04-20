//
// Created by Kok on 3/3/26.
//

#include "m10.h"

#include <string.h>
#include <time.h>

static uint8_t get_cfg_value_size(uint32_t key);
static M10_ErrorTypeDef find_br(M10_HandleTypeDef *hm10, uint32_t *BaudRate);
static M10_ErrorTypeDef configure_br(M10_HandleTypeDef *hm10, uint32_t BaudRate);
static M10_ErrorTypeDef parse_timestamp_ms(uint64_t TimestampMs, M10_ParsedTimestampMsTypeDef *ParsedTimestamp);
static M10_ErrorTypeDef wait_for_mga_ack(M10_HandleTypeDef *hm10, M10_MgaMessageAckInfoCodeTypeDef *AckInfoCode, uint32_t TimeoutMs);

/**
 * @brief Initializes the UART peripheral using the provided UartInit() method in UBX Handle
 * @param hm10 M10 Handle
 * @note This method MUST be called before M10_Init()
 */
M10_ErrorTypeDef M10_InitUART(M10_HandleTypeDef *hm10) {
    UBX_ErrorTypeDef err_ubx = UBX_ERROR_OK;

    if ((err_ubx = UBX_UartInit(&hm10->hubx)) != UBX_ERROR_OK) return M10_ERROR_UBX;

    return M10_ERROR_OK;
}

/**
 * @brief Initializes the u-blox M10 GPS module
 * @param hm10 M10 Handle
 */
M10_ErrorTypeDef M10_Init(M10_HandleTypeDef *hm10) {
    M10_ErrorTypeDef err_m10 = M10_ERROR_OK;

    // Stop GNSS
    M10_GnssStop(hm10);

    M10_ConfigDataTypeDef communication_config[] = {
        // Select UXB as input protocol
        {.Key = M10_CFG_ITM_KEY_UART1INPROT_UBX, .Value = 1},
        {.Key = M10_CFG_ITM_KEY_UART1INPROT_NMEA, .Value = 0},

        // Select UXB and NMEA as output protocols
        {.Key = M10_CFG_ITM_KEY_UART1OUTPROT_UBX, .Value = 1},
        {.Key = M10_CFG_ITM_KEY_UART1OUTPROT_NMEA, .Value = hm10->DeviceConfig.NMEAOutputMessages > 0}
    };

    // Skip ACK, because of changing communication protocol mid-flight
    if ((err_m10 = M10_SendConfig(hm10, communication_config, sizeof(communication_config) / sizeof(communication_config[0]), hm10->DeviceConfig.ConfigLayers, 0)) != M10_ERROR_OK) {
        return err_m10;
    }

    M10_ConfigDataTypeDef main_config[] = {
        // Configure update rate
        {.Key = M10_CFG_ITM_KEY_RATE_MEAS, .Value = 1000 / hm10->DeviceConfig.UpdateRate},

        // Enable MGA_ACK messages
        {.Key = M10_CFG_ITM_KEY_NAVSPG_ACKAIDING, .Value = 1},

        // Select constelations
        {.Key = M10_CFG_ITM_KEY_SIGNAL_GPS_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_GPS_POS)                       & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_GPS_L1CA_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_GPS_POS)                  & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_GAL_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_GALILEO_POS)                   & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_GAL_E1_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_GALILEO_POS)                & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_BDS_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_BEIDOU_POS)                    & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_BDS_B1_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_BEIDOU_POS)                 & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_QZSS_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_QZSS_POS)                     & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_QZSS_L1CA_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_QZSS_POS)                & 0x01},

        // Set NMEA output messages
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_DTM_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_DTM_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_GBS_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_GBS_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_GGA_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_GGA_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_GLL_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_GLL_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_GNS_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_GNS_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_GRS_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_GRS_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_GSA_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_GSA_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_GST_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_GST_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_GSV_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_GSV_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_RLM_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_RLM_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_RMC_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_RMC_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_VLW_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_VLW_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_VTG_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_VTG_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_NMEA_ID_ZDA_UART1,   .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_STD_ZDA_POS)        & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_PUBX_ID_POLYP_UART1, .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_PUBX_CONFIG_POS)    & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_PUBX_ID_POLYS_UART1, .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_PUBX_POSITION_POS)  & 0x01},
        {.Key = M10_CFG_ITM_KEY_MSGOUT_PUBX_ID_POLYT_UART1, .Value = (hm10->DeviceConfig.NMEAOutputMessages >> M10_NMEA_MSG_PUBX_RATE_POS)      & 0x01},

        // Set UBX output messages
        {.Key = M10_CFG_ITM_KEY_MSGOUT_UBX_NAV_PVT_UART1, .Value = 1},

        // Configure power mode
        {.Key = M10_CFG_ITM_KEY_PM_OPERATEMODE, .Value = (hm10->DeviceConfig.PowerConfiguration)},
        {.Key = M10_CFG_ITM_KEY_PM_POSUPDATEPERIOD, .Value = (hm10->DeviceConfig.PositionUpdatePeriodSeconds)},

        // Set a navigation model
        {.Key = M10_CFG_ITM_KEY_NAVSPG_DYNMODEL, .Value = (hm10->DeviceConfig.NavModel)},

        // Configure precision navigation
        {.Key = M10_CFG_ITM_KEY_NAVSPG_OUTFIL_PDOP, .Value = (hm10->DeviceConfig.PDOP != 0 ? hm10->DeviceConfig.PDOP : 250)},
        {.Key = M10_CFG_ITM_KEY_NAVSPG_OUTFIL_TDOP, .Value = (hm10->DeviceConfig.TDOP != 0 ? hm10->DeviceConfig.TDOP : 250)}
    };

    if ((err_m10 = M10_SendConfig(hm10, main_config, sizeof(main_config) / sizeof(main_config[0]), hm10->DeviceConfig.ConfigLayers, 0)) != M10_ERROR_OK) {
        return err_m10;
    }

    // Start GNSS after configuring it
    if ((err_m10 = M10_GnssStart(hm10)) != M10_ERROR_OK) {
        return err_m10;
    }

    return err_m10;
}

/**
 * @brief Sends key-value configuration data to the device
 * @param hm10 M10 Handle
 * @param CfgData Key-Value pairs containing configuration data
 * @param Layers Layers to write the configuration data
 * @param SkipAck
 */
M10_ErrorTypeDef M10_SendConfig(M10_HandleTypeDef *hm10, M10_ConfigDataTypeDef *CfgData, uint32_t CfgDataLen, uint8_t Layers, uint8_t SkipAck) {
    uint8_t cfg_buffer[4 + 12 * 64];                // 4 (key) + 8 (value); Max items: 64
    _Static_assert(sizeof(cfg_buffer) <= UBX_MAX_MSG_PAYLOAD_SIZE, "cfg_buffer is bigger than UBX_MAX_MSG_PAYLOAD_SIZE");

    uint16_t idx = 0;

    // Enable UBX config mode
    UBX_ToggleConfigMode(&hm10->hubx, 1);


    cfg_buffer[idx++] = 0x00;                       // Version
    cfg_buffer[idx++] = Layers;
    cfg_buffer[idx++] = 0x00;                       // Reserved
    cfg_buffer[idx++] = 0x00;                       // Reserved

    for (uint32_t i = 0; i < CfgDataLen; i++) {
        uint32_t key = CfgData[i].Key;
        uint8_t value_size = get_cfg_value_size(key);

        if (value_size == 0) return M10_ERROR_CFG_INVALID_KEY;

        cfg_buffer[idx++] = (key >> 0) & 0xFF;
        cfg_buffer[idx++] = (key >> 8) & 0xFF;
        cfg_buffer[idx++] = (key >> 16) & 0xFF;
        cfg_buffer[idx++] = (key >> 24) & 0xFF;

        for (uint8_t b = 0; b < value_size; b++) {
            cfg_buffer[idx++] = (CfgData[i].Value >> (b * 8)) & 0xFF;
        }
    }

    // Send config data
    UBX_MessageTypeDef ubx_message = {
        .Class = M10_UBX_CLASS_CFG,
        .MessageId = M10_UBX_ID_CFG_VALSET,
        .Length = idx
    };
    if (UBX_AssignMessagePayloadPoolItem(&hm10->hubx, &ubx_message) != UBX_ERROR_OK) {
        // Disable UBX config mode
        UBX_ToggleConfigMode(&hm10->hubx, 0);

        return M10_ERROR_UBX_PAYLOAD;
    }
    memcpy(ubx_message.PayloadPoolItem->Payload, cfg_buffer, idx);

    UBX_ErrorTypeDef ubx_err;
    if ((ubx_err = UBX_SendMsgConfig(&hm10->hubx, &ubx_message, SkipAck)) != UBX_ERROR_OK) {

        // Disable UBX config mode
        UBX_ToggleConfigMode(&hm10->hubx, 0);

        UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
        return M10_ERROR_UBX;
    }

    UBX_ReleaseMessage(&hm10->hubx, &ubx_message);

    // Disable UBX config mode
    UBX_ToggleConfigMode(&hm10->hubx, 0);

    return M10_ERROR_OK;
}

/**
 * @brief Gets the current device status
 * @param hm10 Device handle
 * @param Status Device status
 */
M10_ErrorTypeDef M10_GetStatus(M10_HandleTypeDef *hm10, M10_DeviceStatusTypeDef *Status) {
    UBX_ErrorTypeDef ubx_err = UBX_ERROR_OK;
    UBX_MessageTypeDef ubx_message = {
        .Class = M10_UBX_CLASS_NAV,
        .MessageId = M10_UBX_ID_NAV_STATUS,
        .Length = 0
    };

    UBX_MessageTypeDef output_message = {0};
    if ((ubx_err = UBX_Poll(&hm10->hubx, &ubx_message, &output_message)) != UBX_ERROR_OK) {
        UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
        UBX_ReleaseMessage(&hm10->hubx, &output_message);
        if (ubx_err == UBX_ERROR_TIMEOUT) return M10_ERROR_TIMEOUT;
        return M10_ERROR_UBX;
    };

    M10_NavStatusPayloadTypeDef *nav_status = (M10_NavStatusPayloadTypeDef*)output_message.PayloadPoolItem->Payload;

    Status->Fix = nav_status->GpsFix;
    Status->FixOk = nav_status->Flags & 0x01;
    Status->WknValid = (nav_status->Flags >> 2) & 0x01;
    Status->ToWValid = (nav_status->Flags >> 3) & 0x01;
    Status->Ttff = nav_status->Ttff;
    Status->Msss = nav_status->Msss;

    UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
    UBX_ReleaseMessage(&hm10->hubx, &output_message);

    return ubx_err;
}

M10_ErrorTypeDef M10_GetVersion(M10_HandleTypeDef *hm10, M10_DeviceVersionTypeDef *Version) {
    UBX_MessageTypeDef ubx_message = {
        .Class = M10_UBX_CLASS_MON,
        .MessageId = M10_UBX_ID_MON_VER,
        .Length = 0
    };

    UBX_MessageTypeDef response_msg = {0};

    if (UBX_Poll(&hm10->hubx, &ubx_message, &response_msg) != UBX_ERROR_OK) {
        UBX_ReleaseMessage(&hm10->hubx, &response_msg);
        return M10_ERROR_UBX;
    }

    memcpy(Version->SwVersion, response_msg.PayloadPoolItem->Payload, 30);
    memcpy(Version->HwVersion, response_msg.PayloadPoolItem->Payload + 30, 30);

    UBX_ReleaseMessage(&hm10->hubx, &response_msg);
    return M10_ERROR_OK;
}

/**
 * @brief Returns true if device has 3D fix and the fixOk flag is set
 * @param hm10 Device handle
 */
uint8_t M10_HasValidFix(M10_HandleTypeDef *hm10) {
    M10_ErrorTypeDef m10_err;
    M10_DeviceStatusTypeDef status;
    if ((m10_err = M10_GetStatus(hm10, &status)) != M10_ERROR_OK) {
        return 0;
    }
    return status.Fix == M10_DEV_STATUS_READY && status.FixOk;
}

/**
 * @brief Sends reset command to the device
 * @param hm10 Device handle
 * @param BbrMask Predefined sets which clear specific sections of the BBR
 * @param ResetMode Type of reset
 * @note EXAMPLES:
 * @note Inject AssistNow data \n ------ \n  BbrMask = M10_BBR_MSK_HOT_START; ResetMode = M10_RST_MODE_GNSS_STOP\n Insert data\n ResetMode = M10_RST_MODE_GNSS_START\n ------ \n
 * @note Normal reset \n ------ \n  BbrMask = M10_BBR_MSK_HOT_START;\n ResetMode = M10_RST_MODE_SW_RESET or M10_RST_MODE_SW_GNSS_ONLY_RESET\n ------ \n
 * @note Force reset (when unresponsive) \n ------ \n  BbrMask = M10_BBR_MSK_HOT_START;\n ResetMode = M10_RST_MODE_HW_RESET\n ------ \n
 * @note Satellite data erase \n ------ \n  BbrMask = M10_BBR_MSK_WARM_START or M10_BBR_MSK_COLD_START;\n ResetMode = M10_RST_MODE_SW_RESET\n ------ \n
 */
M10_ErrorTypeDef M10_Reset(M10_HandleTypeDef *hm10, M10_NavBbrMaskTypeDef BbrMask, M10_ResetModeTypeDef ResetMode) {
    uint8_t payload[4] = {0};
    payload[0] = BbrMask & 0xFF;
    payload[1] = (BbrMask >> 8) & 0xFF;
    payload[2] = ResetMode;

    UBX_MessageTypeDef ubx_message = {
        .Class = M10_UBX_CLASS_CFG,
        .MessageId = M10_UBX_ID_CFG_RST,
        .Length = 4
    };
    if (UBX_AssignMessagePayloadPoolItem(&hm10->hubx, &ubx_message) != UBX_ERROR_OK) {
        return M10_ERROR_UBX_PAYLOAD;
    }
    memcpy(ubx_message.PayloadPoolItem->Payload, payload, sizeof(payload) / sizeof(payload[0]));

    if (UBX_SendMsg(&hm10->hubx, &ubx_message) != UBX_ERROR_OK) {
        UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
        return M10_ERROR_UBX;
    }

    UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
    return M10_ERROR_OK;
}

/**
 * @brief Disables device GNSS functionality.
 * @param hm10 Device handle
 */
M10_ErrorTypeDef M10_GnssStop(M10_HandleTypeDef *hm10) {
    return M10_Reset(hm10, M10_BBR_MSK_HOT_START, M10_RST_MODE_GNSS_STOP);
}

/**
 * @brief Enables device GNSS functionality.
 * @param hm10 Device handle
 */
M10_ErrorTypeDef M10_GnssStart(M10_HandleTypeDef *hm10) {
    return M10_Reset(hm10, M10_BBR_MSK_HOT_START, M10_RST_MODE_GNSS_START);
}


/**
 * @brief This method signals the driver whenever a valid signal is detected, no matter what type it is
 * @param hm10 M10 Handle
 * @param MessageType Supported message type
 * @param Payload Optional payload for specific message types
 */
void M10_SignalMessageReceived(M10_HandleTypeDef *hm10, M10_MessageTypeTypeDef MessageType, void *Payload) {
    if (MessageType == M10_MSG_TYPE_UBX) {
        UBX_HandleNewMessage(&hm10->hubx, Payload);
    }
}

/**
 * @brief Configures M10 GNSS Module to use the desired baud rate
 * @param hm10 M10 Handle
 * @param BaudRate Desired baud rate
 * @param Layers Config layers to set
 */
M10_ErrorTypeDef M10_SetBaudRate(M10_HandleTypeDef *hm10, UBX_BaudRateTypeDef BaudRate, uint8_t Layers) {
    M10_ErrorTypeDef m10_err = M10_ERROR_OK;

    M10_ConfigDataTypeDef br_config = {
        .Key = M10_CFG_ITM_KEY_UART1_BAUDRATE, .Value = BaudRate
    };

    if ((m10_err = M10_SendConfig(hm10, &br_config, 1, Layers, 0)) != M10_ERROR_OK) {
        return m10_err;
    };

    return m10_err;
}

/**
 * @brief Sets the current UTC time in the device.
 * @param hm10 Device Handle
 * @param TimestampMs Epoch timestamp in milliseconds
 * @param SAccuracy Seconds accuracy
 * @param NSAccuracy Nanoseconds accuracy
 * @warning This method MUST be called before sending any other MGA messages
 */
M10_ErrorTypeDef M10_SetUTC(M10_HandleTypeDef *hm10, uint64_t TimestampMs, uint16_t SAccuracy, uint32_t NSAccuracy, uint32_t TimeoutMs) {
    M10_ErrorTypeDef m10_err = M10_ERROR_OK;
    UBX_MessageTypeDef ubx_message = {
        .Class = M10_UBX_CLASS_MGA,
        .MessageId = M10_UBX_ID_MGA_INI,
        .Length = 24
    };
    if (UBX_AssignMessagePayloadPoolItem(&hm10->hubx, &ubx_message) != UBX_ERROR_OK) {
        return M10_ERROR_UBX_PAYLOAD;
    }

    M10_ParsedTimestampMsTypeDef utc_time;
    if ((m10_err = parse_timestamp_ms(TimestampMs, &utc_time)) != M10_ERROR_OK) {
        UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
        return m10_err;
    };

    uint8_t payload[24] = {0};
    payload[0] = 0x10;                                              // MGA INI Message type
    payload[1] = 0x00;                                              // MGA INI Message version
    payload[2] = 0x00;                                              // Apply on message receipt
    payload[3] = 0x80;                                              // Receiver will handle the number of leap seconds after 1980
    payload[4] = utc_time.Year & 0xFF;                              // Year
    payload[5] = (utc_time.Year >> 8) & 0xFF;
    payload[6] = utc_time.Month;                                    // Month
    payload[7] = utc_time.Day;                                      // Day
    payload[8] = utc_time.Hour;                                     // Hour
    payload[9] = utc_time.Minute;                                   // Minute
    payload[10] = utc_time.Second;                                  // Second
    payload[11] = 0x01;                                             // Trusted source
    payload[12] = utc_time.Nanosecond & 0xFF;                       // Nanosecond
    payload[13] = (utc_time.Nanosecond >> 8) & 0xFF;
    payload[14] = (utc_time.Nanosecond >> 16) & 0xFF;
    payload[15] = (utc_time.Nanosecond >> 24) & 0xFF;
    payload[16] = SAccuracy & 0xFF;                                 // Seconds part of time accuracy
    payload[17] = (SAccuracy >> 8) & 0xFF;
    payload[20] = NSAccuracy & 0xFF;                                // Nanoseconds part of time accuracy
    payload[21] = (NSAccuracy >> 8) & 0xFF;
    payload[22] = (NSAccuracy >> 16) & 0xFF;
    payload[23] = (NSAccuracy >> 24) & 0xFF;

    memcpy(ubx_message.PayloadPoolItem->Payload, payload, sizeof(payload) / sizeof(payload[0]));

    if (UBX_SendMsg(&hm10->hubx, &ubx_message) != UBX_ERROR_OK) {
        UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
        return M10_ERROR_UBX;
    };

    M10_MgaMessageAckInfoCodeTypeDef info_code;
    if ((m10_err = wait_for_mga_ack(hm10, &info_code, TimeoutMs)) != M10_ERROR_OK) {
        UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
        return m10_err;
    };

    if (info_code != M10_MGA_ACK_ACCEPTED) {
        UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
        return M10_ERROR_MGA_NOT_ACCEPTED;
    }

    UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
    return M10_ERROR_OK;
}

/**
 * @brief Requests all navigation data from the device. This can be used to store the data in persistent storage.
 * @param hm10 Device handle
 * @param HandleDataMessage Function to handle the received message from the device (e.g., store data in flash)
 * @param DataLen The total length of all data chunks
 * @param TimeoutMs Timeout in milliseconds
 */
M10_ErrorTypeDef M10_ExportNavData(M10_HandleTypeDef *hm10, uint8_t(*HandleDataMessage)(uint8_t *ChunkContent, uint32_t Len), uint32_t *DataLen, uint32_t TimeoutMs) {
    uint32_t start = UBX_GetTickMsCB();
    UBX_MessageTypeDef ubx_message = {
        .Class = M10_UBX_CLASS_MGA,
        .MessageId = M10_UBX_ID_MGA_DBD,
        .Length = 0,
    };

    if (UBX_SendMsg(&hm10->hubx, &ubx_message) != UBX_ERROR_OK) {
        UBX_ReleaseMessage(&hm10->hubx, &ubx_message);
        return M10_ERROR_UBX;
    }

    UBX_MsgFilterTypeDef msg_filters[2] = {
        {.Class = M10_UBX_CLASS_MGA, .MessageId = M10_UBX_ID_MGA_DBD},
        {.Class = M10_UBX_CLASS_MGA, .MessageId = M10_UBX_ID_MGA_ACK}
    };

    UBX_MessageTypeDef resp;
    uint32_t total_data_len = 0;
    while (1) {
        uint32_t elapsed = UBX_GetTickMsCB() - start;
        if (elapsed > TimeoutMs) return M10_ERROR_TIMEOUT;

        uint32_t remaining = TimeoutMs - elapsed;
        if (UBX_WaitForMessage(&hm10->hubx, msg_filters, 2, remaining, &resp) != UBX_ERROR_OK) {
            return M10_ERROR_UBX;
        };

        if (resp.MessageId == M10_UBX_ID_MGA_ACK) {
            // Export finished
            M10_MgaMessageAckInfoCodeTypeDef info_code = resp.PayloadPoolItem->Payload[2];
            if (info_code != M10_MGA_ACK_ACCEPTED) {
                UBX_ReleaseMessage(&hm10->hubx, &resp);
                return M10_ERROR_MGA_NOT_ACCEPTED;
            }
            *DataLen = total_data_len;
            break;
        };

        if (resp.MessageId == M10_UBX_ID_MGA_DBD) {
            uint32_t msg_len = 6 + resp.Length + 2;
            uint8_t msg_buffer[172];                // UBX-MGA-DBD message can be maximum of 172 bytes
            msg_buffer[0] = 0xB5;
            msg_buffer[1] = 0x62;
            msg_buffer[2] = resp.Class;
            msg_buffer[3] = resp.MessageId;
            msg_buffer[4] = resp.Length & 0xFF;
            msg_buffer[5] = (resp.Length >> 8) & 0xFF;
            memcpy(&msg_buffer[6], resp.PayloadPoolItem->Payload, resp.Length);
            msg_buffer[6 + resp.Length] = resp.Checksum.Cka;
            msg_buffer[7 + resp.Length] = resp.Checksum.Ckb;

            if (HandleDataMessage(msg_buffer, msg_len) != 0) {
                UBX_ReleaseMessage(&hm10->hubx, &resp);
                return M10_ERROR_MGA_DATA_NOT_HANDLED;
            }
            UBX_ReleaseMessage(&hm10->hubx, &resp);
            total_data_len += msg_len;
        }
    }
    UBX_ReleaseMessage(&hm10->hubx, &resp);
    return M10_ERROR_OK;
}

/**
 * @brief Imports data to the module's database. The data MUST come from M10_ExportNavData() because it is specific to each GNSS module
 * @param hm10 Device handle
 * @param Data Data array received from M10_ExportNavData() or coming directly from U-Blox's AssistNow servers
 * @param DataLen Length of data array
 * @param TimeoutMs Timeout in milliseconds
 */
M10_ErrorTypeDef M10_ImportNavData(M10_HandleTypeDef *hm10, uint8_t *Data, uint32_t DataLen,
                                   uint32_t TimeoutMs) {
    M10_ErrorTypeDef m10_err;
    UBX_ErrorTypeDef ubx_err;

    uint32_t offset = 0;

    while (offset < DataLen) {
        if (Data[offset] != 0xB5 || Data[offset + 1] != 0x62) {
            return M10_ERROR_INVALID_IMPORT_DATA;
        }

        uint16_t payload_len = Data[offset + 4] | ((uint16_t)Data[offset + 5] << 8);
        uint32_t msg_len = 6 + payload_len + 2;  // header + payload + checksum

        if (offset + msg_len > DataLen) return M10_ERROR_INVALID_IMPORT_DATA;

        if ((ubx_err = UBX_SendMsgRaw(&hm10->hubx, &Data[offset])) != UBX_ERROR_OK) {
            return M10_ERROR_UBX;
        };

        M10_MgaMessageAckInfoCodeTypeDef info_code;
        if ((m10_err = wait_for_mga_ack(hm10, &info_code, TimeoutMs)) != M10_ERROR_OK) return m10_err;

        if (info_code != M10_MGA_ACK_ACCEPTED && info_code != M10_MGA_ACK_INFO_NOT_STORED) return M10_ERROR_MGA_NOT_ACCEPTED;

        offset += msg_len;
    }

    return M10_ERROR_OK;
}

/**
 * @brief Extracts value byte-size from key ID bits 30:28
 * @param key Config item key
 */
uint8_t get_cfg_value_size(uint32_t key) {
    switch ((key >> 28) & 0x07) {
        case M10_SIZE_ENC_1BIT:     return 1;
        case M10_SIZE_ENC_1BYTE:    return 1;
        case M10_SIZE_ENC_2BYTES:   return 2;
        case M10_SIZE_ENC_4BYTES:   return 4;
        case M10_SIZE_ENC_8BYTES:   return 8;
        default:                    return 0;
    }
}

M10_ErrorTypeDef parse_timestamp_ms(uint64_t TimestampMs, M10_ParsedTimestampMsTypeDef *ParsedTimestamp) {
    time_t unix_seconds = TimestampMs / 1000;
    struct tm *t = gmtime(&unix_seconds);
    if (t == NULL) return M10_ERROR_INVALID_TIMESTAMP;

    ParsedTimestamp->Nanosecond = (TimestampMs % 1000) * 1000000;
    ParsedTimestamp->Second = t->tm_sec;
    ParsedTimestamp->Minute = t->tm_min;
    ParsedTimestamp->Hour = t->tm_hour;
    ParsedTimestamp->Day = t->tm_mday;
    ParsedTimestamp->Month = t->tm_mon + 1;
    ParsedTimestamp->Year = t->tm_year + 1900;
    return M10_ERROR_OK;
}

/**
 * @brief Waits for MGA message ACK
 * @param hm10 Device handle
 * @param AckInfoCode Info code field of the MGA ACK's info code field
 */
M10_ErrorTypeDef wait_for_mga_ack(M10_HandleTypeDef *hm10, M10_MgaMessageAckInfoCodeTypeDef *AckInfoCode, uint32_t TimeoutMs) {
    UBX_MsgFilterTypeDef msg_filter = {
        .Class = M10_UBX_CLASS_MGA,
        .MessageId = M10_UBX_ID_MGA_ACK
    };
    UBX_MessageTypeDef resp;
    if (UBX_WaitForMessage(&hm10->hubx, &msg_filter, 1, TimeoutMs, &resp) != UBX_ERROR_OK) {
        return M10_ERROR_MGA_NOACK;
    };

    *AckInfoCode = resp.PayloadPoolItem->Payload[2];

    UBX_ReleaseMessage(&hm10->hubx, &resp);
    return M10_ERROR_OK;
}
