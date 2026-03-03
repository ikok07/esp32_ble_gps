//
// Created by Kok on 3/3/26.
//

#include "m10.h"

static uint8_t get_cfg_value_size(uint32_t key);
static M10_ErrorTypeDef send_config(M10_HandleTypeDef *hm10, M10_ConfigDataTypeDef *CfgData, uint32_t CfgDataLen, uint8_t Layers);

/**
 * @brief Initializes the u-blox M10 GPS module
 * @param hm10 M10 Handle
 */
M10_ErrorTypeDef M10_Init(M10_HandleTypeDef *hm10) {
    UBX_ErrorTypeDef err_m10 = M10_ERROR_OK;
    UBX_ErrorTypeDef err_ubx = UBX_ERROR_OK;
    if ((err_ubx = UBX_UartInit(&hm10->hubx)) != UBX_ERROR_OK) return M10_ERROR_UBX;

    // TODO: Set baud rate using NMEA message
    // TODO: Check with CFG-RINV commands if configuration is already set

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
        {.Key = M10_CFG_ITM_KEY_SIGNAL_GPS_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_GPS) & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_GPS_L1CA_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_GPS) & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_GAL_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_GALILEO) & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_GAL_E1_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_GALILEO) & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_BDS_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_BEIDOU) & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_BDS_B1_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_BEIDOU) & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_QZSS_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_QZSS) & 0x01},
        {.Key = M10_CFG_ITM_KEY_SIGNAL_QZSS_L1CA_ENA, .Value = (hm10->DeviceConfig.Constellations >> M10_CONSTELLATION_QZSS) & 0x01},

        // TODO: Set NMEA output messages
        // {.Key = M10_CFG_ITM_KEY_MSGOUT_}

        // Configure power mode
        {.Key = M10_CFG_ITM_KEY_PM_OPERATEMODE, .Value = (hm10->DeviceConfig.PowerConfiguration)},
        {.Key = M10_CFG_ITM_KEY_PM_POSUPDATEPERIOD, .Value = (hm10->DeviceConfig.PositionUpdatePeriodSeconds)},

        // Set navigation model
        {.Key = M10_CFG_ITM_KEY_NAVSPG_DYNMODEL, .Value = (hm10->DeviceConfig.NavModel)}
    };

    if ((err_m10 = send_config(hm10, cfg_data, sizeof(cfg_data) / sizeof(cfg_data[0]), hm10->DeviceConfig.ConfigLayers)) != M10_ERROR_OK) {
        return err_m10;
    }

    return err_m10;
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
        .Length = idx,
        .Payload = cfg_buffer
    };

    if (UBX_SendMsgConfig(&hm10->hubx, &ubx_message) != UBX_ERROR_OK) return M10_ERROR_UBX;
    return M10_ERROR_OK;
}