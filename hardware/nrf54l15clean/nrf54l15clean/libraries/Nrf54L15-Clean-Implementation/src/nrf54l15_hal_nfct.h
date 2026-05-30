#pragma once

#include <stdint.h>

#include "nrf54l15_regs.h"

namespace xiao_nrf54l15 {

enum class NfctNfcId1Size : uint8_t {
  kSingle4Bytes = 0,
  kDouble7Bytes = 1,
  kTriple10Bytes = 2,
};

enum class NfctFrameDelayMode : uint8_t {
  kFreeRun = 0,
  kWindow = 1,
  kExact = 2,
  kWindowGrid = 3,
};

// NFC-A Target (NFCT)
// NFC-A listening device per ISO/IEC 14443-A.
// Datasheet chapter 4.2.9 (NFCT — NFC-A target).
class Nfct {
 public:
  // ---- NFCT control ----

  // Enable NFC sense field mode and wait for a reader field.
  inline static void sense() {
    writeReg(NFCT_TASKS_SENSE, 1);
  }

  // Activate NFCT after a field is detected.
  inline static void activate() {
    writeReg(NFCT_TASKS_ACTIVATE, 1);
  }

  // Compatibility alias: start sensing for NFC field.
  inline static void start() {
    sense();
  }

  // Disable NFCT.
  inline static void disable() {
    writeReg(NFCT_TASKS_DISABLE, 1);
  }

  // Compatibility alias.
  inline static void stop() {
    disable();
  }

  inline static void startTx() {
    writeReg(NFCT_TASKS_STARTTX, 1);
  }

  inline static void stopTx() {
    writeReg(NFCT_TASKS_STOPTX, 1);
  }

  inline static void enableRxData() {
    writeReg(NFCT_TASKS_ENABLERXDATA, 1);
  }

  inline static void goIdle() {
    writeReg(NFCT_TASKS_GOIDLE, 1);
  }

  inline static void goSleep() {
    writeReg(NFCT_TASKS_GOSLEEP, 1);
  }

  // Check if the peripheral is ready to exchange frames.
  inline static bool ready(bool clear = true) {
    return event(NFCT_EVENTS_READY, clear);
  }

  // Check if started event fired.
  inline static bool started(bool clear = true) {
    return event(NFCT_EVENTS_STARTED, clear);
  }

  // Compatibility helper: true when the tag state is disabled/sense.
  inline static bool stopped(bool clear = true) {
    (void)clear;
    return tagState() == 0U;
  }

  // ---- NFCID1 / automatic collision resolution ----

  // Set a 4, 7, or 10 byte NFCID1. Bytes are in normal on-air order.
  inline static bool setNfcId1(const uint8_t* id, uint8_t len) {
    if (id == nullptr || (len != 4U && len != 7U && len != 10U)) {
      return false;
    }

    if (len == 4U) {
      writeReg(NFCT_NFCID1_LAST, pack4(id[0], id[1], id[2], id[3]));
      setNfcId1Size(NfctNfcId1Size::kSingle4Bytes);
      return true;
    }

    if (len == 7U) {
      writeReg(NFCT_NFCID1_SECONDLAST, pack3(id[0], id[1], id[2]));
      writeReg(NFCT_NFCID1_LAST, pack4(id[3], id[4], id[5], id[6]));
      setNfcId1Size(NfctNfcId1Size::kDouble7Bytes);
      return true;
    }

    writeReg(NFCT_NFCID1_THIRDLAST, pack3(id[0], id[1], id[2]));
    writeReg(NFCT_NFCID1_SECONDLAST, pack3(id[3], id[4], id[5]));
    writeReg(NFCT_NFCID1_LAST, pack4(id[6], id[7], id[8], id[9]));
    setNfcId1Size(NfctNfcId1Size::kTriple10Bytes);
    return true;
  }

  inline static uint8_t nfcId1(uint8_t* out, uint8_t maxLen) {
    if (out == nullptr || maxLen == 0U) {
      return 0U;
    }

    const NfctNfcId1Size size = nfcId1Size();
    const uint8_t len = (size == NfctNfcId1Size::kSingle4Bytes)
                            ? 4U
                            : ((size == NfctNfcId1Size::kDouble7Bytes) ? 7U : 10U);
    if (maxLen < len) {
      return 0U;
    }

    uint8_t index = 0U;
    if (len == 10U) {
      unpack3(readReg(NFCT_NFCID1_THIRDLAST), &out[index]);
      index += 3U;
    }
    if (len >= 7U) {
      unpack3(readReg(NFCT_NFCID1_SECONDLAST), &out[index]);
      index += 3U;
    }
    unpack4(readReg(NFCT_NFCID1_LAST), &out[index]);
    return len;
  }

