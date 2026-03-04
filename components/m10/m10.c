//
// Created by Kok on 3/3/26.
//

#include "m10.h"

#include <string.h>

static uint8_t get_cfg_value_size(uint32_t key);
static M10_ErrorTypeDef send_config(M10_HandleTypeDef *hm10, M10_ConfigDataTypeDef *CfgData, uint32_t CfgDataLen, uint8_t Layers);
static M10_ErrorTypeDef find_br(M10_HandleTypeDef *hm10, uint32_t *BaudRate);
static M10_ErrorTypeDef configure_br(M10_HandleTypeDef *hm10, uint32_t BaudRate)

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
        uart_set_baudrate(hm10->hubx.UartConfig.UartPort, hm10->DeviceConfig.BaudRate);
    }

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
    for (uint8_t i = 0; i < sizeof(baud_rates) / sizeof(baud_rates[0]); i++) {
        if (uart_set_baudrate(hm10->hubx.UartConfig.UartPort, baud_rates[i]) != 0) return M10_ERROR_BAUD_RATE;

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
    // TODO: Verify ACK/NACK is sent over old UART
    if ((err_m10 = send_config(hm10, cfg_data, sizeof(cfg_data) / sizeof(cfg_data[0]), hm10->DeviceConfig.ConfigLayers)) != M10_ERROR_OK) {
        return err_m10;
    }
    return err_m10;
}