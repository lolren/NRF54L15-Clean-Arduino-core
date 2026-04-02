#ifndef BLUEFRUIT_H_
#define BLUEFRUIT_H_

#include <Arduino.h>

#include "bluefruit_common.h"

class BluefruitCompatManager;

enum {
  BANDWIDTH_AUTO = 0,
  BANDWIDTH_LOW,
  BANDWIDTH_NORMAL,
  BANDWIDTH_HIGH,
  BANDWIDTH_MAX,
};

enum CharsProperties {
  CHR_PROPS_BROADCAST = bit(0),
  CHR_PROPS_READ = bit(1),
  CHR_PROPS_WRITE_WO_RESP = bit(2),
  CHR_PROPS_WRITE = bit(3),
  CHR_PROPS_NOTIFY = bit(4),
  CHR_PROPS_INDICATE = bit(5),
};

enum BLECharsProperties {
  BLEBroadcast = 0x01,
  BLERead = 0x02,
  BLEWriteWithoutResponse = 0x04,
  BLEWrite = 0x08,
  BLENotify = 0x10,
  BLEIndicate = 0x20,
};

class BLEUuid {
 public:
  BLEUuid();
  BLEUuid(uint16_t uuid16);
  BLEUuid(const uint8_t uuid128[16]);
  BLEUuid(const char* str);

  void set(uint16_t uuid16);
  void set(const uint8_t uuid128[16]);
  void set(const char* str);

  bool get(uint16_t* uuid16) const;
  bool get(uint8_t uuid128[16]) const;
  size_t size() const;
  bool begin() const;
  String toString() const;

  bool operator==(const BLEUuid& rhs) const;
  bool operator!=(const BLEUuid& rhs) const;

  uint16_t uuid16() const;
  const uint8_t* uuid128() const;

 private:
  uint8_t size_;
  uint16_t uuid16_;
  uint8_t uuid128_[16];
};

class BLEService {
 public:
  static BLEService* lastService;

  BLEUuid uuid;

  BLEService();
  BLEService(BLEUuid bleuuid);
  virtual ~BLEService() = default;

  void setUuid(BLEUuid bleuuid);
  void setPermission(SecureMode_t read_perm, SecureMode_t write_perm);
  void getPermission(SecureMode_t* read_perm, SecureMode_t* write_perm) const;

  virtual err_t begin();

 protected:
  uint16_t _handle;
  SecureMode_t _read_perm;
  SecureMode_t _write_perm;
  bool _begun;

  friend class BLECharacteristic;
  friend class BluefruitCompatManager;
};

class BLECharacteristic {
 public:
  typedef void (*read_authorize_cb_t)(uint16_t conn_hdl, BLECharacteristic* chr,
                                      ble_gatts_evt_read_t* request);
  typedef void (*write_authorize_cb_t)(uint16_t conn_hdl, BLECharacteristic* chr,
                                       ble_gatts_evt_write_t* request);
  typedef void (*write_cb_t)(uint16_t conn_hdl, BLECharacteristic* chr,
                             uint8_t* data, uint16_t len);
  typedef void (*write_cccd_cb_t)(uint16_t conn_hdl, BLECharacteristic* chr,
                                  uint16_t value);

  BLEUuid uuid;

  BLECharacteristic();
  BLECharacteristic(BLEUuid bleuuid);
  BLECharacteristic(BLEUuid bleuuid, uint8_t properties);
  BLECharacteristic(BLEUuid bleuuid, uint8_t properties, int max_len,
                    bool fixed_len = false);
  virtual ~BLECharacteristic() = default;

  BLEService& parentService();

  void setTempMemory();
  void setUuid(BLEUuid bleuuid);
  void setProperties(uint8_t prop);
  void setPermission(SecureMode_t read_perm, SecureMode_t write_perm);
  void setMaxLen(uint16_t max_len);
  void setFixedLen(uint16_t fixed_len);
  void setBuffer(void* buf, uint16_t bufsize);

