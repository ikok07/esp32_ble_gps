//
// Created by Kok on 3/3/26.
//

#include "m10.h"

#include <string.h>

static uint8_t get_cfg_value_size(uint32_t key);
static M10_ErrorTypeDef send_config(M10_HandleTypeDef *hm10, M10_ConfigDataTypeDef *CfgData, uint32_t CfgDataLen, uint8_t Layers);
static M10_ErrorTypeDef find_br(M10_HandleTypeDef *hm10, uint32_t *BaudRate);
static M10_ErrorTypeDef configure_br(M10_HandleTypeDef *hm10, uint32_t BaudRate);

/**
 * @brief Initializes the u-blox M10 GPS module
 * @param hm10 M10 Handle
 */
M10_ErrorTypeDef M10_Init(M10_HandleTypeDef *hm10) {
    M10_ErrorTypeDef err_m10 = M10_ERROR_OK;
    UBX_ErrorTypeDef err_ubx = UBX_ERROR_OK;
    if ((err_ubx = UBX_UartInit(&hm10->hubx)) != UBX_ERROR_OK) return M10_ERROR_UBX;

    // Find active baudrate and configure the desired one
    uint32_t configured_baud_rate;
    if ((err_m10 = find_br(hm10, &configured_baud_rate)) != M10_ERROR_OK) {
        return err_m10;
    }
    if (configured_baud_rate != hm10->DeviceConfig.BaudRate) {
        if ((err_m10 = configure_br(hm10, hm10->DeviceConfig.BaudRate)) != M10_ERROR_OK) {
            return err_m10;
        }
        hm10->hubx.UartConfig.UartSetBaudRate(hm10->DeviceConfig.BaudRate);
    }

    M10_ConfigDataTypeDef cfg_data[] = {
        // Select UXB as input protocol
        {.Key = M10_CFG_ITM_KEY_UART1INPROT_UBX, .Value = 1},
        {.Key = M10_CFG_ITM_KEY_UART1INPROT_NMEA, .Value = 0},
        {.Key = M10_CFG_ITM_KEY_UART1INPROT_RTCM3X, .Value = 0},

        // Select UXB and NMEA as output protocols
        {.Key = M10_CFG_ITM_KEY_UART1OUTPROT_UBX, .Value = 1},
        {.Key = M10_CFG_ITM_KEY_UART1OUTPROT_NMEA, .Value = 1},

        // Configure update rate
        {.Key = M10_CFG_ITM_KEY_RATE_MEAS, .Value = 1000 / hm10->DeviceConfig.UpdateRate},

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

        // Configure power mode
        {.Key = M10_CFG_ITM_KEY_PM_OPERATEMODE, .Value = (hm10->DeviceConfig.PowerConfiguration)},
        {.Key = M10_CFG_ITM_KEY_PM_POSUPDATEPERIOD, .Value = (hm10->DeviceConfig.PositionUpdatePeriodSeconds)},

        // Set a navigation model
        {.Key = M10_CFG_ITM_KEY_NAVSPG_DYNMODEL, .Value = (hm10->DeviceConfig.NavModel)}
    };

    _Static_assert(sizeof(cfg_data) / sizeof(cfg_data[0]) <= 64, "cfg_data exceeds UBX VALSET 64 item limit");

    if ((err_m10 = send_config(hm10, cfg_data, sizeof(cfg_data) / sizeof(cfg_data[0]), hm10->DeviceConfig.ConfigLayers)) != M10_ERROR_OK) {
        return err_m10;
    }

    return err_m10;
}

/**
 * @brief Gets the current device status
 * @param hm10 Device handle
 * @param Status Device status
 */
