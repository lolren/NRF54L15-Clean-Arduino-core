#ifndef BLUEFRUIT_COMMON_H_
#define BLUEFRUIT_COMMON_H_

#include <Arduino.h>
#include <stdint.h>

using err_t = int32_t;

static constexpr err_t ERROR_NONE = 0;
static constexpr err_t ERROR_INVALID_STATE = -1;
static constexpr err_t ERROR_INVALID_PARAM = -2;
static constexpr err_t ERROR_NO_MEM = -3;
static constexpr err_t ERROR_NOT_SUPPORTED = -4;

static constexpr uint16_t INVALID_CONNECTION_HANDLE = 0xFFFFU;

typedef void (*ble_connect_callback_t)(uint16_t conn_hdl);
typedef void (*ble_disconnect_callback_t)(uint16_t conn_hdl, uint8_t reason);

struct ble_gap_addr_t {
  uint8_t addr[6];
  uint8_t addr_type;
};

struct ble_gap_adv_report_type_t {
  uint8_t connectable : 1;
  uint8_t scannable : 1;
  uint8_t directed : 1;
  uint8_t scan_response : 1;
  uint8_t extended_pdu : 1;
  uint8_t status : 3;
};

struct ble_data_t {
  uint8_t* p_data;
  uint16_t len;
};

struct ble_gatts_evt_read_t {};
struct ble_gatts_evt_write_t {};
struct ble_gap_evt_adv_report_t {
  ble_gap_addr_t peer_addr;
  int8_t rssi;
  ble_data_t data;
  ble_gap_adv_report_type_t type;
};

struct ble_gatts_char_handles_t {
  uint16_t decl_handle;
  uint16_t value_handle;
  uint16_t cccd_handle;
  uint16_t user_desc_handle;
};

enum SecureMode_t {
  SECMODE_NO_ACCESS = 0x00,
  SECMODE_OPEN = 0x11,
  SECMODE_ENC_NO_MITM = 0x21,
  SECMODE_ENC_WITH_MITM = 0x31,
  SECMODE_ENC_WITH_LESC_MITM = 0x41,
  SECMODE_SIGNED_NO_MITM = 0x12,
  SECMODE_SIGNED_WITH_MITM = 0x22,
};

#define CFG_MAX_DEVNAME_LEN 32
#define BLE_GENERIC_TIMEOUT 100
#define BLE_GAP_CONN_SUPERVISION_TIMEOUT_MS 2000
#define BLE_GAP_CONN_SLAVE_LATENCY 0
#define BLE_GAP_CONN_MIN_INTERVAL_DFLT 16
#define BLE_GAP_CONN_MAX_INTERVAL_DFLT 24
#define PRINT_LOCATION() do { } while (0)

#define MS100TO125(ms100) (((ms100) * 4) / 5)
#define MS125TO100(ms125) (((ms125) * 5) / 4)
#define MS1000TO625(ms1000) (((ms1000) * 8) / 5)
#define MS625TO1000(u625) (((u625) * 5) / 8)

#define BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE 0x01U
#define BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE 0x06U

#define BLE_GAP_ADDR_TYPE_PUBLIC 0x00U
#define BLE_GAP_ADDR_TYPE_RANDOM_STATIC 0x01U

#define BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED 0x00U
#define BLE_GAP_ADV_TYPE_ADV_IND BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED
#define BLE_GAP_ADV_TYPE_ADV_NONCONN_IND 0x02U
#define BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED \
  BLE_GAP_ADV_TYPE_ADV_NONCONN_IND
#define BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED 0x06U

#define BLE_GAP_AD_TYPE_FLAGS 0x01U
#define BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE 0x02U
#define BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE 0x03U
#define BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE 0x06U
#define BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE 0x07U
#define BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME 0x08U
#define BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME 0x09U
#define BLE_GAP_AD_TYPE_TX_POWER_LEVEL 0x0AU
#define BLE_GAP_AD_TYPE_APPEARANCE 0x19U
#define BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA 0xFFU

#define BLE_APPEARANCE_GENERIC_CLOCK 0x0040U
#define BLE_APPEARANCE_HID_KEYBOARD 0x03C1U
#define BLE_APPEARANCE_HID_MOUSE 0x03C2U
#define BLE_APPEARANCE_HID_GAMEPAD 0x03C4U

#define UUID16_SVC_CURRENT_TIME 0x1805U
#define UUID16_SVC_HEALTH_THERMOMETER 0x1809U
#define UUID16_SVC_DEVICE_INFORMATION 0x180AU
#define UUID16_SVC_HEART_RATE 0x180DU
#define UUID16_SVC_BATTERY 0x180FU
#define UUID16_SVC_HUMAN_INTERFACE_DEVICE 0x1812U

#define UUID16_CHR_SYSTEM_ID 0x2A23U
#define UUID16_CHR_MODEL_NUMBER_STRING 0x2A24U
#define UUID16_CHR_SERIAL_NUMBER_STRING 0x2A25U
#define UUID16_CHR_FIRMWARE_REVISION_STRING 0x2A26U
#define UUID16_CHR_HARDWARE_REVISION_STRING 0x2A27U
#define UUID16_CHR_SOFTWARE_REVISION_STRING 0x2A28U
#define UUID16_CHR_MANUFACTURER_NAME_STRING 0x2A29U
#define UUID16_CHR_REGULATORY_CERT_DATA_LIST 0x2A2AU
#define UUID16_CHR_BATTERY_LEVEL 0x2A19U
#define UUID16_CHR_TEMPERATURE_MEASUREMENT 0x2A1CU
#define UUID16_CHR_HEART_RATE_MEASUREMENT 0x2A37U
#define UUID16_CHR_BODY_SENSOR_LOCATION 0x2A38U
#define UUID16_CHR_PNP_ID 0x2A50U

#endif  // BLUEFRUIT_COMMON_H_