  uint16_t getMaxLen() const;
  bool isFixedLen() const;

  void setUserDescriptor(const char* descriptor);
  void setReportRefDescriptor(uint8_t id, uint8_t type);
  void setPresentationFormatDescriptor(uint8_t type, int8_t exponent,
                                       uint16_t unit, uint8_t name_space = 1,
                                       uint16_t descriptor = 0);

  void setWriteCallback(write_cb_t fp, bool useAdaCallback = true);
  void setCccdWriteCallback(write_cccd_cb_t fp, bool useAdaCallback = true);
  void setReadAuthorizeCallback(read_authorize_cb_t fp,
                                bool useAdaCallback = true);
  void setWriteAuthorizeCallback(write_authorize_cb_t fp,
                                 bool useAdaCallback = true);

  virtual err_t begin();

  ble_gatts_char_handles_t handles() const;

  uint16_t write(const void* data, uint16_t len);
  uint16_t write(const char* str);
  uint16_t write8(uint8_t num);
  uint16_t write16(uint16_t num);
  uint16_t write32(uint32_t num);
  uint16_t write32(int num);
  uint16_t writeFloat(float num);

  uint16_t read(void* buffer, uint16_t bufsize, uint16_t offset = 0);
  uint8_t read8();
  uint16_t read16();
  uint32_t read32();
  float readFloat();

  uint16_t getCccd(uint16_t conn_hdl = 0);
  bool notifyEnabled();
  bool notifyEnabled(uint16_t conn_hdl);

  bool notify(const void* data, uint16_t len);
  bool notify(const char* str);
  bool notify8(uint8_t num);
  bool notify16(uint16_t num);
  bool notify32(uint32_t num);
  bool notify32(int num);
  bool notify32(float num);

  bool notify(uint16_t conn_hdl, const void* data, uint16_t len);
  bool notify(uint16_t conn_hdl, const char* str);
  bool notify8(uint16_t conn_hdl, uint8_t num);
  bool notify16(uint16_t conn_hdl, uint16_t num);
  bool notify32(uint16_t conn_hdl, uint32_t num);
  bool notify32(uint16_t conn_hdl, int num);
  bool notify32(uint16_t conn_hdl, float num);

  bool indicateEnabled();
  bool indicateEnabled(uint16_t conn_hdl);

  bool indicate(const void* data, uint16_t len);
  bool indicate(const char* str);
  bool indicate8(uint8_t num);
  bool indicate16(uint16_t num);
  bool indicate32(uint32_t num);
  bool indicate32(int num);
  bool indicate32(float num);

  bool indicate(uint16_t conn_hdl, const void* data, uint16_t len);
  bool indicate(uint16_t conn_hdl, const char* str);
  bool indicate8(uint16_t conn_hdl, uint8_t num);
  bool indicate16(uint16_t conn_hdl, uint16_t num);
  bool indicate32(uint16_t conn_hdl, uint32_t num);
  bool indicate32(uint16_t conn_hdl, int num);
  bool indicate32(uint16_t conn_hdl, float num);

 protected:
  bool _is_temp;
  uint16_t _max_len;
  bool _fixed_len;
  BLEService* _service;
  uint8_t* _userbuf;
  uint16_t _userbufsize;
  uint8_t _properties;
  SecureMode_t _read_perm;
  SecureMode_t _write_perm;
  ble_gatts_char_handles_t _handles;
  uint8_t _value[20];
  uint8_t _value_len;
  bool _notify_enabled;
  bool _indicate_enabled;
  const char* _usr_descriptor;
  write_cb_t _wr_cb;
  write_cccd_cb_t _cccd_wr_cb;
  read_authorize_cb_t _rd_authorize_cb;
  write_authorize_cb_t _wr_authorize_cb;

  void handleWriteFromRadio(const uint8_t* data, uint16_t len);
  void pollCccdState();

  friend class BluefruitCompatManager;
};

