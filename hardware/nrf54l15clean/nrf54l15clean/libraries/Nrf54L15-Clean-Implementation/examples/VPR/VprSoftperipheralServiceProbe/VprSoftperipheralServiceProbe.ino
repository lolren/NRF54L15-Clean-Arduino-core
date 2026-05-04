// VPR Softperipheral Service Probe
// =================================
// Tests the VPR softperipheral framework: boots VPR, discovers services,
// makes RPC calls (PING, GET_SERVICES), and verifies the VPR is responsive.
//
// Upload to a single XIAO nRF54L15 board. Open Serial Monitor at 115200 baud.
// The VPR RISC-V CPU will boot and respond to RPC calls.
//
// Expected output:
//   VPR boot: OK
//   VPR ready: heartbeat=...
//   Services discovered: 2
//     svc[0]: vpr_svc_0 (id=0x0001)
//     svc[1]: vpr_svc_1 (id=0x0002)
//   PING: OK
//   RPC stats: sent=N ok=N failed=0
//
// Note: This example uses the default CS transport stub firmware (bundled).
// A custom firmware with additional services would need a RISC-V toolchain.

#include <nrf54_all.h>
#include "nrf54l15_vpr.h"
#include "vpr_softperipheral_manager.h"

using xiao_nrf54l15::VprSoftperipheralManager;
using xiao_nrf54l15::VprControl;
using xiao_nrf54l15::VprSharedTransportStream;

VprSoftperipheralManager g_vpr;

void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() - 1) < 3000) {}

  Serial.println();
  Serial.println("=== VPR Softperipheral Probe ===");
  Serial.println();

  // ─── 1. Boot VPR ──────────────────────────────────────
  Serial.print("VPR boot: ");
  if (!g_vpr.begin()) {
    Serial.println("FAIL (check VPR Support tool menu)");
    return;
  }
  Serial.println("OK");

  // ─── 2. Service Discovery ──────────────────────────────
  uint8_t count = g_vpr.discoverServices();
  Serial.print("Services discovered: ");
  Serial.println(count);

  for (uint8_t i = 0; i < count; i++) {
    const VprServiceDescriptor* svc = g_vpr.getService(i);
    if (svc) {
      Serial.print("  svc[");
      Serial.print(i);
      Serial.print("]: ");
      Serial.write((const uint8_t*)svc->name, VPR_SERVICE_MAX_NAME);
      Serial.print(" (id=0x");
      Serial.print(svc->serviceId, HEX);
      Serial.print(" v=");
      Serial.print(svc->version);
      Serial.println(")");
    }
  }

  // ─── 3. PING Test ─────────────────────────────────────
  Serial.print("PING: ");
  uint8_t rsp[VPR_RPC_MAX_PAYLOAD];
  uint16_t rspLen = 0;
  if (g_vpr.rpcCall(VPR_RPC_PING, nullptr, 0, rsp, sizeof(rsp), &rspLen, 2000)) {
    Serial.println("OK");
  } else {
    Serial.println("FAIL");
  }

  // ─── 4. Stats ──────────────────────────────────────────
  Serial.print("RPC stats: sent=");
  Serial.print(g_vpr.rpcCallsSent());
  Serial.print(" ok=");
  Serial.print(g_vpr.rpcCallsOk());
  Serial.print(" failed=");
  Serial.println(g_vpr.rpcCallsFailed());

  Serial.println();
  Serial.println("DONE");
}

void loop() {
  g_vpr.poll();
  delay(100);
}