  inline static void setNfcId1Size(NfctNfcId1Size size) {
    uint32_t sens = readReg(NFCT_SENSRES) & ~(3UL << 6);
    sens |= (static_cast<uint32_t>(size) & 3UL) << 6;
    writeReg(NFCT_SENSRES, sens);
  }

  inline static NfctNfcId1Size nfcId1Size() {
    return static_cast<NfctNfcId1Size>((readReg(NFCT_SENSRES) >> 6) & 3UL);
  }

  inline static void setAutoCollisionResolution(bool enable = true) {
    writeReg(NFCT_AUTOCOLRESCONFIG, enable ? 0U : 1U);
  }

  // Legacy helpers retained for sketches that used the original wrapper names.
  inline static void setNfcId3rdLast(uint32_t value) {
    writeReg(NFCT_NFCID1_THIRDLAST, value & 0x00FFFFFFUL);
  }

  inline static void setNfcId2ndLast(uint32_t value) {
    writeReg(NFCT_NFCID1_SECONDLAST, value & 0x00FFFFFFUL);
  }

  inline static void setNfcIdLast(uint8_t byte0, uint8_t byte1, uint8_t byte2,
                                  uint8_t byte3 = 0U) {
    writeReg(NFCT_NFCID1_LAST, pack4(byte0, byte1, byte2, byte3));
  }

  inline static uint32_t nfcIdLast() { return readReg(NFCT_NFCID1_LAST); }
  inline static uint32_t nfcIdSecondLast() { return readReg(NFCT_NFCID1_SECONDLAST); }
  inline static uint32_t nfcIdThirdLast() { return readReg(NFCT_NFCID1_THIRDLAST); }

  // Compatibility aliases. NFCT has NFCID1 registers, not TAGHEADER registers.
  inline static uint32_t tagHeader0() { return nfcIdLast(); }
  inline static uint32_t tagHeader1() { return nfcIdSecondLast(); }
  inline static uint32_t tagHeader2() { return nfcIdThirdLast(); }
  inline static uint32_t tagHeader3() { return 0U; }

  // ---- I/O control ----

  // Compatibility helper retained for older sketches. The nRF54L15 NFCT
  // register block does not expose the legacy IOCONFIG polarity register.
  inline static void setIoPolarity(bool activeHigh) {
    (void)activeHigh;
  }

  // Enable auto-response mode.
  inline static void enableAutoResponse(bool enable = true) {
    setAutoCollisionResolution(enable);
  }

  // ---- Frame handling ----

  // Check if frame received event fired.
  inline static bool frameReceived(bool clear = true) {
    return event(NFCT_EVENTS_RXFRAMEEND, clear);
  }

  // Check if frame transmitted event fired.
  inline static bool frameTransmitted(bool clear = true) {
    return event(NFCT_EVENTS_TXFRAMEEND, clear);
  }

  inline static bool txFrameStarted(bool clear = true) {
    return event(NFCT_EVENTS_TXFRAMESTART, clear);
  }

  inline static bool rxFrameStarted(bool clear = true) {
    return event(NFCT_EVENTS_RXFRAMESTART, clear);
  }

  inline static bool endRx(bool clear = true) {
    return event(NFCT_EVENTS_ENDRX, clear);
  }

  inline static bool endTx(bool clear = true) {
    return event(NFCT_EVENTS_ENDTX, clear);
  }

  // Error events.
  inline static bool errorDetected(bool clear = true) {
    return event(NFCT_EVENTS_ERROR, clear);
  }

  inline static bool rxErrorDetected(bool clear = true) {
    return event(NFCT_EVENTS_RXERROR, clear);
  }

  inline static bool selected(bool clear = true) {
    return event(NFCT_EVENTS_SELECTED, clear);
  }

  inline static bool collision(bool clear = true) {
    return event(NFCT_EVENTS_COLLISION, clear);
  }

  // Check for field presence (wake-on-field).
  inline static bool fieldDetected(bool clear = true) {
    return event(NFCT_EVENTS_FIELDDETECTED, clear);
  }

  inline static bool fieldLost(bool clear = true) {
    return event(NFCT_EVENTS_FIELDLOST, clear);
  }

  inline static bool fieldPresent() {
    return (readReg(NFCT_FIELDPRESENT) & 1UL) != 0U;
  }

  inline static bool fieldLocked() {
    return (readReg(NFCT_FIELDPRESENT) & (1UL << 1)) != 0U;
  }