M10_ErrorTypeDef M10_GetStatus(M10_HandleTypeDef *hm10, M10_DeviceStatusTypeDef *Status) {
    UBX_ErrorTypeDef ubx_err = M10_ERROR_OK;
    UBX_MessageTypeDef message = {
        .Class = M10_UBX_CLASS_NAV,
        .MessageId = M10_UBX_ID_NAV_STATUS,
        .Length = 0
    };

    UBX_MessageTypeDef output_message;
    if ((ubx_err = UBX_Poll(&hm10->hubx, &message, &output_message)) != UBX_ERROR_OK) {
        if (ubx_err == UBX_ERROR_TIMEOUT) return M10_ERROR_TIMEOUT;
        return M10_ERROR_UBX;
    };

    M10_NavStatusPayloadTypeDef *nav_status = (M10_NavStatusPayloadTypeDef*)output_message.Payload;

    Status->Fix = nav_status->GpsFix;
    Status->FixOk = nav_status->Flags & 0x01;
    Status->WknValid = (nav_status->Flags >> 2) & 0x01;
    Status->ToWValid = (nav_status->Flags >> 3) & 0x01;
    Status->Ttff = nav_status->Ttff;
    Status->Msss = nav_status->Msss;

    return ubx_err;
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

    UBX_MessageTypeDef message = {
        .Class = M10_UBX_CLASS_CFG,
        .MessageId = M10_UBX_ID_CFG_RST,
        .Length = 4
    };
    memcpy(message.Payload, payload, sizeof(payload) / sizeof(payload[0]));

    if (UBX_SendMsg(&hm10->hubx, &message) != UBX_ERROR_OK) return M10_ERROR_UBX;

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

/**
 * @brief Sends key-value configuration data to the device
 * @param hm10 M10 Handle
 * @param CfgData Key-Value pairs containing configuration data
 * @param Layers Layers to write the configuration data
 */
M10_ErrorTypeDef send_config(M10_HandleTypeDef *hm10, M10_ConfigDataTypeDef *CfgData, uint32_t CfgDataLen, uint8_t Layers) {
    if (CfgDataLen > 64) return M10_ERROR_CFG_TOO_MANY_ITEMS;

    uint8_t cfg_buffer[4 + 12 * 64];                // 4 (key) + 8 (value); Max items: 64
    uint16_t idx = 0;

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
    memcpy(ubx_message.Payload, cfg_buffer, idx);

    if (UBX_SendMsgConfig(&hm10->hubx, &ubx_message) != UBX_ERROR_OK) return M10_ERROR_UBX;
    return M10_ERROR_OK;
}


/**
 * @brief Cycles through supported baud rates until it finds the right one for the communication
 * @param hm10 Device handle
 * @param BaudRate Found baud rate
 */
M10_ErrorTypeDef find_br(M10_HandleTypeDef *hm10, uint32_t *BaudRate) {
    UBX_MessageTypeDef test_msg = {
        .Class = M10_UBX_CLASS_MON,
        .MessageId = M10_UBX_ID_MON_VER,
        .Length = 0
    };
    UBX_MessageTypeDef response_msg;

    uint32_t baud_rates[] = {
        UBX_BaudRate115200,
        UBX_BaudRate9600,
        UBX_BaudRate38400,
        UBX_BaudRate4800,
        UBX_BaudRate19200,
        UBX_BaudRate57600,
        UBX_BaudRate230400,
        UBX_BaudRate460800,
        UBX_BaudRate921600
    };
    for (uint32_t i = 0; i < sizeof(baud_rates) / sizeof(baud_rates[0]); i++) {
        if (hm10->hubx.UartConfig.UartSetBaudRate(baud_rates[i]) != 0) return M10_ERROR_BAUD_RATE;

        if (UBX_Poll(&hm10->hubx, &test_msg, &response_msg) == UBX_ERROR_OK) {
            *BaudRate = baud_rates[i];
            return M10_ERROR_OK;
        }
    }

    return M10_ERROR_BAUD_RATE;
}

/**
 * @brief Sends baud rate config command to device
 * @param hm10 M10 Handle
 * @param BaudRate Baud rate to set
 */
M10_ErrorTypeDef configure_br(M10_HandleTypeDef *hm10, uint32_t BaudRate) {
    M10_ErrorTypeDef err_m10 = M10_ERROR_OK;
    M10_ConfigDataTypeDef cfg_data[] = {
        {.Key = M10_CFG_ITM_KEY_UART1_BAUDRATE, .Value = BaudRate}
    };
    // TODO: Verify ACK/NACK is sent over old UART baud rate
    if ((err_m10 = send_config(hm10, cfg_data, sizeof(cfg_data) / sizeof(cfg_data[0]), hm10->DeviceConfig.ConfigLayers)) != M10_ERROR_OK) {
        return err_m10;
    }
    return err_m10;
}