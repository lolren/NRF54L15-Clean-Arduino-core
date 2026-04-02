#include "bluefruit.h"

#include <stdio.h>
#include <string.h>

#include <nrf54l15_hal.h>

using xiao_nrf54l15::BleConnectionInfo;
using xiao_nrf54l15::BleDisconnectDebug;
using xiao_nrf54l15::BleDisconnectReason;
using xiao_nrf54l15::BleGattCharacteristicProperty;
using xiao_nrf54l15::BleRadio;

constexpr uint8_t kAdTypeFlags = 0x01U;
constexpr uint8_t kAdTypeIncomplete16 = 0x02U;
constexpr uint8_t kAdTypeComplete16 = 0x03U;
constexpr uint8_t kAdTypeIncomplete128 = 0x06U;
constexpr uint8_t kAdTypeComplete128 = 0x07U;
constexpr uint8_t kAdTypeShortName = 0x08U;
constexpr uint8_t kAdTypeCompleteName = 0x09U;
constexpr uint8_t kAdTypeTxPower = 0x0AU;
constexpr uint8_t kAdTypeAppearance = 0x19U;
constexpr uint8_t kAdTypeManufacturer = 0xFFU;
constexpr uint8_t kLedOnState = LOW;
constexpr uint8_t kLedOffState = HIGH;

constexpr uint16_t kUuidDfuService = 0xFE59U;
constexpr uint16_t kCompanyIdApple = 0x004CU;

uint8_t clampValueLen(uint16_t len) {
  if (len > BleRadio::kCustomGattMaxValueLength) {
    return BleRadio::kCustomGattMaxValueLength;
  }
  return static_cast<uint8_t>(len);
}