  inline static uint32_t errorStatus(bool clear = false) {
    uint32_t value = readReg(NFCT_ERRORSTATUS);
    if (clear) {
      writeReg(NFCT_ERRORSTATUS, value);
    }
    return value;
  }

  inline static uint32_t rxFrameStatus() {
    return readReg(NFCT_FRAMESTATUS_RX);
  }

  inline static uint32_t tagState() {
    return readReg(NFCT_NFCTAGSTATE);
  }

  inline static bool sleepState() {
    return (readReg(NFCT_SLEEPSTATE) & 1UL) != 0U;
  }

  // ---- SENS_RES response ----

  // Set SENS_RES response value.
  inline static void setSensRes(uint8_t value) {
    writeReg(NFCT_SENSRES, value);
  }

  // Set SEL_RES response value.
  inline static void setSelRes(uint8_t value) {
    writeReg(NFCT_SELRES, value);
  }

  inline static uint32_t sensRes() {
    return readReg(NFCT_SENSRES);
  }

  inline static uint32_t selRes() {
    return readReg(NFCT_SELRES);
  }

  inline static void configureFrameDelay(uint16_t minCycles, uint32_t maxCycles,
                                         NfctFrameDelayMode mode) {
    writeReg(NFCT_FRAMEDELAYMIN, minCycles);
    writeReg(NFCT_FRAMEDELAYMAX, maxCycles & 0x000FFFFFUL);
    writeReg(NFCT_FRAMEDELAYMODE, static_cast<uint32_t>(mode) & 3UL);
  }

  // ---- Modulation and data ----

  // NFCT uses one shared EasyDMA packet buffer for RX and TX storage.
  inline static bool setPacketBuffer(uint8_t* ptr, uint16_t maxLen) {
    if (ptr == nullptr || maxLen == 0U || maxLen > 0x101U) {
      return false;
    }
    writeReg(NFCT_PACKETPTR, reinterpret_cast<uint32_t>(ptr));
    writeReg(NFCT_MAXLEN, maxLen);
    return true;
  }

  // Compatibility alias. TX/RX share PACKETPTR and MAXLEN on nRF54L15 NFCT.
  inline static void setTxBuffer(const uint8_t* ptr, uint16_t len) {
    (void)setPacketBuffer(const_cast<uint8_t*>(ptr), len);
  }

  inline static void setRxBuffer(uint8_t* ptr, uint16_t maxLen) {
    (void)setPacketBuffer(ptr, maxLen);
  }

  inline static void configureTxFrame(bool parity = true, bool sof = true,
                                      bool crc16 = true, bool discardStart = false) {
    uint32_t cfg = 0U;
    if (parity) cfg |= 1UL << 0;
    if (discardStart) cfg |= 1UL << 1;
    if (sof) cfg |= 1UL << 2;
    if (crc16) cfg |= 1UL << 4;
    writeReg(NFCT_TXD_FRAMECONFIG, cfg);
  }

  inline static void configureRxFrame(bool parity = true, bool sof = true,
                                      bool crc16 = true) {
    uint32_t cfg = 0U;
    if (parity) cfg |= 1UL << 0;
    if (sof) cfg |= 1UL << 2;
    if (crc16) cfg |= 1UL << 4;
    writeReg(NFCT_RXD_FRAMECONFIG, cfg);
  }

  inline static void setTxAmount(uint16_t bytes, uint8_t bits = 0U) {
    writeReg(NFCT_TXD_AMOUNT,
             ((static_cast<uint32_t>(bytes) & 0x1FFUL) << 3) |
                 (static_cast<uint32_t>(bits) & 0x7UL));
  }

  // Get number of bytes actually received.
  inline static uint16_t rxByteCount() {
    return static_cast<uint16_t>((readReg(NFCT_RXD_AMOUNT) >> 3) & 0x1FFUL);
  }

  // Get number of bytes actually transmitted.
  inline static uint16_t txByteCount() {
    return static_cast<uint16_t>((readReg(NFCT_TXD_AMOUNT) >> 3) & 0x1FFUL);
  }

  inline static uint8_t rxBitCount() {
    return static_cast<uint8_t>(readReg(NFCT_RXD_AMOUNT) & 0x7UL);
  }

  inline static uint8_t txBitCount() {
    return static_cast<uint8_t>(readReg(NFCT_TXD_AMOUNT) & 0x7UL);
  }

  inline static void setLowPowerMode(bool fullLowPower = true) {
    writeReg(NFCT_MODE, fullLowPower ? 3U : 1U);
  }

  inline static void setLowLatencyMode() {
    writeReg(NFCT_MODE, 0U);
  }

