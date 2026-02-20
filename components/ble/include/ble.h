//
// Created by Kok on 2/13/26.
//

#ifndef ESP32S3_BLE_BLE_H
#define ESP32S3_BLE_BLE_H

#include "task_scheduler.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

/* ------ Host config ------ */
#define BLE_DEVICE_MANUFACTURER                                     "LED Industries LTD."
#define BLE_DEVICE_SERIAL                                           "LS001"

/* ------ Config definition mappings ------ */
#define MYNEWT_VAL_BLE_SVC_DIS_MANUFACTURER_NAME_DEFAULT            BLE_DEVICE_MANUFACTURER
#define MYNEWT_VAL_BLE_SVC_DIS_SERIAL_NUMBER_DEFAULT                BLE_DEVICE_SERIAL

#define __weak __attribute__((weak))

typedef enum {
    BLE_ERROR_OK,
    BLE_ERROR_INVALID_MAX_CONN,
    BLE_ERROR_INVALID_STATE,
    BLE_ERROR_MISSING_CONN,
    BLE_ERROR_MISSING_HANDLE,
    BLE_ERROR_NVS,
    BLE_ERROR_INIT,
    BLE_ERROR_GAP_CONN_FULL,
    BLE_ERROR_GAP_CONN_NOT_FOUND,
    BLE_ERROR_GAP_NAME,
    BLE_ERROR_GAP_APPEARANCE,
    BLE_ERROR_GAP_ADDRESS,
    BLE_ERROR_GATTS_COUNT,
    BLE_ERROR_GATTS_ADD_SVCS,
    BLE_ERROR_ADV_ADDR,
    BLE_ERROR_ADV_ADDR_CALC,
    BLE_ERROR_ADV_ADDR_COPY,
    BLE_ERROR_ADV_FIELDS,
    BLE_ERROR_RSP_FIELDS,
    BLE_ERROR_ADV_START
} BLE_ErrorTypeDef;

typedef enum {
    BLE_GAP_ROLE_PERIPHERAL,
    BLE_GAP_ROLE_CENTRAL,
} BLE_GapRoleTypeDef;

typedef enum {
    BLE_GAP_EVENT_CONN_SUCCESS,                         // Passed argument - pointer to struct ble_gap_conn_desc
    BLE_GAP_EVENT_CONN_FAILED,
    BLE_GAP_EVENT_CONN_STORE_FAILED,
    BLE_GAP_EVENT_CONN_STORE_NOTIFY_TOGGLE_FAILED,
    BLE_GAP_EVENT_CONN_ENC,
    BLE_GAP_EVENT_CONN_ENC_FAILED,
    BLE_GAP_EVENT_CONN_DISCONNECT,
    BLE_GAP_EVENT_CONN_UPD,                             // Passed argument - pointer to struct ble_gap_conn_desc
    BLE_GAP_EVENT_SUB,
    BLE_GAP_EVENT_UNSUB,
    BLE_GAP_EVENT_PASSKEY,
} BLE_GapEventTypeDef;

typedef enum {
    BLE_GATT_REG_EVENT_REG_SVC,                             // Register service
    BLE_GATT_REG_EVENT_REG_CHR,                             // Register characteristic
    BLE_GATT_REG_EVENT_REG_DSC,                             // Register descriptor
} BLE_GattRegisterEventTypeDef;

typedef enum {
    BLE_PROTECTION_JUST_WORKS,
    BLE_PROTECTION_YESNO,
    BLE_PROTECTION_PASSKEY
} BLE_ProtectionTypeDef;

typedef enum {
    BLE_IOCAP_DISP_ONLY,
    BLE_IOCAP_DISP_YESNO,                                   // This would require the user to accept or decline the displayed code. This will required in most cases some GPIO interrupt
    BLE_IOCAP_NO_INP_OUT = 3,
} BLE_IOCapabilityTypeDef;

typedef struct {
    uint8_t EncryptedConnection;                            // This enabled device bonding, random private address as well as secure connection flag
    BLE_ProtectionTypeDef ProtectionType;
    BLE_IOCapabilityTypeDef IOCapability;
} BLE_SecurityConfigTypeDef;

typedef struct {
    uint8_t Active;
    uint16_t hconn;
    uint8_t NotificationsEnabled;
} BLE_ConnTypeDef;

typedef struct {
    char *DeviceName;
    uint16_t GapAppearance;
    uint8_t PrivateAddressEnabled;
    BLE_GapRoleTypeDef GapRole;
    uint16_t AdvertisingIntervalMS;
    uint8_t MaxConnections;                                 // This number should not be greater than CONFIG_NIMBLE_MAX_CONNECTIONS in menuconfig
    BLE_SecurityConfigTypeDef Security;
} BLE_ConfigTypeDef;

typedef struct {
    SCHEDULER_TaskTypeDef *BLE_Task;
    BLE_ConfigTypeDef Config;
    BLE_ConnTypeDef Connections[CONFIG_BT_NIMBLE_MAX_CONNECTIONS];
    uint8_t AddressType;
    uint8_t Address[6];
    char AddressStr[20];
} BLE_HandleTypeDef;

extern struct ble_gatt_svc_def gGattServices[];

/* ------ Main methods ------ */
BLE_ErrorTypeDef BLE_Init(BLE_HandleTypeDef *hble);
BLE_ErrorTypeDef BLE_CheckConnEncrypted(uint16_t hconn, uint8_t *IsEncrypted);

/* ------ Callbacks ------ */
void BLE_StackResetCB(int Reason);
void BLE_GapEventCB(BLE_GapEventTypeDef Event, struct ble_gap_event *GapEvent, void *Arg);
void BLE_GattRegEventCB(BLE_GattRegisterEventTypeDef Event, struct ble_gatt_register_ctxt *EventCtxt, void *Arg);
uint8_t BLE_GattSubscribeCB(struct ble_gap_event *event);
void BLE_ErrorCB(BLE_ErrorTypeDef Error);
void BLE_AdvertiseSvcsCB(struct ble_hs_adv_fields *Fields);

/* ------ Utilities ------ */



#endif //ESP32S3_BLE_BLE_H