#ifndef BLE_NUS_H_
#define BLE_NUS_H_

#include <Arduino.h>

#include "nrf54l15_hal.h"

namespace xiao_nrf54l15 {

class BleNordicUart : public Stream {
 public:
  static constexpr size_t kRxBufferSize = 256U;
  static constexpr size_t kTxBufferSize = 256U;
  static constexpr uint8_t kMaxPayloadLength = BleRadio::kCustomGattMaxValueLength;

  static const uint8_t kServiceUuid128[16];
  static const uint8_t kRxCharacteristicUuid128[16];
  static const uint8_t kTxCharacteristicUuid128[16];

  explicit BleNordicUart(BleRadio& ble);

  bool begin();
  void end();
  void service(const BleConnectionEvent* event = nullptr);

  bool initialized() const;
  bool isConnected() const;
  bool isNotifyEnabled() const;
  bool hasPendingTx() const;

  uint16_t serviceHandle() const;
  uint16_t rxValueHandle() const;
  uint16_t txValueHandle() const;
  uint16_t txCccdHandle() const;

  uint32_t rxDroppedBytes() const;
  uint32_t txDroppedBytes() const;

  void clear();
  void clearRx();
  void clearTx();

  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  int availableForWrite() override;
  size_t write(uint8_t value) override;
  size_t write(const uint8_t* buffer, size_t size);

  using Print::write;

 private:
  static void onRxWriteThunk(uint16_t valueHandle, const uint8_t* value,
                             uint8_t valueLength, bool withResponse,
                             void* context);
  static bool eventSentNotificationForHandle(const BleConnectionEvent* event,
                                             uint16_t valueHandle);

  void onRxWrite(const uint8_t* value, uint8_t valueLength);
  bool queueNextNotification();
  size_t copyTxChunk(uint8_t* outChunk, size_t maxLength) const;
  void resetSessionState();

  BleRadio& ble_;
  uint16_t serviceHandle_;
  uint16_t rxValueHandle_;
  uint16_t txValueHandle_;
  uint16_t txCccdHandle_;
  uint16_t rxHead_;
  uint16_t rxTail_;
  uint16_t rxCount_;
  uint16_t txHead_;
  uint16_t txTail_;
  uint16_t txCount_;
  uint8_t rxBuffer_[kRxBufferSize];
  uint8_t txBuffer_[kTxBufferSize];
  uint8_t txChunk_[kMaxPayloadLength];
  uint8_t txChunkLength_;
  uint32_t rxDroppedBytes_;
  uint32_t txDroppedBytes_;
  bool initialized_;
  bool connected_;
  bool txNotificationInFlight_;
};

}  // namespace xiao_nrf54l15

#endif  // BLE_NUS_H_