  inline static void setNfcPadsEnabled(bool enable = true) {
    writeReg(NFCT_PADCONFIG, enable ? 1U : 0U);
  }

  inline static void setModulationOutputDisabled() {
    writeReg(NFCT_MODULATIONPSEL, 0xFFFFFFFFUL);
    writeReg(NFCT_MODULATIONCTRL, 1U);
  }

  inline static void setModulationOutputPin(uint8_t port, uint8_t pin,
                                            bool keepInternal = true) {
    writeReg(NFCT_MODULATIONPSEL,
             (static_cast<uint32_t>(pin) & 0x1FUL) |
                 ((static_cast<uint32_t>(port) & 0x3UL) << 5));
    writeReg(NFCT_MODULATIONCTRL, keepInternal ? 3U : 2U);
  }

  // ---- Enable/disable ----

  // Compatibility helper: the nRF54L15 NFCT block is task-driven. Calling
  // enable(false) disables it; enable(true) enters sense mode.
  inline static void enable(bool en = true) {
    if (en) {
      sense();
    } else {
      disable();
    }
  }

  // ---- Interrupts ----

  enum Interrupt : uint32_t {
    kInterruptReady = 1UL << 0,
    kInterruptFieldDetected = 1UL << 1,
    kInterruptFieldLost = 1UL << 2,
    kInterruptTxFrameStart = 1UL << 3,
    kInterruptTxFrameEnd = 1UL << 4,
    kInterruptRxFrameStart = 1UL << 5,
    kInterruptRxFrameEnd = 1UL << 6,
    kInterruptError = 1UL << 7,
    kInterruptRxError = 1UL << 10,
    kInterruptEndRx = 1UL << 11,
    kInterruptEndTx = 1UL << 12,
    kInterruptAutoColResStarted = 1UL << 14,
    kInterruptCollision = 1UL << 18,
    kInterruptSelected = 1UL << 19,
    kInterruptStarted = 1UL << 20,
  };

  inline static void enableInterrupts(uint32_t mask) {
    writeReg(NFCT_INTENSET, mask);
  }

  inline static void disableInterrupts(uint32_t mask) {
    writeReg(NFCT_INTENCLR, mask);
  }

  inline static uint32_t interruptsEnabled() {
    return readReg(NFCT_INTEN);
  }

  inline static void enableErrorInterrupt(bool en = true) {
    if (en) {
      enableInterrupts(kInterruptError | kInterruptRxError);
    } else {
      disableInterrupts(kInterruptError | kInterruptRxError);
    }
  }

  inline static void clearEvents() {
    writeReg(NFCT_EVENTS_READY, 0);
    writeReg(NFCT_EVENTS_FIELDDETECTED, 0);
    writeReg(NFCT_EVENTS_FIELDLOST, 0);
    writeReg(NFCT_EVENTS_TXFRAMESTART, 0);
    writeReg(NFCT_EVENTS_TXFRAMEEND, 0);
    writeReg(NFCT_EVENTS_RXFRAMESTART, 0);
    writeReg(NFCT_EVENTS_RXFRAMEEND, 0);
    writeReg(NFCT_EVENTS_ERROR, 0);
    writeReg(NFCT_EVENTS_RXERROR, 0);
    writeReg(NFCT_EVENTS_ENDRX, 0);
    writeReg(NFCT_EVENTS_ENDTX, 0);
    writeReg(NFCT_EVENTS_AUTOCOLRESSTARTED, 0);
    writeReg(NFCT_EVENTS_COLLISION, 0);
    writeReg(NFCT_EVENTS_SELECTED, 0);
    writeReg(NFCT_EVENTS_STARTED, 0);
  }

  // ---- Low-level ----

  inline static uint32_t readReg(uint32_t offset) {
    return *reinterpret_cast<const volatile uint32_t*>(BASE + offset);
  }

  inline static void writeReg(uint32_t offset, uint32_t value) {
    *reinterpret_cast<volatile uint32_t*>(BASE + offset) = value;
  }

 private:
  static inline bool event(uint32_t offset, bool clear) {
    uint32_t ev = readReg(offset);
    if (clear) writeReg(offset, 0);
    return ev != 0U;
  }

  static constexpr uint32_t pack3(uint8_t a, uint8_t b, uint8_t c) {
    return static_cast<uint32_t>(a) |
           (static_cast<uint32_t>(b) << 8) |
           (static_cast<uint32_t>(c) << 16);
  }

  static constexpr uint32_t pack4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return pack3(a, b, c) | (static_cast<uint32_t>(d) << 24);
  }

  static inline void unpack3(uint32_t value, uint8_t* out) {
    out[0] = static_cast<uint8_t>(value & 0xFFU);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
  }