class BLEAdvertisingData {
 public:
  explicit BLEAdvertisingData(bool scan_response = false);

  void clearData();
  bool setData(const uint8_t* data, uint8_t len);
  bool addData(uint8_t type, const void* data, uint8_t len);
  bool addFlags(uint8_t flags);
  bool addTxPower();
  bool addName();
  bool addAppearance(uint16_t appearance);
  bool addManufacturerData(const void* data, uint8_t len);
  bool addService(const BLEService& service);
  bool addService(const BLEService& service1, const BLEService& service2);
  bool addService(const BLEService& service1, const BLEService& service2,
                  const BLEService& service3);
  bool addService(const BLEService& service1, const BLEService& service2,
                  const BLEService& service3, const BLEService& service4);
  bool addService(const BLEUuid& uuid);
  bool addUuid(const BLEUuid& uuid1, const BLEUuid& uuid2);
  bool addUuid(const BLEUuid& uuid1, const BLEUuid& uuid2, const BLEUuid& uuid3);
  bool addUuid(const BLEUuid& uuid1, const BLEUuid& uuid2, const BLEUuid& uuid3,
               const BLEUuid& uuid4);
  bool addUuid(const BLEUuid uuids[], uint8_t count);
  bool addUuid(const BLEUuid& uuid);
  uint8_t count() const;
  uint8_t* getData();

 protected:
  uint8_t data_[31];
  uint8_t len_;
  bool scan_response_;
  bool dirty_;

  friend class BluefruitCompatManager;
};

class BLEAdvertising : public BLEAdvertisingData {
 public:
  typedef void (*stop_callback_t)();
  typedef void (*slow_callback_t)();

  BLEAdvertising();

  void restartOnDisconnect(bool enable);
  void setInterval(uint16_t fast, uint16_t slow = 0);
  void setIntervalMS(uint16_t fast_ms, uint16_t slow_ms = 0);
  void setFastTimeout(uint16_t seconds);
  void setType(uint8_t type);
  void setSlowCallback(slow_callback_t cb);
  void setStopCallback(stop_callback_t cb);
  uint16_t getInterval() const;
  bool setBeacon(class BLEBeacon& beacon);

  bool start(uint16_t timeout = 0);
  bool stop();
  bool isRunning() const;

 private:
  bool restart_on_disconnect_;
  uint16_t interval_fast_;
  uint16_t interval_slow_;
  uint16_t fast_timeout_s_;
  uint16_t stop_timeout_s_;
  uint8_t adv_type_;
  bool running_;
  slow_callback_t slow_callback_;
  stop_callback_t stop_callback_;

  friend class BluefruitCompatManager;
};

class BLEBeacon {
 public:
  BLEBeacon();
  explicit BLEBeacon(const uint8_t uuid128[16]);
  BLEBeacon(const uint8_t uuid128[16], uint16_t major, uint16_t minor, int8_t rssi);

  void setManufacturer(uint16_t manufacturer);
  void setUuid(const uint8_t uuid128[16]);
  void setMajorMinor(uint16_t major, uint16_t minor);
  void setRssiAt1m(int8_t rssi);

  bool start();
  bool start(BLEAdvertising& adv);

 private:
  void reset();

  uint16_t manufacturer_;
  uint8_t uuid128_[16];
  bool uuid_valid_;
  uint16_t major_be_;
  uint16_t minor_be_;
  int8_t rssi_at_1m_;
};

class BLEPeriph {
 public:
  BLEPeriph();

  void setConnectCallback(ble_connect_callback_t fp);
  void setDisconnectCallback(ble_disconnect_callback_t fp);
  void setConnInterval(uint16_t min_interval, uint16_t max_interval = 0);

 private:
  ble_connect_callback_t connect_callback_;
  ble_disconnect_callback_t disconnect_callback_;
  uint16_t conn_interval_min_;
  uint16_t conn_interval_max_;

  friend class BluefruitCompatManager;
};

class BLEConnection {
 public:
  BLEConnection();

