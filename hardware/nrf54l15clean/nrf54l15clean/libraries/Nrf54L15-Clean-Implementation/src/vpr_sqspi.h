// VPR sQSPI SoftPeripheral — CPUAPP (Cortex-M33) Side
// ======================================================
// Provides sQSPI (serial Quad SPI) flash access via the VPR RISC-V CPU.
//
// sQSPI is only accessible from VPR on nRF54L15 — not from CPUAPP directly.
// This library wraps the VPR RPC protocol to provide a clean Arduino API.
//
// Usage:
//   VprSQspi qspi;
//   qspi.begin();                           // boot VPR + init sQSPI
//   qspi.read(0x000000, buffer, 256);       // read 256 bytes from flash
//   qspi.write(0x000000, data, 256);        // write 256 bytes (must erase first)
//   qspi.eraseSector(0x000000);             // erase 4KB sector
//   qspi.getJedecId(id);                    // read JEDEC manufacturer ID
//
// RPC opcodes (defined in vpr_softperipheral_rpc.h):
//   VPR_RPC_SQSPI_INIT   — initialize sQSPI peripheral
//   VPR_RPC_SQSPI_READ   — read from flash
//   VPR_RPC_SQSPI_WRITE  — write to flash
//   VPR_RPC_SQSPI_ERASE  — erase sector/block
//   VPR_RPC_SQSPI_STATUS — get flash status
//   VPR_RPC_SQSPI_DEINIT — shutdown

#pragma once

#include <Arduino.h>
#include "vpr_softperipheral_manager.h"
#include "vpr_softperipheral_rpc.h"

namespace xiao_nrf54l15 {

class VprSQspi {
 public:
  VprSQspi();

  // ─── Lifecycle ──────────────────────────────────────────

  // Initialize the VPR and sQSPI peripheral.
  // Returns true if VPR booted and sQSPI initialized.
  bool begin();

  // Shutdown sQSPI and optionally stop VPR
  void end(bool stopVpr = false);

  bool isReady() const { return ready_; }

  // ─── Flash Operations ───────────────────────────────────

  // Read bytes from flash at the given address.
  // addr: 24-bit flash address
  // buffer: destination buffer
  // len: number of bytes to read
  bool read(uint32_t addr, uint8_t* buffer, size_t len);

  // Write bytes to flash (must erase sector first).
  // addr: 24-bit flash address
  // data: source buffer
  // len: number of bytes to write (must be within one page, typically 256)
  bool write(uint32_t addr, const uint8_t* data, size_t len);

  // Erase a 4KB sector at the given address.
  bool eraseSector(uint32_t addr);

  // Erase a 64KB block at the given address.
  bool eraseBlock(uint32_t addr);

  // Erase the entire flash chip.
  bool eraseChip();

  // ─── Identification ─────────────────────────────────────

  // Read JEDEC manufacturer ID (3 bytes).
  bool getJedecId(uint8_t idOut[3]);

  // Read unique ID (8 bytes, if supported by flash).
  bool getUniqueId(uint8_t idOut[8]);

  // ─── Status ─────────────────────────────────────────────

  // Read the flash status register.
  bool readStatus(uint8_t* statusOut);

  // Wait until the flash is not busy (write/erase complete).
  bool waitReady(uint32_t timeoutMs = 10000);

  // ─── Direct VPR Access ──────────────────────────────────

  VprSoftperipheralManager& vpr() { return vpr_; }

 private:
  bool rpc(uint16_t opcode,
           const uint8_t* req, uint16_t reqLen,
           uint8_t* rsp, uint16_t rspMax, uint16_t* rspLen);

  VprSoftperipheralManager vpr_;
  bool ready_;
};

}  // namespace xiao_nrf54l15