  static inline void unpack4(uint32_t value, uint8_t* out) {
    unpack3(value, out);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
  }

  static constexpr uint32_t BASE = nrf54l15::NFCT_BASE;

  // Tasks.
  static constexpr uint32_t NFCT_TASKS_ACTIVATE = 0x000;
  static constexpr uint32_t NFCT_TASKS_DISABLE = 0x004;
  static constexpr uint32_t NFCT_TASKS_SENSE = 0x008;
  static constexpr uint32_t NFCT_TASKS_STARTTX = 0x00C;
  static constexpr uint32_t NFCT_TASKS_STOPTX = 0x010;
  static constexpr uint32_t NFCT_TASKS_ENABLERXDATA = 0x01C;
  static constexpr uint32_t NFCT_TASKS_GOIDLE = 0x024;
  static constexpr uint32_t NFCT_TASKS_GOSLEEP = 0x028;

  // Events.
  static constexpr uint32_t NFCT_EVENTS_READY = 0x100;
  static constexpr uint32_t NFCT_EVENTS_FIELDDETECTED = 0x104;
  static constexpr uint32_t NFCT_EVENTS_FIELDLOST = 0x108;
  static constexpr uint32_t NFCT_EVENTS_TXFRAMESTART = 0x10C;
  static constexpr uint32_t NFCT_EVENTS_TXFRAMEEND = 0x110;
  static constexpr uint32_t NFCT_EVENTS_RXFRAMESTART = 0x114;
  static constexpr uint32_t NFCT_EVENTS_RXFRAMEEND = 0x118;
  static constexpr uint32_t NFCT_EVENTS_ERROR = 0x11C;
  static constexpr uint32_t NFCT_EVENTS_RXERROR = 0x128;
  static constexpr uint32_t NFCT_EVENTS_ENDRX = 0x12C;
  static constexpr uint32_t NFCT_EVENTS_ENDTX = 0x130;
  static constexpr uint32_t NFCT_EVENTS_AUTOCOLRESSTARTED = 0x138;
  static constexpr uint32_t NFCT_EVENTS_COLLISION = 0x148;
  static constexpr uint32_t NFCT_EVENTS_SELECTED = 0x14C;
  static constexpr uint32_t NFCT_EVENTS_STARTED = 0x150;

  // Config.
  static constexpr uint32_t NFCT_ERRORSTATUS = 0x404;
  static constexpr uint32_t NFCT_FRAMESTATUS_RX = 0x40C;
  static constexpr uint32_t NFCT_NFCTAGSTATE = 0x410;
  static constexpr uint32_t NFCT_SLEEPSTATE = 0x420;
  static constexpr uint32_t NFCT_FIELDPRESENT = 0x43C;
  static constexpr uint32_t NFCT_FRAMEDELAYMIN = 0x504;
  static constexpr uint32_t NFCT_FRAMEDELAYMAX = 0x508;
  static constexpr uint32_t NFCT_FRAMEDELAYMODE = 0x50C;
  static constexpr uint32_t NFCT_PACKETPTR = 0x510;
  static constexpr uint32_t NFCT_MAXLEN = 0x514;
  static constexpr uint32_t NFCT_TXD_FRAMECONFIG = 0x518;
  static constexpr uint32_t NFCT_TXD_AMOUNT = 0x51C;
  static constexpr uint32_t NFCT_RXD_FRAMECONFIG = 0x520;
  static constexpr uint32_t NFCT_RXD_AMOUNT = 0x524;
  static constexpr uint32_t NFCT_MODULATIONCTRL = 0x52C;
  static constexpr uint32_t NFCT_MODULATIONPSEL = 0x538;
  static constexpr uint32_t NFCT_MODE = 0x550;
  static constexpr uint32_t NFCT_NFCID1_LAST = 0x590;
  static constexpr uint32_t NFCT_NFCID1_SECONDLAST = 0x594;
  static constexpr uint32_t NFCT_NFCID1_THIRDLAST = 0x598;
  static constexpr uint32_t NFCT_AUTOCOLRESCONFIG = 0x59C;
  static constexpr uint32_t NFCT_SENSRES = 0x5A0;
  static constexpr uint32_t NFCT_SELRES = 0x5A4;
  static constexpr uint32_t NFCT_PADCONFIG = 0x6D4;

  // Interrupt.
  static constexpr uint32_t NFCT_INTEN = 0x300;
  static constexpr uint32_t NFCT_INTENSET = 0x304;
  static constexpr uint32_t NFCT_INTENCLR = 0x308;
};

}  // namespace xiao_nrf54l15