  uint16_t handle() const;
  bool getPeerName(char* name, uint16_t bufsize) const;
  bool requestPHY() { return false; }
  bool requestDataLengthUpdate() { return false; }
  bool requestMtuExchange(uint16_t mtu) {
    (void)mtu;
    return false;
  }
  uint16_t getMtu() const { return 23U; }

 private:
  uint16_t handle_;

  friend class BluefruitCompatManager;
};

class BLECentral {
 public:
  template <typename T>
  void setConnectCallback(T) {}
  template <typename T>
  void setDisconnectCallback(T) {}
  bool connect(const ble_gap_evt_adv_report_t*) { return false; }
  bool connected() const { return false; }
  void clearBonds() {}
};

class BLEScanner {
 public:
  template <typename T>
  void setRxCallback(T) {}
  void setInterval(uint16_t, uint16_t = 0) {}
  void useActiveScan(bool) {}
  void restartOnDisconnect(bool) {}
  void filterUuid(const BLEUuid&) {}
  void filterService(const BLEUuid&) {}
  void filterMSD(uint16_t) {}
  void filterRssi(int8_t) {}
  void start(uint16_t) {}
  void resume() {}
  bool checkReportForUuid(const ble_gap_evt_adv_report_t*, const BLEUuid&) const {
    return false;
  }
  bool checkReportForUuid(const ble_gap_evt_adv_report_t*,
                          const uint8_t uuid128[16]) const {
    (void)uuid128;
    return false;
  }
  bool checkReportForService(const ble_gap_evt_adv_report_t*, const BLEUuid&) const {
    return false;
  }
  bool checkReportForService(const ble_gap_evt_adv_report_t*,
                             const class BLEClientService&) const {
    return false;
  }
  bool checkReportForService(const ble_gap_evt_adv_report_t*,
                             const class BLEClientUart&) const {
    return false;
  }
  int parseReportByType(const ble_gap_evt_adv_report_t*, uint8_t, void*, uint8_t) const {
    return 0;
  }
};

class BLEDiscovery {};
class BLEGatt {};

class BLESecurity {
 public:
  template <typename T>
  void setSecuredCallback(T) {}
  template <typename T>
  void setPairPasskeyCallback(T) {}
  template <typename T>
  void setPairCompleteCallback(T) {}
  void setIOCaps(uint8_t) {}
  void setPIN(const char*) {}
};

class BLEClientCharacteristic {
 public:
  typedef void (*notify_cb_t)(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len);

  BLEUuid uuid;

  BLEClientCharacteristic()
      : uuid(), discovered_(false), notify_callback_(nullptr) {}
  explicit BLEClientCharacteristic(BLEUuid bleuuid)
      : uuid(bleuuid), discovered_(false), notify_callback_(nullptr) {}

  bool begin() { return true; }
  bool discover(uint16_t conn_hdl) {
    (void)conn_hdl;
    discovered_ = false;
    return false;
  }
  bool discover() {
    discovered_ = false;
    return false;
  }
  bool discovered() const { return discovered_; }
  void setNotifyCallback(notify_cb_t fp) { notify_callback_ = fp; }
  uint8_t read8() const { return 0U; }
  bool enableNotify() { return false; }

 protected:
  bool discovered_;
  notify_cb_t notify_callback_;
};

class BLEClientService {
 public:
  BLEUuid uuid;

  BLEClientService() : uuid(), discovered_(false) {}
  explicit BLEClientService(BLEUuid bleuuid) : uuid(bleuuid), discovered_(false) {}

  bool begin() { return true; }
  bool discover(uint16_t conn_hdl) {
    (void)conn_hdl;
    discovered_ = false;
    return false;
  }
  bool discovered() const { return discovered_; }

 protected:
  bool discovered_;
};
class BLEClientUart : public Stream {
 public:
  typedef void (*rx_callback_t)(BLEClientUart& uart_svc);

  BLEClientUart() : discovered_(false), rx_callback_(nullptr) {}

