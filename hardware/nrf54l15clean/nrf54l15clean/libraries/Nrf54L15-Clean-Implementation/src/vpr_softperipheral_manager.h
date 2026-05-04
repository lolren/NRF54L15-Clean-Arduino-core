// VPR Softperipheral Manager — CPUAPP (Cortex-M33) Side
// ======================================================
// Manages VPR services: discovery, RPC calls, lifecycle.
// Uses the existing VprSharedTransportStream for transport.

#pragma once

#include <Arduino.h>
#include "nrf54l15_vpr.h"
#include "nrf54l15_vpr_transport_shared.h"
#include "vpr_softperipheral_rpc.h"

namespace xiao_nrf54l15 {

class VprSoftperipheralManager {
 public:
  VprSoftperipheralManager();

  // ─── Lifecycle ──────────────────────────────────────────

  bool begin();
  void end();
  bool isReady() const { return ready_; }

  // ─── Service Discovery ──────────────────────────────────

  // Discover available services on the VPR. Returns count.
  uint8_t discoverServices();

  // Get descriptor for a discovered service
  const VprServiceDescriptor* getService(uint8_t index) const;
  const VprServiceDescriptor* findService(uint16_t serviceId) const;

  // ─── RPC Calls ──────────────────────────────────────────

  // Synchronous RPC call. Blocks until response or timeout.
  // Returns true if call succeeded (status == VPR_RPC_OK).
  bool rpcCall(uint16_t opcode,
               const uint8_t* requestPayload, uint16_t requestLen,
               uint8_t* responsePayload, uint16_t responseMax,
               uint16_t* responseLenOut,
               uint32_t timeoutMs = 5000);

  // Fire-and-forget RPC (no response expected)
  bool rpcNotify(uint16_t opcode,
                 const uint8_t* payload, uint16_t payloadLen);

  // ─── Convenience: Pre-built services ────────────────────

  // CRC32 computation offload (VPR_RPC_CRC32_COMPUTE)
  bool crc32Compute(const uint8_t* data, size_t len, uint32_t* crcOut);

  // FNV1a hash offload (VPR_RPC_FNV1A_COMPUTE)
  bool fnv1aCompute(const uint8_t* data, size_t len, uint32_t* hashOut);

  // Ticker offload
  bool tickerStart(uint32_t periodUs);
  bool tickerStop();

  // ─── Direct Transport Access ────────────────────────────

  VprSharedTransportStream& transport() { return transport_; }
  bool poll();

  // ─── Statistics ─────────────────────────────────────────

  uint32_t rpcCallsSent() const { return stats_.rpcCallsSent; }
  uint32_t rpcCallsOk() const { return stats_.rpcCallsOk; }
  uint32_t rpcCallsFailed() const { return stats_.rpcCallsFailed; }

 private:
  bool ensureReady();

  VprSharedTransportStream transport_;
  bool ready_;

  VprServiceDescriptor services_[VPR_MAX_SERVICES];
  uint8_t serviceCount_;

  struct {
    uint32_t rpcCallsSent;
    uint32_t rpcCallsOk;
    uint32_t rpcCallsFailed;
  } stats_;
};

}  // namespace xiao_nrf54l15
