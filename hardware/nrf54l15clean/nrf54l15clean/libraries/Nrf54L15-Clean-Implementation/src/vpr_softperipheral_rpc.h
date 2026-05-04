// VPR Softperipheral RPC Protocol
// =================================
// Shared definitions for CPUAPP ↔ VPR communication.
//
// Protocol layers:
//   1. Transport  — shared memory ring buffers (existing VprSharedTransportStream)
//   2. Service    — service discovery and registration
//   3. RPC        — request/response calls with opcodes
//
// Memory layout:
//   VPR Shared RAM:     0x20018000 - 0x20020800 (32KB)
//   Host→VPR ring:      first 128 bytes
//   VPR→Host ring:      next 256 bytes
//   RPC scratch area:   remaining space

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── RPC opcodes (16-bit) ──────────────────────────────────

// System opcodes (0x0000 - 0x00FF)
#define VPR_RPC_PING              0x0001  // heartbeart check, no payload
#define VPR_RPC_GET_SERVICES      0x0002  // list registered services
#define VPR_RPC_GET_SERVICE_INFO  0x0003  // get info about a specific service
#define VPR_RPC_SHUTDOWN          0x00FF  // graceful shutdown

// sQSPI service (0x0100 - 0x01FF)
#define VPR_RPC_SQSPI_INIT        0x0100
#define VPR_RPC_SQSPI_READ        0x0101
#define VPR_RPC_SQSPI_WRITE       0x0102
#define VPR_RPC_SQSPI_ERASE       0x0103
#define VPR_RPC_SQSPI_STATUS      0x0104
#define VPR_RPC_SQSPI_DEINIT      0x0105

// CRC offload (0x0200 - 0x02FF)
#define VPR_RPC_CRC32_COMPUTE     0x0200
#define VPR_RPC_CRC32C_COMPUTE    0x0201
#define VPR_RPC_FNV1A_COMPUTE     0x0202

// Ticker offload (0x0300 - 0x03FF)
#define VPR_RPC_TICKER_START      0x0300
#define VPR_RPC_TICKER_STOP       0x0301
#define VPR_RPC_TICKER_PERIOD     0x0302

// BLE CS offload (0x0400 - 0x04FF) — existing CS transport stubs
#define VPR_RPC_CS_INIT           0x0400
#define VPR_RPC_CS_PROCEDURE      0x0401
#define VPR_RPC_CS_RESULT         0x0402

// User-defined services (0x1000 - 0xFFFF)
#define VPR_RPC_USER_BASE         0x1000

// ─── RPC status codes ──────────────────────────────────────

#define VPR_RPC_OK                0x0000
#define VPR_RPC_ERR_UNKNOWN_OP    0x0001
#define VPR_RPC_ERR_INVALID_PARAM 0x0002
#define VPR_RPC_ERR_BUSY          0x0003
#define VPR_RPC_ERR_TIMEOUT       0x0004
#define VPR_RPC_ERR_NOT_READY     0x0005
#define VPR_RPC_ERR_MEMORY        0x0006

// ─── Service descriptor ────────────────────────────────────

#define VPR_SERVICE_MAX_NAME      16
#define VPR_MAX_SERVICES          8

typedef struct {
  uint16_t serviceId;                          // unique service ID (VPR_RPC_* base)
  uint16_t version;                            // service version
  char     name[VPR_SERVICE_MAX_NAME];         // human-readable name
} VprServiceDescriptor;

// ─── RPC message format ────────────────────────────────────

#define VPR_RPC_MAX_PAYLOAD       240          // max payload bytes per message

typedef struct {
  uint16_t opcode;                             // VPR_RPC_*
  uint16_t status;                             // 0 = request/ok, error code otherwise
  uint16_t payloadLen;                         // bytes in payload
  uint8_t  reserved[2];
  uint8_t  payload[VPR_RPC_MAX_PAYLOAD];       // variable-length payload
} VprRpcMessage;

// ─── Ring buffer (host→vpr and vpr→host) ───────────────────

#define VPR_RING_BUFFER_SIZE      256

typedef struct {
  volatile uint32_t writeIdx;                  // producer index
  volatile uint32_t readIdx;                   // consumer index
  volatile uint32_t overflow;                  // set if buffer overflows
  uint8_t  data[VPR_RING_BUFFER_SIZE];
} VprRingBuffer;

// ─── VPR service function pointer type ─────────────────────

typedef uint16_t (*VprServiceHandler)(const uint8_t* request, uint16_t requestLen,
                                       uint8_t* response, uint16_t responseMax,
                                       uint16_t* responseLenOut);

#ifdef __cplusplus
}
#endif