  bool begin() { return true; }
  bool discover(uint16_t conn_hdl) {
    (void)conn_hdl;
    discovered_ = false;
    return false;
  }
  bool discovered() const { return discovered_; }
  void setRxCallback(rx_callback_t fp) { rx_callback_ = fp; }
  bool enableTXD() { return false; }
  int read() override { return -1; }
  int available() override { return 0; }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t value) override {
    (void)value;
    return 0U;
  }
  using Print::write;

 private:
  bool discovered_;
  rx_callback_t rx_callback_;
};
class BLEClientDis : public BLEClientService {
 public:
  BLEClientDis() : BLEClientService(UUID16_SVC_DEVICE_INFORMATION) {}

  bool getManufacturer(char* buffer, uint16_t len) {
    if (buffer != nullptr && len > 0U) {
      buffer[0] = '\0';
    }
    return false;
  }

  bool getModel(char* buffer, uint16_t len) {
    if (buffer != nullptr && len > 0U) {
      buffer[0] = '\0';
    }
    return false;
  }
};
class BLEClientBas : public BLEClientService {
 public:
  BLEClientBas() : BLEClientService(UUID16_SVC_BATTERY) {}
  uint8_t read() const { return 0U; }
};
class BLEClientCts : public BLEClientService {
 public:
  BLEClientCts() : BLEClientService(UUID16_SVC_CURRENT_TIME) {}
};
class BLEAncs {};

class BLEDis : public BLEService {
 public:
  BLEDis();

  void setSystemID(const char* system_id, uint8_t length);
  void setModel(const char* model, uint8_t length);
  void setSerialNum(const char* serial_num, uint8_t length);
  void setFirmwareRev(const char* firmware_rev, uint8_t length);
  void setHardwareRev(const char* hw_rev, uint8_t length);
  void setSoftwareRev(const char* sw_rev, uint8_t length);
  void setManufacturer(const char* manufacturer, uint8_t length);
  void setRegCertList(const char* reg_cert_list, uint8_t length);
  void setPNPID(const char* pnp_id, uint8_t length);

  void setSystemID(const char* system_id) { setSystemID(system_id, strlen(system_id)); }
  void setModel(const char* model) { setModel(model, strlen(model)); }
  void setSerialNum(const char* serial_num) { setSerialNum(serial_num, strlen(serial_num)); }
  void setFirmwareRev(const char* firmware_rev) { setFirmwareRev(firmware_rev, strlen(firmware_rev)); }
  void setHardwareRev(const char* hw_rev) { setHardwareRev(hw_rev, strlen(hw_rev)); }
  void setSoftwareRev(const char* sw_rev) { setSoftwareRev(sw_rev, strlen(sw_rev)); }
  void setManufacturer(const char* manufacturer) { setManufacturer(manufacturer, strlen(manufacturer)); }
  void setRegCertList(const char* reg_cert_list) { setRegCertList(reg_cert_list, strlen(reg_cert_list)); }
  void setPNPID(const char* pnp_id) { setPNPID(pnp_id, strlen(pnp_id)); }

  err_t begin() override;

 private:
  const char* values_[9];
  uint8_t lengths_[9];
};

class BLEDfu : public BLEService {
 public:
  BLEDfu();
  err_t begin() override;
};

class BLEBas : public BLEService {
 public:
  BLEBas();

  err_t begin() override;
  bool write(uint8_t level);
  bool notify(uint8_t level);
  bool notify(uint16_t conn_hdl, uint8_t level);
};

class BLEUart : public BLEService, public Stream {
 public:
  typedef void (*rx_callback_t)(uint16_t conn_hdl);
  typedef void (*notify_callback_t)(uint16_t conn_hdl, bool enabled);
  typedef void (*rx_overflow_callback_t)(uint16_t conn_hdl, uint16_t leftover);

  explicit BLEUart(uint16_t fifo_depth = 256);

  err_t begin() override;