bool timeReachedUs(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

uint16_t byteSwap16(uint16_t value) {
  return static_cast<uint16_t>((value >> 8U) | (value << 8U));
}

int hexDigitValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

bool parseUuidString(const char* str, uint8_t outUuid[16], uint8_t* outSize,
                     uint16_t* outUuid16) {
  if (str == nullptr || outSize == nullptr || outUuid16 == nullptr) {
    return false;
  }

  const size_t len = strlen(str);
  if (len == 4U) {
    char* end = nullptr;
    const unsigned long value = strtoul(str, &end, 16);
    if (end == nullptr || *end != '\0' || value > 0xFFFFUL) {
      return false;
    }
    *outSize = 2U;
    *outUuid16 = static_cast<uint16_t>(value);
    return true;
  }

  char hex[32] = {0};
  size_t used = 0U;
  for (size_t i = 0U; i < len; ++i) {
    const char c = str[i];
    if (c == '-') {
      continue;
    }
    if (hexDigitValue(c) < 0 || used >= sizeof(hex)) {
      return false;
    }
    hex[used++] = c;
  }
  if (used != 32U) {
    return false;
  }

  uint8_t bigEndian[16] = {0};
  for (size_t i = 0U; i < 16U; ++i) {
    const int hi = hexDigitValue(hex[i * 2U]);
    const int lo = hexDigitValue(hex[i * 2U + 1U]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    bigEndian[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  for (size_t i = 0U; i < 16U; ++i) {
    outUuid[i] = bigEndian[15U - i];
  }
  *outSize = 16U;
  *outUuid16 = 0U;
  return true;
}

uint8_t mapProperties(uint8_t properties) {
  uint8_t mapped = 0U;
  if ((properties & CHR_PROPS_READ) != 0U) {
    mapped |= xiao_nrf54l15::kBleGattPropRead;
  }
  if ((properties & CHR_PROPS_WRITE_WO_RESP) != 0U) {
    mapped |= xiao_nrf54l15::kBleGattPropWriteNoRsp;
  }
  if ((properties & CHR_PROPS_WRITE) != 0U) {
    mapped |= xiao_nrf54l15::kBleGattPropWrite;
  }
  if ((properties & CHR_PROPS_NOTIFY) != 0U) {
    mapped |= xiao_nrf54l15::kBleGattPropNotify;
  }
  if ((properties & CHR_PROPS_INDICATE) != 0U) {
    mapped |= xiao_nrf54l15::kBleGattPropIndicate;
  }
  return mapped;
}

uint8_t disconnectReasonToHci(const BleDisconnectDebug& debug) {
  switch (static_cast<BleDisconnectReason>(debug.reason)) {
    case BleDisconnectReason::kApi:
      return 0x16U;
    case BleDisconnectReason::kSupervisionTimeout:
      return 0x08U;
    case BleDisconnectReason::kPeerTerminate:
      return (debug.errorCode != 0U) ? debug.errorCode : 0x13U;
    case BleDisconnectReason::kMicFailure:
      return 0x3DU;
    case BleDisconnectReason::kInternalTerminate:
      return (debug.errorCode != 0U) ? debug.errorCode : 0x13U;
    case BleDisconnectReason::kNone:
    default:
      return 0x13U;
  }
}

class BluefruitCompatManager {
 public:
  BluefruitCompatManager()
      : started_(false),
        last_connected_(false),
        next_adv_due_us_(0U),
        adv_started_ms_(0UL),
        scan_rsp_name_added_(false),
        characteristic_count_(0U) {}

  bool begin(uint8_t prph_count, uint8_t central_count) {
    (void)prph_count;
    (void)central_count;
    if (started_) {
      return true;
    }

    if (!radio_.begin()) {
      return false;
    }
    radio_.loadAddressFromFicr();
    radio_.setAdvertisingPduType(xiao_nrf54l15::BleAdvPduType::kAdvInd);
    radio_.setAdvertisingChannelSelectionAlgorithm2(false);
    radio_.clearCustomGatt();
    radio_.setGattDeviceName(Bluefruit.device_name_);
    radio_.setGattAppearance(Bluefruit.appearance_);
    radio_.setCustomGattWriteCallback(&BluefruitCompatManager::gattWriteThunk, this);

    if (Bluefruit.auto_conn_led_) {
      pinMode(LED_BUILTIN, OUTPUT);
      digitalWrite(LED_BUILTIN, kLedOffState);
    }

    started_ = true;
    last_connected_ = radio_.isConnected();
    return true;
  }

  BleRadio& radio() { return radio_; }
  const BleRadio& radio() const { return radio_; }

  bool registerCharacteristic(BLECharacteristic* characteristic) {
    if (characteristic == nullptr || characteristic_count_ >= kMaxCharacteristics) {
      return false;
    }
    characteristics_[characteristic_count_++] = characteristic;
    return true;
  }

  void markAdvertisingDirty() {
    Bluefruit.Advertising.dirty_ = true;
    Bluefruit.ScanResponse.dirty_ = true;
  }

  void advertisingStarted() {
    adv_started_ms_ = millis();
    next_adv_due_us_ = 0U;
  }

  BLEConnection* connection() { return &connection_; }

  void idleService() {
    if (!started_) {
      return;
    }

    if (!radio_.isConnected()) {
      maybeAdvertise();
    }

    const bool connected = radio_.isConnected();
    if (connected != last_connected_) {
      handleConnectionEdge(connected);
      last_connected_ = connected;
    }

    if (connected) {
      for (uint8_t i = 0U; i < characteristic_count_; ++i) {
        characteristics_[i]->pollCccdState();
      }
    }
  }

 private:
  static constexpr uint8_t kMaxCharacteristics = 24U;

  BleRadio radio_;
  bool started_;
  bool last_connected_;
  uint32_t next_adv_due_us_;
  unsigned long adv_started_ms_;
  bool scan_rsp_name_added_;
  BLECharacteristic* characteristics_[kMaxCharacteristics];
  uint8_t characteristic_count_;
  BLEConnection connection_;

  static void gattWriteThunk(uint16_t valueHandle, const uint8_t* value,
                             uint8_t valueLength, bool withResponse, void* context) {
    (void)withResponse;
    auto* self = static_cast<BluefruitCompatManager*>(context);
    if (self != nullptr) {
      self->handleGattWrite(valueHandle, value, valueLength);
    }
  }

  void handleGattWrite(uint16_t valueHandle, const uint8_t* value, uint8_t valueLength) {
    for (uint8_t i = 0U; i < characteristic_count_; ++i) {
      BLECharacteristic* characteristic = characteristics_[i];
      if (characteristic == nullptr) {
        continue;
      }
      if (characteristic->_handles.value_handle == valueHandle) {
        characteristic->handleWriteFromRadio(value, valueLength);
        return;
      }
    }
  }

  bool applyAdvertisingPayloads() {
    if (!radio_.setAdvertisingPduType(
            static_cast<xiao_nrf54l15::BleAdvPduType>(Bluefruit.Advertising.adv_type_))) {
      return false;
    }
    if (!radio_.setAdvertisingData(Bluefruit.Advertising.data_, Bluefruit.Advertising.len_)) {
      return false;
    }
    if (!radio_.setScanResponseData(Bluefruit.ScanResponse.data_, Bluefruit.ScanResponse.len_)) {
      return false;
    }
    Bluefruit.Advertising.dirty_ = false;
    Bluefruit.ScanResponse.dirty_ = false;
    return true;
  }

  void maybeAdvertise() {
    if (!Bluefruit.Advertising.running_) {
      return;
    }

    if (Bluefruit.Advertising.stop_timeout_s_ != 0U) {
      const unsigned long elapsedMs = millis() - adv_started_ms_;
      if (elapsedMs >= (static_cast<unsigned long>(Bluefruit.Advertising.stop_timeout_s_) *
                        1000UL)) {
        Bluefruit.Advertising.running_ = false;
        if (Bluefruit.Advertising.stop_callback_ != nullptr) {
          Bluefruit.Advertising.stop_callback_();
        }
        return;
      }
    }

    if (Bluefruit.Advertising.dirty_ || Bluefruit.ScanResponse.dirty_) {
      if (!applyAdvertisingPayloads()) {
        return;
      }
    }

    uint16_t intervalUnits = Bluefruit.Advertising.interval_fast_;
    if (Bluefruit.Advertising.fast_timeout_s_ != 0U) {
      const unsigned long elapsedMs = millis() - adv_started_ms_;
      if (elapsedMs >= (static_cast<unsigned long>(Bluefruit.Advertising.fast_timeout_s_) *
                        1000UL)) {
        intervalUnits = (Bluefruit.Advertising.interval_slow_ != 0U)
                            ? Bluefruit.Advertising.interval_slow_
                            : Bluefruit.Advertising.interval_fast_;
      }
    }
    if (intervalUnits == 0U) {
      intervalUnits = 32U;
    }

    const uint32_t nowUs = micros();
    if (next_adv_due_us_ != 0U && !timeReachedUs(nowUs, next_adv_due_us_)) {
      return;
    }

    radio_.advertiseInteractEvent(nullptr);
    next_adv_due_us_ = nowUs + (static_cast<uint32_t>(intervalUnits) * 625UL);
  }

  void handleConnectionEdge(bool connected) {
    if (connected) {
      connection_.handle_ = 0U;
      if (Bluefruit.auto_conn_led_) {
        digitalWrite(LED_BUILTIN, kLedOnState);
      }
      for (uint8_t i = 0U; i < characteristic_count_; ++i) {
        if (characteristics_[i] != nullptr) {
          characteristics_[i]->_notify_enabled = false;
          characteristics_[i]->_indicate_enabled = false;
        }
      }
      if (Bluefruit.Periph.connect_callback_ != nullptr) {
        Bluefruit.Periph.connect_callback_(0U);
      }
      if (!Bluefruit.Advertising.restart_on_disconnect_) {
        Bluefruit.Advertising.running_ = false;
      }
      return;
    }

    BleDisconnectDebug debug{};
    uint8_t reason = 0x13U;
    if (radio_.getDisconnectDebug(&debug)) {
      reason = disconnectReasonToHci(debug);
    }
    connection_.handle_ = INVALID_CONNECTION_HANDLE;
    for (uint8_t i = 0U; i < characteristic_count_; ++i) {
      if (characteristics_[i] != nullptr) {
        const bool hadNotify = characteristics_[i]->_notify_enabled;
        const bool hadIndicate = characteristics_[i]->_indicate_enabled;
        characteristics_[i]->_notify_enabled = false;
        characteristics_[i]->_indicate_enabled = false;
        if ((hadNotify || hadIndicate) && characteristics_[i]->_cccd_wr_cb != nullptr) {
          characteristics_[i]->_cccd_wr_cb(0U, characteristics_[i], 0U);
        }
      }
    }
    if (Bluefruit.auto_conn_led_) {
      digitalWrite(LED_BUILTIN, kLedOffState);
    }
    if (Bluefruit.Periph.disconnect_callback_ != nullptr) {
      Bluefruit.Periph.disconnect_callback_(0U, reason);
    }
    if (Bluefruit.Advertising.restart_on_disconnect_) {
      Bluefruit.Advertising.running_ = true;
      adv_started_ms_ = millis();
      next_adv_due_us_ = 0U;
    }
  }

};

BluefruitCompatManager& manager() {
  static BluefruitCompatManager instance;
  return instance;
}

BLEService* BLEService::lastService = nullptr;
AdafruitBluefruit Bluefruit;
const uint8_t BLEUART_UUID_SERVICE[16] = {0x9EU, 0xCAU, 0xDCU, 0x24U, 0x0EU, 0xE5U,
                                          0xA9U, 0xE0U, 0x93U, 0xF3U, 0xA3U, 0xB5U,
                                          0x01U, 0x00U, 0x40U, 0x6EU};
const uint8_t BLEUART_UUID_CHR_RXD[16] = {0x9EU, 0xCAU, 0xDCU, 0x24U, 0x0EU, 0xE5U,
                                          0xA9U, 0xE0U, 0x93U, 0xF3U, 0xA3U, 0xB5U,
                                          0x02U, 0x00U, 0x40U, 0x6EU};
const uint8_t BLEUART_UUID_CHR_TXD[16] = {0x9EU, 0xCAU, 0xDCU, 0x24U, 0x0EU, 0xE5U,
                                          0xA9U, 0xE0U, 0x93U, 0xF3U, 0xA3U, 0xB5U,
                                          0x03U, 0x00U, 0x40U, 0x6EU};

extern "C" void nrf54l15_bluefruit_compat_idle_service(void) {
  manager().idleService();
}

BLEUuid::BLEUuid() : size_(0U), uuid16_(0U), uuid128_{0} {}

BLEUuid::BLEUuid(uint16_t uuid16) : BLEUuid() { set(uuid16); }

BLEUuid::BLEUuid(const uint8_t uuid128[16]) : BLEUuid() { set(uuid128); }

BLEUuid::BLEUuid(const char* str) : BLEUuid() { set(str); }

void BLEUuid::set(uint16_t uuid16) {
  size_ = 2U;
  uuid16_ = uuid16;
  memset(uuid128_, 0, sizeof(uuid128_));
}

void BLEUuid::set(const uint8_t uuid128[16]) {
  if (uuid128 == nullptr) {
    size_ = 0U;
    uuid16_ = 0U;
    memset(uuid128_, 0, sizeof(uuid128_));
    return;
  }
  size_ = 16U;
  uuid16_ = 0U;
  memcpy(uuid128_, uuid128, sizeof(uuid128_));
}

void BLEUuid::set(const char* str) {
  uint8_t parsed[16] = {0};
  uint8_t parsedSize = 0U;
  uint16_t parsed16 = 0U;
  if (!parseUuidString(str, parsed, &parsedSize, &parsed16)) {
    size_ = 0U;
    uuid16_ = 0U;
    memset(uuid128_, 0, sizeof(uuid128_));
    return;
  }
  if (parsedSize == 2U) {
    set(parsed16);
  } else {
    set(parsed);
  }
}

bool BLEUuid::get(uint16_t* uuid16) const {
  if (uuid16 == nullptr || size_ != 2U) {
    return false;
  }
  *uuid16 = uuid16_;
  return true;
}

bool BLEUuid::get(uint8_t uuid128[16]) const {
  if (uuid128 == nullptr || size_ != 16U) {
    return false;
  }
  memcpy(uuid128, uuid128_, sizeof(uuid128_));
  return true;
}

size_t BLEUuid::size() const { return size_; }

bool BLEUuid::begin() const { return size_ == 2U || size_ == 16U; }

String BLEUuid::toString() const {
  if (size_ == 2U) {
    char buffer[5] = {0};
    snprintf(buffer, sizeof(buffer), "%04X", uuid16_);
    return String(buffer);
  }
  if (size_ != 16U) {
    return String();
  }
  uint8_t bigEndian[16] = {0};
  for (size_t i = 0U; i < 16U; ++i) {
    bigEndian[i] = uuid128_[15U - i];
  }
  char buffer[37] = {0};
  snprintf(buffer, sizeof(buffer),
           "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
           bigEndian[0], bigEndian[1], bigEndian[2], bigEndian[3], bigEndian[4],
           bigEndian[5], bigEndian[6], bigEndian[7], bigEndian[8], bigEndian[9],
           bigEndian[10], bigEndian[11], bigEndian[12], bigEndian[13], bigEndian[14],
           bigEndian[15]);
  return String(buffer);
}

bool BLEUuid::operator==(const BLEUuid& rhs) const {
  if (size_ != rhs.size_) {
    return false;
  }
  if (size_ == 2U) {
    return uuid16_ == rhs.uuid16_;
  }
  if (size_ == 16U) {
    return memcmp(uuid128_, rhs.uuid128_, sizeof(uuid128_)) == 0;
  }
  return true;
}

bool BLEUuid::operator!=(const BLEUuid& rhs) const { return !(*this == rhs); }

uint16_t BLEUuid::uuid16() const { return uuid16_; }

const uint8_t* BLEUuid::uuid128() const { return uuid128_; }

BLEService::BLEService()
    : uuid(), _handle(0U), _read_perm(SECMODE_OPEN), _write_perm(SECMODE_OPEN),
      _begun(false) {}

BLEService::BLEService(BLEUuid bleuuid)
    : uuid(bleuuid), _handle(0U), _read_perm(SECMODE_OPEN), _write_perm(SECMODE_OPEN),
      _begun(false) {}

void BLEService::setUuid(BLEUuid bleuuid) { uuid = bleuuid; }

void BLEService::setPermission(SecureMode_t read_perm, SecureMode_t write_perm) {
  _read_perm = read_perm;
  _write_perm = write_perm;
}

void BLEService::getPermission(SecureMode_t* read_perm, SecureMode_t* write_perm) const {
  if (read_perm != nullptr) {
    *read_perm = _read_perm;
  }
  if (write_perm != nullptr) {
    *write_perm = _write_perm;
  }
}

err_t BLEService::begin() {
  if (!manager().begin(1U, 0U)) {
    return ERROR_INVALID_STATE;
  }
  if (_begun) {
    lastService = this;
    return ERROR_NONE;
  }

  bool ok = false;
  if (uuid.size() == 2U) {
    ok = manager().radio().addCustomGattService(uuid.uuid16(), &_handle);
  } else if (uuid.size() == 16U) {
    ok = manager().radio().addCustomGattService128(uuid.uuid128(), &_handle);
  }
  if (!ok) {
    return ERROR_INVALID_STATE;
  }

  _begun = true;
  lastService = this;
  return ERROR_NONE;
}

BLECharacteristic::BLECharacteristic()
    : uuid(),
      _is_temp(false),
      _max_len(BleRadio::kCustomGattMaxValueLength),
      _fixed_len(false),
      _service(nullptr),
      _userbuf(nullptr),
      _userbufsize(0U),
      _properties(0U),
      _read_perm(SECMODE_OPEN),
      _write_perm(SECMODE_OPEN),
      _handles{0U, 0U, 0U, 0U},
      _value{0},
      _value_len(0U),
      _notify_enabled(false),
      _indicate_enabled(false),
      _usr_descriptor(nullptr),
      _wr_cb(nullptr),
      _cccd_wr_cb(nullptr),
      _rd_authorize_cb(nullptr),
      _wr_authorize_cb(nullptr) {}

BLECharacteristic::BLECharacteristic(BLEUuid bleuuid) : BLECharacteristic() { uuid = bleuuid; }

BLECharacteristic::BLECharacteristic(BLEUuid bleuuid, uint8_t properties)
    : BLECharacteristic(bleuuid) {
  _properties = properties;
}

BLECharacteristic::BLECharacteristic(BLEUuid bleuuid, uint8_t properties, int max_len,
                                     bool fixed_len)
    : BLECharacteristic(bleuuid, properties) {
  setMaxLen(static_cast<uint16_t>(max_len));
  if (fixed_len) {
    setFixedLen(static_cast<uint16_t>(max_len));
  }
}

BLEService& BLECharacteristic::parentService() { return *_service; }

void BLECharacteristic::setTempMemory() { _is_temp = true; }

void BLECharacteristic::setUuid(BLEUuid bleuuid) { uuid = bleuuid; }

void BLECharacteristic::setProperties(uint8_t prop) { _properties = prop; }

void BLECharacteristic::setPermission(SecureMode_t read_perm, SecureMode_t write_perm) {
  _read_perm = read_perm;
  _write_perm = write_perm;
}

void BLECharacteristic::setMaxLen(uint16_t max_len) {
  _max_len = (max_len > BleRadio::kCustomGattMaxValueLength)
                 ? BleRadio::kCustomGattMaxValueLength
                 : max_len;
}

void BLECharacteristic::setFixedLen(uint16_t fixed_len) {
  setMaxLen(fixed_len);
  _fixed_len = true;
}

void BLECharacteristic::setBuffer(void* buf, uint16_t bufsize) {
  _userbuf = static_cast<uint8_t*>(buf);
  _userbufsize = bufsize;
}

uint16_t BLECharacteristic::getMaxLen() const { return _max_len; }

bool BLECharacteristic::isFixedLen() const { return _fixed_len; }

void BLECharacteristic::setUserDescriptor(const char* descriptor) { _usr_descriptor = descriptor; }

void BLECharacteristic::setReportRefDescriptor(uint8_t id, uint8_t type) {
  (void)id;
  (void)type;
}

void BLECharacteristic::setPresentationFormatDescriptor(uint8_t type, int8_t exponent,
                                                        uint16_t unit, uint8_t name_space,
                                                        uint16_t descriptor) {
  (void)type;
  (void)exponent;
  (void)unit;
  (void)name_space;
  (void)descriptor;
}

void BLECharacteristic::setWriteCallback(write_cb_t fp, bool useAdaCallback) {
  (void)useAdaCallback;
  _wr_cb = fp;
}

void BLECharacteristic::setCccdWriteCallback(write_cccd_cb_t fp, bool useAdaCallback) {
  (void)useAdaCallback;
  _cccd_wr_cb = fp;
}

void BLECharacteristic::setReadAuthorizeCallback(read_authorize_cb_t fp, bool useAdaCallback) {
  (void)useAdaCallback;
  _rd_authorize_cb = fp;
}

void BLECharacteristic::setWriteAuthorizeCallback(write_authorize_cb_t fp,
                                                  bool useAdaCallback) {
  (void)useAdaCallback;
  _wr_authorize_cb = fp;
}

err_t BLECharacteristic::begin() {
  if (!manager().begin(1U, 0U)) {
    return ERROR_INVALID_STATE;
  }
  if (_handles.value_handle != 0U) {
    return ERROR_NONE;
  }

  _service = BLEService::lastService;
  if (_service == nullptr || !_service->_begun) {
    return ERROR_INVALID_STATE;
  }

  uint16_t valueHandle = 0U;
  uint16_t cccdHandle = 0U;
  const uint8_t initialLen = clampValueLen(_value_len);
  const uint8_t properties = mapProperties(_properties);
  bool ok = false;
  if (uuid.size() == 2U) {
    ok = manager().radio().addCustomGattCharacteristic(
        _service->_handle, uuid.uuid16(), properties, _value, initialLen, &valueHandle,
        &cccdHandle);
  } else if (uuid.size() == 16U) {
    ok = manager().radio().addCustomGattCharacteristic128(
        _service->_handle, uuid.uuid128(), properties, _value, initialLen, &valueHandle,
        &cccdHandle);
  }
  if (!ok) {
    return ERROR_INVALID_STATE;
  }

  _handles.value_handle = valueHandle;
  _handles.decl_handle = (valueHandle > 0U) ? static_cast<uint16_t>(valueHandle - 1U) : 0U;
  _handles.cccd_handle = cccdHandle;
  if (!manager().registerCharacteristic(this)) {
    return ERROR_NO_MEM;
  }
  return ERROR_NONE;
}

ble_gatts_char_handles_t BLECharacteristic::handles() const { return _handles; }

uint16_t BLECharacteristic::write(const void* data, uint16_t len) {
  if (data == nullptr && len > 0U) {
    return 0U;
  }
  const uint8_t toWrite = clampValueLen((_fixed_len && _max_len > 0U) ? _max_len : len);
  memset(_value, 0, sizeof(_value));
  if (data != nullptr && toWrite > 0U) {
    memcpy(_value, data, min<uint8_t>(toWrite, clampValueLen(len)));
  }
  _value_len = toWrite;
  if (_userbuf != nullptr && _userbufsize > 0U) {
    const uint16_t copied = min<uint16_t>(_userbufsize, _value_len);
    memcpy(_userbuf, _value, copied);
  }
  if (_handles.value_handle != 0U) {
    manager().radio().setCustomGattCharacteristicValue(_handles.value_handle, _value, _value_len);
  }
  return _value_len;
}

uint16_t BLECharacteristic::write(const char* str) {
  return write(str, (str != nullptr) ? static_cast<uint16_t>(strlen(str)) : 0U);
}

uint16_t BLECharacteristic::write8(uint8_t num) { return write(&num, sizeof(num)); }

uint16_t BLECharacteristic::write16(uint16_t num) { return write(&num, sizeof(num)); }

uint16_t BLECharacteristic::write32(uint32_t num) { return write(&num, sizeof(num)); }

uint16_t BLECharacteristic::write32(int num) { return write(&num, sizeof(num)); }

uint16_t BLECharacteristic::writeFloat(float num) { return write(&num, sizeof(num)); }

uint16_t BLECharacteristic::read(void* buffer, uint16_t bufsize, uint16_t offset) {
  if (buffer == nullptr || bufsize == 0U) {
    return 0U;
  }
  if (_handles.value_handle != 0U) {
    uint8_t len = sizeof(_value);
    if (manager().radio().getCustomGattCharacteristicValue(_handles.value_handle, _value, &len)) {
      _value_len = len;
    }
  }
  if (offset >= _value_len) {
    return 0U;
  }
  const uint16_t toCopy = min<uint16_t>(bufsize, static_cast<uint16_t>(_value_len - offset));
  memcpy(buffer, &_value[offset], toCopy);
  return toCopy;
}

uint8_t BLECharacteristic::read8() {
  uint8_t value = 0U;
  read(&value, sizeof(value));
  return value;
}

uint16_t BLECharacteristic::read16() {
  uint16_t value = 0U;
  read(&value, sizeof(value));
  return value;
}

uint32_t BLECharacteristic::read32() {
  uint32_t value = 0UL;
  read(&value, sizeof(value));
  return value;
}

float BLECharacteristic::readFloat() {
  float value = 0.0f;
  read(&value, sizeof(value));
  return value;
}

uint16_t BLECharacteristic::getCccd(uint16_t conn_hdl) {
  (void)conn_hdl;
  uint16_t value = 0U;
  if (notifyEnabled()) {
    value |= 0x0001U;
  }
  if (indicateEnabled()) {
    value |= 0x0002U;
  }
  return value;
}

bool BLECharacteristic::notifyEnabled() { return notifyEnabled(0U); }

bool BLECharacteristic::notifyEnabled(uint16_t conn_hdl) {
  (void)conn_hdl;
  return (_handles.value_handle != 0U) &&
         manager().radio().isCustomGattCccdEnabled(_handles.value_handle, false);
}

bool BLECharacteristic::notify(const void* data, uint16_t len) {
  return notify(0U, data, len);
}

bool BLECharacteristic::notify(const char* str) {
  return notify(0U, str, (str != nullptr) ? static_cast<uint16_t>(strlen(str)) : 0U);
}

bool BLECharacteristic::notify8(uint8_t num) { return notify(&num, sizeof(num)); }

bool BLECharacteristic::notify16(uint16_t num) { return notify(&num, sizeof(num)); }

bool BLECharacteristic::notify32(uint32_t num) { return notify(&num, sizeof(num)); }

bool BLECharacteristic::notify32(int num) { return notify(&num, sizeof(num)); }

bool BLECharacteristic::notify32(float num) { return notify(&num, sizeof(num)); }

bool BLECharacteristic::notify(uint16_t conn_hdl, const void* data, uint16_t len) {
  (void)conn_hdl;
  write(data, len);
  return (_handles.value_handle != 0U) &&
         manager().radio().notifyCustomGattCharacteristic(_handles.value_handle, false);
}

bool BLECharacteristic::notify(uint16_t conn_hdl, const char* str) {
  return notify(conn_hdl, str, (str != nullptr) ? static_cast<uint16_t>(strlen(str)) : 0U);
}

bool BLECharacteristic::notify8(uint16_t conn_hdl, uint8_t num) {
  return notify(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::notify16(uint16_t conn_hdl, uint16_t num) {
  return notify(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::notify32(uint16_t conn_hdl, uint32_t num) {
  return notify(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::notify32(uint16_t conn_hdl, int num) {
  return notify(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::notify32(uint16_t conn_hdl, float num) {
  return notify(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::indicateEnabled() { return indicateEnabled(0U); }

bool BLECharacteristic::indicateEnabled(uint16_t conn_hdl) {
  (void)conn_hdl;
  return (_handles.value_handle != 0U) &&
         manager().radio().isCustomGattCccdEnabled(_handles.value_handle, true);
}

bool BLECharacteristic::indicate(const void* data, uint16_t len) {
  return indicate(0U, data, len);
}

bool BLECharacteristic::indicate(const char* str) {
  return indicate(0U, str, (str != nullptr) ? static_cast<uint16_t>(strlen(str)) : 0U);
}

bool BLECharacteristic::indicate8(uint8_t num) { return indicate(&num, sizeof(num)); }

bool BLECharacteristic::indicate16(uint16_t num) { return indicate(&num, sizeof(num)); }

bool BLECharacteristic::indicate32(uint32_t num) { return indicate(&num, sizeof(num)); }

bool BLECharacteristic::indicate32(int num) { return indicate(&num, sizeof(num)); }

bool BLECharacteristic::indicate32(float num) { return indicate(&num, sizeof(num)); }

bool BLECharacteristic::indicate(uint16_t conn_hdl, const void* data, uint16_t len) {
  (void)conn_hdl;
  write(data, len);
  return (_handles.value_handle != 0U) &&
         manager().radio().notifyCustomGattCharacteristic(_handles.value_handle, true);
}

bool BLECharacteristic::indicate(uint16_t conn_hdl, const char* str) {
  return indicate(conn_hdl, str, (str != nullptr) ? static_cast<uint16_t>(strlen(str)) : 0U);
}

bool BLECharacteristic::indicate8(uint16_t conn_hdl, uint8_t num) {
  return indicate(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::indicate16(uint16_t conn_hdl, uint16_t num) {
  return indicate(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::indicate32(uint16_t conn_hdl, uint32_t num) {
  return indicate(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::indicate32(uint16_t conn_hdl, int num) {
  return indicate(conn_hdl, &num, sizeof(num));
}

bool BLECharacteristic::indicate32(uint16_t conn_hdl, float num) {
  return indicate(conn_hdl, &num, sizeof(num));
}

void BLECharacteristic::handleWriteFromRadio(const uint8_t* data, uint16_t len) {
  const uint8_t toCopy = clampValueLen(len);
  memset(_value, 0, sizeof(_value));
  if (data != nullptr && toCopy > 0U) {
    memcpy(_value, data, toCopy);
  }
  _value_len = toCopy;
  if (_userbuf != nullptr && _userbufsize > 0U) {
    const uint16_t copied = min<uint16_t>(_userbufsize, _value_len);
    memcpy(_userbuf, _value, copied);
  }
  if (_wr_cb != nullptr) {
    _wr_cb(0U, this, _value, _value_len);
  }
}

void BLECharacteristic::pollCccdState() {
  if (_handles.value_handle == 0U || _cccd_wr_cb == nullptr) {
    return;
  }
  const bool notify = manager().radio().isCustomGattCccdEnabled(_handles.value_handle, false);
  const bool indicate = manager().radio().isCustomGattCccdEnabled(_handles.value_handle, true);
  if (notify == _notify_enabled && indicate == _indicate_enabled) {
    return;
  }
  _notify_enabled = notify;
  _indicate_enabled = indicate;
  uint16_t cccd = 0U;
  if (notify) {
    cccd |= 0x0001U;
  }
  if (indicate) {
    cccd |= 0x0002U;
  }
  _cccd_wr_cb(0U, this, cccd);
}

BLEAdvertisingData::BLEAdvertisingData(bool scan_response)
    : data_{0}, len_(0U), scan_response_(scan_response), dirty_(true) {}

void BLEAdvertisingData::clearData() {
  memset(data_, 0, sizeof(data_));
  len_ = 0U;
  dirty_ = true;
}

bool BLEAdvertisingData::setData(const uint8_t* data, uint8_t len) {
  if ((data == nullptr && len > 0U) || len > sizeof(data_)) {
    return false;
  }
  memset(data_, 0, sizeof(data_));
  if (len > 0U) {
    memcpy(data_, data, len);
  }
  len_ = len;
  dirty_ = true;
  return true;
}

bool BLEAdvertisingData::addData(uint8_t type, const void* data, uint8_t len) {
  if ((data == nullptr && len > 0U) || (len_ + len + 2U) > sizeof(data_)) {
    return false;
  }
  data_[len_++] = static_cast<uint8_t>(len + 1U);
  data_[len_++] = type;
  if (len > 0U) {
    memcpy(&data_[len_], data, len);
    len_ = static_cast<uint8_t>(len_ + len);
  }
  dirty_ = true;
  return true;
}

bool BLEAdvertisingData::addFlags(uint8_t flags) { return addData(kAdTypeFlags, &flags, 1U); }

bool BLEAdvertisingData::addTxPower() {
  const int8_t txPower = Bluefruit.getTxPower();
  return addData(kAdTypeTxPower, &txPower, 1U);
}

bool BLEAdvertisingData::addName() {
  char name[CFG_MAX_DEVNAME_LEN + 1] = {0};
  const uint8_t nameLen = Bluefruit.getName(name, sizeof(name));
  if (nameLen == 0U) {
    return false;
  }
  const uint8_t maxLen = static_cast<uint8_t>((sizeof(data_) > (len_ + 2U))
                                                  ? (sizeof(data_) - len_ - 2U)
                                                  : 0U);
  if (maxLen == 0U) {
    return false;
  }
  const uint8_t actualLen = (nameLen > maxLen) ? maxLen : nameLen;
  const uint8_t type = (actualLen == nameLen) ? kAdTypeCompleteName : kAdTypeShortName;
  return addData(type, name, actualLen);
}

bool BLEAdvertisingData::addAppearance(uint16_t appearance) {
  return addData(kAdTypeAppearance, &appearance, sizeof(appearance));
}

bool BLEAdvertisingData::addManufacturerData(const void* data, uint8_t len) {
  return addData(kAdTypeManufacturer, data, len);
}

bool BLEAdvertisingData::addService(const BLEService& service) { return addService(service.uuid); }

bool BLEAdvertisingData::addService(const BLEService& service1, const BLEService& service2) {
  return addService(service1) && addService(service2);
}

bool BLEAdvertisingData::addService(const BLEService& service1, const BLEService& service2,
                                    const BLEService& service3) {
  return addService(service1) && addService(service2) && addService(service3);
}

bool BLEAdvertisingData::addService(const BLEService& service1, const BLEService& service2,
                                    const BLEService& service3,
                                    const BLEService& service4) {
  return addService(service1) && addService(service2) && addService(service3) &&
         addService(service4);
}

bool BLEAdvertisingData::addService(const BLEUuid& uuid) { return addUuid(uuid); }

bool BLEAdvertisingData::addUuid(const BLEUuid& uuid) {
  if (uuid.size() == 2U) {
    const uint16_t uuid16 = uuid.uuid16();
    return addData(kAdTypeComplete16, &uuid16, sizeof(uuid16));
  }
  if (uuid.size() == 16U) {
    return addData(kAdTypeComplete128, uuid.uuid128(), 16U);
  }
  return false;
}

bool BLEAdvertisingData::addUuid(const BLEUuid& uuid1, const BLEUuid& uuid2) {
  return addUuid(uuid1) && addUuid(uuid2);
}

bool BLEAdvertisingData::addUuid(const BLEUuid& uuid1, const BLEUuid& uuid2,
                                 const BLEUuid& uuid3) {
  return addUuid(uuid1) && addUuid(uuid2) && addUuid(uuid3);
}

bool BLEAdvertisingData::addUuid(const BLEUuid& uuid1, const BLEUuid& uuid2,
                                 const BLEUuid& uuid3, const BLEUuid& uuid4) {
  return addUuid(uuid1) && addUuid(uuid2) && addUuid(uuid3) && addUuid(uuid4);
}

bool BLEAdvertisingData::addUuid(const BLEUuid uuids[], uint8_t count) {
  if (uuids == nullptr) {
    return false;
  }
  for (uint8_t i = 0U; i < count; ++i) {
    if (!addUuid(uuids[i])) {
      return false;
    }
  }
  return true;
}

uint8_t BLEAdvertisingData::count() const { return len_; }

uint8_t* BLEAdvertisingData::getData() { return data_; }

BLEAdvertising::BLEAdvertising()
    : BLEAdvertisingData(false),
      restart_on_disconnect_(true),
      interval_fast_(32U),
      interval_slow_(244U),
      fast_timeout_s_(30U),
      stop_timeout_s_(0U),
      adv_type_(static_cast<uint8_t>(xiao_nrf54l15::BleAdvPduType::kAdvInd)),
      running_(false),
      slow_callback_(nullptr),
      stop_callback_(nullptr) {}

void BLEAdvertising::restartOnDisconnect(bool enable) { restart_on_disconnect_ = enable; }

void BLEAdvertising::setInterval(uint16_t fast, uint16_t slow) {
  interval_fast_ = fast;
  interval_slow_ = slow;
}

void BLEAdvertising::setIntervalMS(uint16_t fast_ms, uint16_t slow_ms) {
  setInterval(MS1000TO625(fast_ms), (slow_ms == 0U) ? 0U : MS1000TO625(slow_ms));
}

void BLEAdvertising::setFastTimeout(uint16_t seconds) { fast_timeout_s_ = seconds; }

void BLEAdvertising::setType(uint8_t type) { adv_type_ = type; }

void BLEAdvertising::setSlowCallback(slow_callback_t cb) { slow_callback_ = cb; }

void BLEAdvertising::setStopCallback(stop_callback_t cb) { stop_callback_ = cb; }

uint16_t BLEAdvertising::getInterval() const { return interval_fast_; }

bool BLEAdvertising::setBeacon(BLEBeacon& beacon) { return beacon.start(*this); }

bool BLEAdvertising::start(uint16_t timeout) {
  if (!manager().begin(1U, 0U)) {
    return false;
  }
  stop_timeout_s_ = timeout;
  running_ = true;
  manager().markAdvertisingDirty();
  manager().advertisingStarted();
  return true;
}

bool BLEAdvertising::stop() {
  running_ = false;
  return true;
}

bool BLEAdvertising::isRunning() const { return running_; }

BLEBeacon::BLEBeacon() { reset(); }

BLEBeacon::BLEBeacon(const uint8_t uuid128[16]) {
  reset();
  setUuid(uuid128);
}

BLEBeacon::BLEBeacon(const uint8_t uuid128[16], uint16_t major, uint16_t minor, int8_t rssi) {
  reset();
  setUuid(uuid128);
  setMajorMinor(major, minor);
  setRssiAt1m(rssi);
}

void BLEBeacon::reset() {
  manufacturer_ = kCompanyIdApple;
  memset(uuid128_, 0, sizeof(uuid128_));
  uuid_valid_ = false;
  major_be_ = 0U;
  minor_be_ = 0U;
  rssi_at_1m_ = -54;
}

void BLEBeacon::setManufacturer(uint16_t manufacturer) { manufacturer_ = manufacturer; }

void BLEBeacon::setUuid(const uint8_t uuid128[16]) {
  if (uuid128 == nullptr) {
    memset(uuid128_, 0, sizeof(uuid128_));
    uuid_valid_ = false;
    return;
  }
  memcpy(uuid128_, uuid128, sizeof(uuid128_));
  uuid_valid_ = true;
}

void BLEBeacon::setMajorMinor(uint16_t major, uint16_t minor) {
  major_be_ = byteSwap16(major);
  minor_be_ = byteSwap16(minor);
}

void BLEBeacon::setRssiAt1m(int8_t rssi) { rssi_at_1m_ = rssi; }

bool BLEBeacon::start() { return start(Bluefruit.Advertising); }

bool BLEBeacon::start(BLEAdvertising& adv) {
  if (!uuid_valid_) {
    return false;
  }

  struct {
    uint16_t manufacturer;
    uint8_t beacon_type;
    uint8_t beacon_len;
    uint8_t uuid128[16];
    uint16_t major;
    uint16_t minor;
    int8_t rssi_at_1m;
  } payload{};

  payload.manufacturer = manufacturer_;
  payload.beacon_type = 0x02U;
  payload.beacon_len = 21U;
  memcpy(payload.uuid128, uuid128_, sizeof(payload.uuid128));
  payload.major = major_be_;
  payload.minor = minor_be_;
  payload.rssi_at_1m = rssi_at_1m_;

  adv.clearData();
  return adv.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE) &&
         adv.addData(BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, &payload,
                     sizeof(payload));
}

BLEPeriph::BLEPeriph()
    : connect_callback_(nullptr),
      disconnect_callback_(nullptr),
      conn_interval_min_(BLE_GAP_CONN_MIN_INTERVAL_DFLT),
      conn_interval_max_(BLE_GAP_CONN_MAX_INTERVAL_DFLT) {}

void BLEPeriph::setConnectCallback(ble_connect_callback_t fp) { connect_callback_ = fp; }

void BLEPeriph::setDisconnectCallback(ble_disconnect_callback_t fp) {
  disconnect_callback_ = fp;
}

void BLEPeriph::setConnInterval(uint16_t min_interval, uint16_t max_interval) {
  conn_interval_min_ = min_interval;
  conn_interval_max_ = (max_interval == 0U) ? min_interval : max_interval;
}

BLEConnection::BLEConnection() : handle_(INVALID_CONNECTION_HANDLE) {}

uint16_t BLEConnection::handle() const { return handle_; }

bool BLEConnection::getPeerName(char* name, uint16_t bufsize) const {
  if (name == nullptr || bufsize == 0U || !manager().radio().isConnected()) {
    return false;
  }
  BleConnectionInfo info{};
  if (!manager().radio().getConnectionInfo(&info)) {
    return false;
  }
  snprintf(name, bufsize, "%02X:%02X:%02X:%02X:%02X:%02X", info.peerAddress[5],
           info.peerAddress[4], info.peerAddress[3], info.peerAddress[2], info.peerAddress[1],
           info.peerAddress[0]);
  return true;
}

BLEDis::BLEDis() : BLEService(UUID16_SVC_DEVICE_INFORMATION), values_{nullptr}, lengths_{0} {}

void BLEDis::setSystemID(const char* system_id, uint8_t length) {
  values_[0] = system_id;
  lengths_[0] = length;
}

void BLEDis::setModel(const char* model, uint8_t length) {
  values_[1] = model;
  lengths_[1] = length;
}

void BLEDis::setSerialNum(const char* serial_num, uint8_t length) {
  values_[2] = serial_num;
  lengths_[2] = length;
}

void BLEDis::setFirmwareRev(const char* firmware_rev, uint8_t length) {
  values_[3] = firmware_rev;
  lengths_[3] = length;
}

void BLEDis::setHardwareRev(const char* hw_rev, uint8_t length) {
  values_[4] = hw_rev;
  lengths_[4] = length;
}

void BLEDis::setSoftwareRev(const char* sw_rev, uint8_t length) {
  values_[5] = sw_rev;
  lengths_[5] = length;
}

void BLEDis::setManufacturer(const char* manufacturer, uint8_t length) {
  values_[6] = manufacturer;
  lengths_[6] = length;
}

void BLEDis::setRegCertList(const char* reg_cert_list, uint8_t length) {
  values_[7] = reg_cert_list;
  lengths_[7] = length;
}

void BLEDis::setPNPID(const char* pnp_id, uint8_t length) {
  values_[8] = pnp_id;
  lengths_[8] = length;
}

err_t BLEDis::begin() {
  static const uint16_t kCharacteristicUuids[9] = {
      UUID16_CHR_SYSTEM_ID,        UUID16_CHR_MODEL_NUMBER_STRING,
      UUID16_CHR_SERIAL_NUMBER_STRING, UUID16_CHR_FIRMWARE_REVISION_STRING,
      UUID16_CHR_HARDWARE_REVISION_STRING, UUID16_CHR_SOFTWARE_REVISION_STRING,
      UUID16_CHR_MANUFACTURER_NAME_STRING, UUID16_CHR_REGULATORY_CERT_DATA_LIST,
      UUID16_CHR_PNP_ID};

  const err_t status = BLEService::begin();
  if (status != ERROR_NONE) {
    return status;
  }

  for (uint8_t i = 0U; i < 9U; ++i) {
    if (values_[i] == nullptr || lengths_[i] == 0U) {
      continue;
    }
    const uint8_t len = clampValueLen(lengths_[i]);
    if (!manager().radio().addCustomGattCharacteristic(
            _handle, kCharacteristicUuids[i], xiao_nrf54l15::kBleGattPropRead,
            reinterpret_cast<const uint8_t*>(values_[i]), len, nullptr, nullptr)) {
      return ERROR_INVALID_STATE;
    }
  }
  return ERROR_NONE;
}

BLEDfu::BLEDfu() : BLEService(kUuidDfuService) {}

err_t BLEDfu::begin() {
  _begun = true;
  lastService = this;
  return ERROR_NONE;
}

BLEBas::BLEBas() : BLEService(UUID16_SVC_BATTERY) {}

err_t BLEBas::begin() {
  _begun = true;
  lastService = this;
  return ERROR_NONE;
}

bool BLEBas::write(uint8_t level) { return manager().radio().setGattBatteryLevel(level); }

bool BLEBas::notify(uint8_t level) { return notify(0U, level); }

bool BLEBas::notify(uint16_t conn_hdl, uint8_t level) {
  (void)conn_hdl;
  return write(level);
}

BLEUart::BLEUart(uint16_t fifo_depth)
    : BLEService(BLEUART_UUID_SERVICE),
      _txd(BLEUART_UUID_CHR_TXD),
      _rxd(BLEUART_UUID_CHR_RXD),
      _rx_fifo_depth((fifo_depth == 0U) ? 1U : fifo_depth),
      _rx_fifo(new uint8_t[(fifo_depth == 0U) ? 1U : fifo_depth]),
      _rx_head(0U),
      _rx_tail(0U),
      _rx_count(0U),
      _tx_buffered(false),
      _rx_cb(nullptr),
      _notify_cb(nullptr),
      _overflow_cb(nullptr) {}

err_t BLEUart::begin() {
  const err_t status = BLEService::begin();
  if (status != ERROR_NONE) {
    return status;
  }

  _txd.setProperties(CHR_PROPS_NOTIFY);
  _txd.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  _txd.setMaxLen(BleRadio::kCustomGattMaxValueLength);
  _txd.setCccdWriteCallback(bleuart_txd_cccd_cb);
  if (_txd.begin() != ERROR_NONE) {
    return ERROR_INVALID_STATE;
  }

  _rxd.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
  _rxd.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  _rxd.setMaxLen(BleRadio::kCustomGattMaxValueLength);
  _rxd.setWriteCallback(bleuart_rxd_cb);
  if (_rxd.begin() != ERROR_NONE) {
    return ERROR_INVALID_STATE;
  }

  return ERROR_NONE;
}

bool BLEUart::notifyEnabled() { return _txd.notifyEnabled(); }

bool BLEUart::notifyEnabled(uint16_t conn_hdl) { return _txd.notifyEnabled(conn_hdl); }

void BLEUart::setRxCallback(rx_callback_t fp, bool deferred) {
  (void)deferred;
  _rx_cb = fp;
}

void BLEUart::setRxOverflowCallback(rx_overflow_callback_t fp) { _overflow_cb = fp; }

void BLEUart::setNotifyCallback(notify_callback_t fp) { _notify_cb = fp; }

void BLEUart::bufferTXD(bool enable) { _tx_buffered = enable; }

bool BLEUart::flushTXD() { return flushTXD(0U); }

bool BLEUart::flushTXD(uint16_t conn_hdl) {
  (void)conn_hdl;
  return true;
}

uint8_t BLEUart::read8() { return static_cast<uint8_t>(read()); }

uint16_t BLEUart::read16() {
  uint16_t value = 0U;
  read(reinterpret_cast<uint8_t*>(&value), sizeof(value));
  return value;
}

uint32_t BLEUart::read32() {
  uint32_t value = 0UL;
  read(reinterpret_cast<uint8_t*>(&value), sizeof(value));
  return value;
}

int BLEUart::read() {
  if (_rx_count == 0U) {
    return -1;
  }
  const int value = _rx_fifo[_rx_tail];
  _rx_tail = static_cast<uint16_t>((_rx_tail + 1U) % _rx_fifo_depth);
  --_rx_count;
  return value;
}

int BLEUart::read(uint8_t* buf, size_t size) {
  if (buf == nullptr || size == 0U) {
    return 0;
  }
  size_t copied = 0U;
  while (copied < size) {
    const int ch = read();
    if (ch < 0) {
      break;
    }
    buf[copied++] = static_cast<uint8_t>(ch);
  }
  return static_cast<int>(copied);
}

size_t BLEUart::write(uint8_t b) { return write(0U, &b, 1U); }

size_t BLEUart::write(const uint8_t* content, size_t len) { return write(0U, content, len); }

size_t BLEUart::write(uint16_t conn_hdl, uint8_t b) { return write(conn_hdl, &b, 1U); }

size_t BLEUart::write(uint16_t conn_hdl, const uint8_t* content, size_t len) {
  (void)conn_hdl;
  if (content == nullptr || len == 0U || !notifyEnabled()) {
    return 0U;
  }

  size_t sent = 0U;
  while (sent < len) {
    const uint16_t chunk =
        min<uint16_t>(BleRadio::kCustomGattMaxValueLength, static_cast<uint16_t>(len - sent));
    if (!_txd.notify(&content[sent], chunk)) {
      break;
    }
    sent += chunk;
    if (sent < len) {
      delay(5);
    }
  }
  return sent;
}

int BLEUart::available() { return _rx_count; }

int BLEUart::peek() {
  if (_rx_count == 0U) {
    return -1;
  }
  return _rx_fifo[_rx_tail];
}

void BLEUart::flush() {}

void BLEUart::handleRx(uint16_t conn_hdl, const uint8_t* data, uint16_t len) {
  uint16_t dropped = 0U;
  for (uint16_t i = 0U; i < len; ++i) {
    if (_rx_count >= _rx_fifo_depth) {
      ++dropped;
      continue;
    }
    _rx_fifo[_rx_head] = data[i];
    _rx_head = static_cast<uint16_t>((_rx_head + 1U) % _rx_fifo_depth);
    ++_rx_count;
  }
  if (dropped > 0U && _overflow_cb != nullptr) {
    _overflow_cb(conn_hdl, dropped);
  }
  if (_rx_cb != nullptr) {
    _rx_cb(conn_hdl);
  }
}

void BLEUart::bleuart_rxd_cb(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data,
                             uint16_t len) {
  auto& service = static_cast<BLEUart&>(chr->parentService());
  service.handleRx(conn_hdl, data, len);
}

void BLEUart::bleuart_txd_cccd_cb(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t value) {
  auto& service = static_cast<BLEUart&>(chr->parentService());
  if (service._notify_cb != nullptr) {
    service._notify_cb(conn_hdl, (value & 0x0001U) != 0U);
  }
}

AdafruitBluefruit::AdafruitBluefruit()
    : Periph(),
      Central(),
      Security(),
      Gatt(),
      Advertising(),
      ScanResponse(true),
      Scanner(),
      Discovery(),
      device_name_{0},
      tx_power_(0),
      appearance_(0),
      auto_conn_led_(false),
      conn_led_interval_ms_(500UL) {
  strncpy(device_name_, "XIAO nRF54L15", sizeof(device_name_) - 1U);
}

void AdafruitBluefruit::configServiceChanged(bool changed) { (void)changed; }

void AdafruitBluefruit::configUuid128Count(uint8_t uuid128_max) { (void)uuid128_max; }

void AdafruitBluefruit::configAttrTableSize(uint32_t attr_table_size) {
  (void)attr_table_size;
}

void AdafruitBluefruit::configPrphConn(uint16_t mtu_max, uint16_t event_len,
                                       uint8_t hvn_qsize, uint8_t wrcmd_qsize) {
  (void)mtu_max;
  (void)event_len;
  (void)hvn_qsize;
  (void)wrcmd_qsize;
}

void AdafruitBluefruit::configCentralConn(uint16_t mtu_max, uint16_t event_len,
                                          uint8_t hvn_qsize, uint8_t wrcmd_qsize) {
  (void)mtu_max;
  (void)event_len;
  (void)hvn_qsize;
  (void)wrcmd_qsize;
}

void AdafruitBluefruit::configPrphBandwidth(uint8_t bw) { (void)bw; }

void AdafruitBluefruit::configCentralBandwidth(uint8_t bw) { (void)bw; }

bool AdafruitBluefruit::begin(uint8_t prph_count, uint8_t central_count) {
  return manager().begin(prph_count, central_count);
}

void AdafruitBluefruit::setName(const char* str) {
  if (str == nullptr) {
    return;
  }
  strncpy(device_name_, str, CFG_MAX_DEVNAME_LEN);
  device_name_[CFG_MAX_DEVNAME_LEN] = '\0';
  manager().radio().setGattDeviceName(device_name_);
  manager().markAdvertisingDirty();
}

uint8_t AdafruitBluefruit::getName(char* name, uint16_t bufsize) {
  if (name == nullptr || bufsize == 0U) {
    return 0U;
  }
  const size_t len = strlen(device_name_);
  const uint16_t copyLen = min<uint16_t>(bufsize - 1U, static_cast<uint16_t>(len));
  memcpy(name, device_name_, copyLen);
  name[copyLen] = '\0';
  return static_cast<uint8_t>(copyLen);
}

bool AdafruitBluefruit::setTxPower(int8_t power) {
  tx_power_ = power;
  manager().markAdvertisingDirty();
  return manager().radio().setTxPowerDbm(power);
}

int8_t AdafruitBluefruit::getTxPower() const { return tx_power_; }

bool AdafruitBluefruit::setAppearance(uint16_t appearance) {
  appearance_ = appearance;
  return manager().radio().setGattAppearance(appearance);
}

uint16_t AdafruitBluefruit::getAppearance() const { return appearance_; }

void AdafruitBluefruit::autoConnLed(bool enabled) {
  auto_conn_led_ = enabled;
  if (enabled) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN,
                 manager().radio().isConnected() ? kLedOnState : kLedOffState);
  }
}

void AdafruitBluefruit::setConnLedInterval(uint32_t ms) { conn_led_interval_ms_ = ms; }

uint8_t AdafruitBluefruit::connected() const { return manager().radio().isConnected() ? 1U : 0U; }

bool AdafruitBluefruit::connected(uint16_t conn_hdl) const {
  return (conn_hdl == 0U) && manager().radio().isConnected();
}

uint8_t AdafruitBluefruit::getConnectedHandles(uint16_t* hdl_list, uint8_t max_count) const {
  if (hdl_list == nullptr || max_count == 0U || !manager().radio().isConnected()) {
    return 0U;
  }
  hdl_list[0] = 0U;
  return 1U;
}

uint16_t AdafruitBluefruit::connHandle() const {
  return manager().radio().isConnected() ? 0U : INVALID_CONNECTION_HANDLE;
}

bool AdafruitBluefruit::disconnect(uint16_t conn_hdl) {
  if (conn_hdl != 0U || !manager().radio().isConnected()) {
    return false;
  }
  return manager().radio().disconnect();
}

BLEConnection* AdafruitBluefruit::Connection(uint16_t conn_hdl) {
  if (conn_hdl != 0U || !manager().radio().isConnected()) {
    return nullptr;
  }
  return manager().connection();
}
