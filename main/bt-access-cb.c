//
// Created by Kok on 4/22/26.
//

#include "bt.h"

#include "ble.h"
#include "shared_values.h"
#include "app_state.h"

int BT_LNFeaturesAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            uint32_t supported_features = 0;
            supported_features |= (1 << 2);             // Location
            supported_features |= (1 << 3);             // Elevation
            supported_features |= (1 << 4);             // Heading
            supported_features |= (1 << 15);            // HDOP

            err = os_mbuf_append(ctxt->om, &supported_features, 1);

            return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            break;
        default:
            break;
    }

    return err;
}

int BT_LocAndSpdAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            SHVAL_ErrorTypeDef shval_err;
            BT_GnssBaseDataTypeDef gnss_base_data;
            if ((shval_err = SHVAL_PointerGetValue(&gAppState.SharedValues->GnssBaseData, &gnss_base_data, NULL, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared gnss base data! Error code: %d", shval_err);
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            uint8_t buffer[24];
            BT_FormatLocAndSpdBuffer(&gnss_base_data, buffer);

            err = os_mbuf_append(ctxt->om, &buffer, sizeof(buffer) / sizeof(buffer[0]));

            return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            break;
        default:
            break;
    }

    return err;
}

#ifdef DEBUG_MODE_ENABLED
int BT_LocAndSpdHumanAccessCB(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
    void *arg) {
    uint8_t err = 0;

    uint16_t buffer_len = 128;
    char buffer[buffer_len];

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            SHVAL_ErrorTypeDef shval_err;
            BT_GnssBaseDataTypeDef gnss_base_data;
            if ((shval_err = SHVAL_PointerGetValue(&gAppState.SharedValues->GnssBaseData, &gnss_base_data, NULL, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared gnss base data! Error code: %d", shval_err);
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            snprintf(
                buffer,
                buffer_len,
                "Longitude: %.7f; Latitude: %.7f; Velocity: %.2f m/s (%.1f km/h); Altitude: %.2f m",
                gnss_base_data.Longitude / 10000000.0,
                gnss_base_data.Latitude / 10000000.0,
                gnss_base_data.VelocityCmPerS / 100.0,
                gnss_base_data.VelocityCmPerS / 27.778,
                gnss_base_data.AltitudeCm / 100.0
            );

            err = os_mbuf_append(ctxt->om, buffer, strlen(buffer));

            return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            break;
        default:
            break;
    }

    return err;
}
#endif

int BT_AltitudeAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            SHVAL_ErrorTypeDef shval_err;
            BT_GnssBaseDataTypeDef gnss_base_data;
            if ((shval_err = SHVAL_PointerGetValue(&gAppState.SharedValues->GnssBaseData, &gnss_base_data, NULL, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared gnss base data! Error code: %d", shval_err);
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            uint8_t altitude[3];
            altitude[0] = gnss_base_data.AltitudeCm & 0xFF;
            altitude[1] = (gnss_base_data.AltitudeCm >> 8) & 0xFF;
            altitude[2] = (gnss_base_data.AltitudeCm >> 16) & 0xFF;

            err = os_mbuf_append(ctxt->om, altitude, 3);

            return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            break;
        default:
            break;
    }

    return err;
}

int BT_GnssFixQltAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            SHVAL_ErrorTypeDef shval_err;
            BT_GnssPrecisionDataTypeDef gnss_precision_data;
            if ((shval_err = SHVAL_PointerGetValue(&gAppState.SharedValues->GnssPrecisionData, &gnss_precision_data, NULL, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared gnss precision data! Error code: %d", shval_err);
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            uint8_t buffer[5];
            BT_FormatGnssFixQltBuffer(&gnss_precision_data, buffer);

            err = os_mbuf_append(ctxt->om, buffer, sizeof(buffer) / sizeof(buffer[0]));

            return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            break;
        default:
            break;
    }

    return err;
}

int BT_DateTimeAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t err = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            SHVAL_ErrorTypeDef shval_err;
            BT_GnssBaseDataTypeDef gnss_base_data;
            if ((shval_err = SHVAL_PointerGetValue(&gAppState.SharedValues->GnssBaseData, &gnss_base_data, NULL, 1000)) != SHVAL_ERROR_OK) {
                LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to get shared gnss base data! Error code: %d", shval_err);
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            uint8_t buffer[7];
            BT_FormatDateTimeBuffer(&gnss_base_data, buffer);

            err = os_mbuf_append(ctxt->om, &buffer, sizeof(buffer) / sizeof(buffer[0]));

            return err == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            break;
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            if (ctxt->om->om_len != 7) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
            if (SHVAL_PointerExists(&gAppState.SharedValues->GnssAuxUtcUpdateData)) {
                if ((shval_err = SHVAL_PointerSetValue(&gAppState.SharedValues->GnssAuxUtcUpdateData, ctxt->om->om_data, 1000)) != SHVAL_ERROR_OK) {
                    LOGGER_LogF(LOGGER_LEVEL_ERROR, "Failed to set shaved gnss aux UTC update data! Error code: %d", gAppState.SharedValues->GnssAuxUtcUpdateData);
                    break;
                }
            }
            break;
        default:
            break;
    }

    return err;
}

int BT_DescriptionDescAccessCB(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC) return BLE_ATT_ERR_UNLIKELY;
    const char *name = (const char*)arg;
    return os_mbuf_append(ctxt->om, name, strlen(name));
}

void BT_FormatLocAndSpdBuffer(BT_GnssBaseDataTypeDef *Data, uint8_t *Buffer) {
    uint16_t flags = 0;
    flags |= (1 << 0);                                                              // Speed
    flags |= (1 << 2);                                                              // Location
    flags |= (1 << 3);                                                              // Altitude
    flags |= (1 << 4);                                                              // Heading
    flags |= (1 << 6);                                                              // UTC Time
    flags &=~ (1 << 9);                                                             // 2D speed
    flags &=~ (0x03 << 10);                                                         // Elevation source: positioning system
    flags |= (1 << 12);                                                             // Heading based on magnetic compass

    uint8_t status = (Data->GnssFix == M10_DEV_STATUS_READY) ? 0x01 : 0x00;
    flags |= (status << 7);                                                         // Position status

    uint8_t offset = 0;

    // Flags
    memcpy(&Buffer[offset], &flags, 2);
    offset += 2;

    // Speed
    Buffer[offset++] = (uint8_t)(Data->VelocityCmPerS & 0xFF);
    Buffer[offset++] = (uint8_t)((Data->VelocityCmPerS >> 8) & 0xFF);

    // Latitude
    memcpy(&Buffer[offset], &Data->Latitude, 4);
    offset += 4;

    // Longitude
    memcpy(&Buffer[offset], &Data->Longitude, 4);
    offset += 4;

    // Altitude
    Buffer[offset++] = (uint8_t)(Data->AltitudeCm & 0xFF);
    Buffer[offset++] = (uint8_t)((Data->AltitudeCm >> 8) & 0xFF);
    Buffer[offset++] = (uint8_t)((Data->AltitudeCm >> 16) & 0xFF);

    // Heading
    Buffer[offset++] = (uint8_t)(Data->Heading & 0xFF);
    Buffer[offset++] = (uint8_t)((Data->Heading >> 8) & 0xFF);

    // Date time
    memcpy(&Buffer[offset], &Data->DateTime.Year, 2); offset += 2;
    memcpy(&Buffer[offset++], &Data->DateTime.Month, 1);
    memcpy(&Buffer[offset++], &Data->DateTime.Day, 1);
    memcpy(&Buffer[offset++], &Data->DateTime.Hours, 1);
    memcpy(&Buffer[offset++], &Data->DateTime.Minutes, 1);
    memcpy(&Buffer[offset], &Data->DateTime.Seconds, 1);
}

void BT_FormatGnssFixQltBuffer(BT_GnssPrecisionDataTypeDef *Data, uint8_t *Buffer) {
    uint16_t flags = 0;
    flags |= (1 << 0);                                              // Number of space vehicles in solution
    flags |= (0x03 << 5);                                           // HDOP & VDOP
    flags |= ((Data->GnssFix == M10_DEV_STATUS_READY) << 7);        // Fix status

    uint8_t offset = 0;

    // Flags
    memcpy(&Buffer[offset], &flags, 2);
    offset += 2;

    // Space vehicles count
    memcpy(&Buffer[offset++], &Data->VehicleCount, 1);

    // HDOP & VDOP
    memcpy(&Buffer[offset++], &Data->Hdop, 1);
    memcpy(&Buffer[offset], &Data->Vdop, 1);
}

void BT_FormatDateTimeBuffer(BT_GnssBaseDataTypeDef *Data, uint8_t *Buffer) {
    uint8_t offset = 0;

    memcpy(&Buffer[offset], &Data->DateTime.Year, 2); offset += 2;
    memcpy(&Buffer[offset++], &Data->DateTime.Month, 1);
    memcpy(&Buffer[offset++], &Data->DateTime.Day, 1);
    memcpy(&Buffer[offset++], &Data->DateTime.Hours, 1);
    memcpy(&Buffer[offset++], &Data->DateTime.Minutes, 1);
    memcpy(&Buffer[offset], &Data->DateTime.Seconds, 1);
}