  bool notifyEnabled();
  bool notifyEnabled(uint16_t conn_hdl);

  void setRxCallback(rx_callback_t fp, bool deferred = true);
  void setRxOverflowCallback(rx_overflow_callback_t fp);
  void setNotifyCallback(notify_callback_t fp);

  void bufferTXD(bool enable);
  bool flushTXD();
  bool flushTXD(uint16_t conn_hdl);

  uint8_t read8();
  uint16_t read16();
  uint32_t read32();

  int read() override;
  int read(uint8_t* buf, size_t size);
  int read(char* buf, size_t size) { return read(reinterpret_cast<uint8_t*>(buf), size); }

  size_t write(uint8_t b) override;
  size_t write(const uint8_t* content, size_t len);
  size_t write(uint16_t conn_hdl, uint8_t b);
  size_t write(uint16_t conn_hdl, const uint8_t* content, size_t len);

  int available() override;
  int peek() override;
  void flush() override;

  using Print::write;

 private:
  BLECharacteristic _txd;
  BLECharacteristic _rxd;
  uint16_t _rx_fifo_depth;
  uint8_t* _rx_fifo;
  uint16_t _rx_head;
  uint16_t _rx_tail;
  uint16_t _rx_count;
  bool _tx_buffered;
  rx_callback_t _rx_cb;
  notify_callback_t _notify_cb;
  rx_overflow_callback_t _overflow_cb;

  void handleRx(uint16_t conn_hdl, const uint8_t* data, uint16_t len);
  static void bleuart_rxd_cb(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data,
                             uint16_t len);
  static void bleuart_txd_cccd_cb(uint16_t conn_hdl, BLECharacteristic* chr,
                                  uint16_t value);
};

extern const uint8_t BLEUART_UUID_SERVICE[16];
extern const uint8_t BLEUART_UUID_CHR_RXD[16];
extern const uint8_t BLEUART_UUID_CHR_TXD[16];

class AdafruitBluefruit {
 public:
  AdafruitBluefruit();

  BLEPeriph Periph;
  BLECentral Central;
  BLESecurity Security;
  BLEGatt Gatt;
  BLEAdvertising Advertising;
  BLEAdvertisingData ScanResponse;
  BLEScanner Scanner;
  BLEDiscovery Discovery;

  void configServiceChanged(bool changed);
  void configUuid128Count(uint8_t uuid128_max);
  void configAttrTableSize(uint32_t attr_table_size);
  void configPrphConn(uint16_t mtu_max, uint16_t event_len, uint8_t hvn_qsize,
                      uint8_t wrcmd_qsize);
  void configCentralConn(uint16_t mtu_max, uint16_t event_len, uint8_t hvn_qsize,
                         uint8_t wrcmd_qsize);
  void configPrphBandwidth(uint8_t bw);
  void configCentralBandwidth(uint8_t bw);

  bool begin(uint8_t prph_count = 1, uint8_t central_count = 0);

  void setName(const char* str);
  uint8_t getName(char* name, uint16_t bufsize);
  bool setTxPower(int8_t power);
  int8_t getTxPower() const;
  bool setAppearance(uint16_t appearance);
  uint16_t getAppearance() const;
  void autoConnLed(bool enabled);
  void setConnLedInterval(uint32_t ms);

  uint8_t connected() const;
  bool connected(uint16_t conn_hdl) const;
  uint8_t getConnectedHandles(uint16_t* hdl_list, uint8_t max_count) const;
  uint16_t connHandle() const;
  bool disconnect(uint16_t conn_hdl);
  BLEConnection* Connection(uint16_t conn_hdl);

 private:
  char device_name_[CFG_MAX_DEVNAME_LEN + 1];
  int8_t tx_power_;
  uint16_t appearance_;
  bool auto_conn_led_;
  uint32_t conn_led_interval_ms_;

  friend class BluefruitCompatManager;
};

extern AdafruitBluefruit Bluefruit;

#endif  // BLUEFRUIT_H_
