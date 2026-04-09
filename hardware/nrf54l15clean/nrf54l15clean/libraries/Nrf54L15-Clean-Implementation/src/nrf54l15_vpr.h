#pragma once

#include <Arduino.h>

#include "nrf54l15_hal.h"
#include "nrf54l15_vpr_transport_shared.h"

namespace xiao_nrf54l15 {

class CtrlApMailbox {
 public:
  static void clearEvents();
  static bool pollRxReady(bool clearEvent = true);
  static bool pollTxDone(bool clearEvent = true);
  static bool rxPending();
  static bool txPending();
  static bool read(uint32_t* value);
  static bool write(uint32_t value);
  static void enableInterrupts(bool rxReady, bool txDone);

 private:
  static NRF_CTRLAPPERI_Type* regs();
};

class VprControl {
 public:
  static bool setInitPc(uint32_t address);
  static uint32_t initPc();
  static void prepareForLaunch();
  static bool start(uint32_t address);
  static void stop();
  static bool isRunning();
  static bool secureAccessEnabled();
  static uint32_t spuPerm();
  static void enableRtPeripherals();
  static void configureDataAccessNonCacheable();
  static void clearCache();
  static void clearBufferedSignals();
  static void clearDebugHaltState();
  static uint32_t debugStatus();
  static uint32_t haltSummary0();
  static uint32_t haltSummary1();
  static uint32_t rawNordicAxCache();
  static uint32_t rawNordicTasks();
  static uint32_t rawNordicEvents();
  static uint32_t rawNordicEventStatus();
  static uint32_t rawNordicCacheCtrl();
  static uint32_t rawMpcMemAccErrEvent();
  static uint32_t rawMpcMemAccErrAddress();
  static uint32_t rawMpcMemAccErrInfo();
  static void clearMpcMemAccErr();

  static bool triggerTask(uint8_t index);
  static bool pollEvent(uint8_t index, bool clearEvent = true);
  static bool clearEvent(uint8_t index);
  static bool enableEventInterrupt(uint8_t index, bool enable = true);

 private:
  static NRF_VPR_Type* regs();
  static bool validTriggerIndex(uint8_t index);
};

class VprSharedTransportStream : public Stream {
 public:
  VprSharedTransportStream();

  bool resetSharedState(bool clearScripts = true);
  bool clearScripts();
  bool addScriptResponse(uint16_t opcode, const uint8_t* response, size_t len);
  bool loadFirmware(const uint8_t* image, size_t len);
  bool bootLoadedFirmware();
  bool loadFirmwareAndStart(const uint8_t* image, size_t len);
  bool loadDefaultCsTransportStubImage();
  bool loadDefaultCsTransportStub();
  void stop();
  bool waitReady(uint32_t spinLimit = 100000UL);
  bool poll();

  uint32_t heartbeat() const;
  uint16_t lastOpcode() const;
  uint32_t transportStatus() const;
  uint32_t lastError() const;
  uint32_t initPc() const;
  bool isRunning() const;
  bool secureAccessEnabled() const;
  uint32_t spuPerm() const;
  uint32_t debugStatus() const;
  uint32_t haltSummary0() const;
  uint32_t haltSummary1() const;
  uint32_t rawNordicTasks() const;
  uint32_t rawNordicEvents() const;
  uint32_t rawNordicEventStatus() const;
  uint32_t rawNordicCacheCtrl() const;
  uint32_t rawMpcMemAccErrEvent() const;
  uint32_t rawMpcMemAccErrAddress() const;
  uint32_t rawMpcMemAccErrInfo() const;

  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  size_t write(uint8_t byte) override;
  size_t write(const uint8_t* buffer, size_t len) override;
  using Print::write;

 private:
  bool pullResponse();
  volatile Nrf54l15VprTransportHostShared* hostShared() const;
  volatile Nrf54l15VprTransportVprShared* vprShared() const;

  uint8_t rxBuffer_[NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA];
  size_t rxLen_;
  size_t rxIndex_;
};

}  // namespace xiao_nrf54l15
