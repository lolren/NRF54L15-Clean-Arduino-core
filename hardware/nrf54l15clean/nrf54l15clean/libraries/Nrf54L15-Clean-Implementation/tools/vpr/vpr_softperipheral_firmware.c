// VPR Softperipheral Firmware Framework — RISC-V Side
// ====================================================
// This runs on the nRF54L15's VPR RISC-V CPU.
// Compile with: riscv32-unknown-elf-gcc -march=rv32imc -mabi=ilp32
//
// Memory layout:
//   .text:  0x2003CD00 (VPR_IMAGE_BASE)
//   .data:  follows .text
//   .bss:   follows .data
//   Stack:  grows down from end of VPR RAM
//
// Shared memory for RPC:
//   Host→VPR: 0x20018000 + 0  (ring buffer)
//   VPR→Host: 0x20018000 + 256 (ring buffer)
//   Scratch:  0x20018000 + 512 (RPC message area)

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ─── Memory-mapped hardware addresses (VPR view) ────────

#define VPR_SHARED_BASE     0x20018000UL
#define VPR_HOST_RING_BASE  (VPR_SHARED_BASE)
#define VPR_VPR_RING_BASE   (VPR_SHARED_BASE + 256)
#define VPR_RPC_SCRATCH     (VPR_SHARED_BASE + 512)

// ─── Ring buffer (same layout as CPUAPP side) ───────────

typedef struct {
  volatile uint32_t writeIdx;
  volatile uint32_t readIdx;
  volatile uint32_t overflow;
  uint8_t  data[256];
} VprRingBuffer;

// ─── RPC definitions ────────────────────────────────────

#define VPR_RPC_MAX_PAYLOAD     240
#define VPR_MAX_SERVICES        8
#define VPR_SERVICE_MAX_NAME    16

typedef struct {
  uint16_t opcode;
  uint16_t status;
  uint16_t payloadLen;
  uint8_t  reserved[2];
  uint8_t  payload[VPR_RPC_MAX_PAYLOAD];
} VprRpcMessage;

typedef struct {
  uint16_t serviceId;
  uint16_t version;
  char     name[VPR_SERVICE_MAX_NAME];
} VprServiceDescriptor;

typedef uint16_t (*VprServiceHandler)(const uint8_t* req, uint16_t reqLen,
                                       uint8_t* rsp, uint16_t rspMax,
                                       uint16_t* rspLenOut);

typedef struct {
  uint16_t        serviceId;
  VprServiceHandler handler;
} VprServiceEntry;

// ─── System opcodes ─────────────────────────────────────

#define VPR_RPC_PING            0x0001
#define VPR_RPC_GET_SERVICES    0x0002
#define VPR_RPC_GET_SERVICE_INFO 0x0003

#define VPR_RPC_OK              0x0000
#define VPR_RPC_ERR_UNKNOWN_OP  0x0001

// ─── Ring buffer helpers ─────────────────────────────────

static int ring_empty(volatile VprRingBuffer* rb) {
  return rb->readIdx == rb->writeIdx;
}

static int ring_full(volatile VprRingBuffer* rb) {
  uint32_t next = (rb->writeIdx + 1) % 256;
  return next == rb->readIdx;
}

static uint8_t ring_read(volatile VprRingBuffer* rb) {
  while (ring_empty(rb)) { __asm__ volatile("nop"); }
  uint8_t b = rb->data[rb->readIdx];
  rb->readIdx = (rb->readIdx + 1) % 256;
  return b;
}

static void ring_write(volatile VprRingBuffer* rb, uint8_t b) {
  while (ring_full(rb)) {
    rb->overflow = 1;
    rb->readIdx = (rb->readIdx + 1) % 256;  // drop oldest
  }
  rb->data[rb->writeIdx] = b;
  rb->writeIdx = (rb->writeIdx + 1) % 256;
}

// ─── Service registry ────────────────────────────────────

static VprServiceEntry g_services[VPR_MAX_SERVICES];
static int g_serviceCount = 0;

int vpr_register_service(uint16_t serviceId, VprServiceHandler handler) {
  if (g_serviceCount >= VPR_MAX_SERVICES) return -1;
  g_services[g_serviceCount].serviceId = serviceId;
  g_services[g_serviceCount].handler = handler;
  g_serviceCount++;
  return 0;
}

// ─── System service handlers ─────────────────────────────

static uint16_t sys_ping(const uint8_t* req, uint16_t reqLen,
                          uint8_t* rsp, uint16_t rspMax, uint16_t* rspLen) {
  *rspLen = 0;
  return VPR_RPC_OK;
}

static uint16_t sys_get_services(const uint8_t* req, uint16_t reqLen,
                                  uint8_t* rsp, uint16_t rspMax, uint16_t* rspLen) {
  // Response: count(1) + descriptors
  rsp[0] = (uint8_t)g_serviceCount;
  size_t off = 1;
  for (int i = 0; i < g_serviceCount; i++) {
    VprServiceDescriptor desc;
    desc.serviceId = g_services[i].serviceId;
    desc.version = 0x0001;
    // Copy name from service table (we don't store names — use "vpr_svc_N")
    for (int j = 0; j < VPR_SERVICE_MAX_NAME - 1; j++) desc.name[j] = ' ';
    desc.name[0] = 'v'; desc.name[1] = 'p'; desc.name[2] = 'r';
    desc.name[3] = '_'; desc.name[4] = 's'; desc.name[5] = 'v';
    desc.name[6] = 'c'; desc.name[7] = '_'; desc.name[8] = '0' + i;
    desc.name[VPR_SERVICE_MAX_NAME - 1] = 0;
    if (off + sizeof(desc) <= rspMax) {
      memcpy(rsp + off, &desc, sizeof(desc));
      off += sizeof(desc);
    }
  }
  *rspLen = off;
  return VPR_RPC_OK;
}

// ─── Main RPC dispatch loop ───────────────────────────────

static void process_rpc(VprRpcMessage* req, VprRpcMessage* rsp) {
  memset(rsp, 0, sizeof(VprRpcMessage));
  rsp->opcode = req->opcode;
  
  // Check system opcodes first
  if (req->opcode == VPR_RPC_PING) {
    rsp->status = sys_ping(req->payload, req->payloadLen,
                           rsp->payload, VPR_RPC_MAX_PAYLOAD, &rsp->payloadLen);
    return;
  }
  if (req->opcode == VPR_RPC_GET_SERVICES) {
    rsp->status = sys_get_services(req->payload, req->payloadLen,
                                    rsp->payload, VPR_RPC_MAX_PAYLOAD, &rsp->payloadLen);
    return;
  }
  
  // Check registered service handlers
  for (int i = 0; i < g_serviceCount; i++) {
    if (g_services[i].serviceId == req->opcode) {
      rsp->status = g_services[i].handler(
          req->payload, req->payloadLen,
          rsp->payload, VPR_RPC_MAX_PAYLOAD, &rsp->payloadLen);
      return;
    }
  }
  
  rsp->status = VPR_RPC_ERR_UNKNOWN_OP;
  rsp->payloadLen = 0;
}

// ─── Main entry point ─────────────────────────────────────

// Called by VPR boot code after hardware init
void vpr_main(void) {
  volatile VprRingBuffer* hostRing = (volatile VprRingBuffer*)VPR_HOST_RING_BASE;
  volatile VprRingBuffer* vprRing  = (volatile VprRingBuffer*)VPR_VPR_RING_BASE;
  
  // Register built-in services
  vpr_register_service(VPR_RPC_PING, sys_ping);
  vpr_register_service(VPR_RPC_GET_SERVICES, sys_get_services);
  
  // Main event loop
  while (1) {
    // Check for incoming RPC requests from host ring
    if (!ring_empty(hostRing)) {
      // Read message length (2 bytes)
      uint16_t msgLen = ring_read(hostRing);
      msgLen |= ((uint16_t)ring_read(hostRing) << 8);
      
      if (msgLen > 0 && msgLen <= sizeof(VprRpcMessage)) {
        VprRpcMessage req;
        for (uint16_t i = 0; i < msgLen; i++) {
          ((uint8_t*)&req)[i] = ring_read(hostRing);
        }
        
        VprRpcMessage rsp;
        process_rpc(&req, &rsp);
        
        // Send response via VPR ring
        uint16_t rspSize = sizeof(VprRpcMessage) - VPR_RPC_MAX_PAYLOAD + rsp.payloadLen;
        ring_write(vprRing, (uint8_t)(rspSize & 0xFF));
        ring_write(vprRing, (uint8_t)((rspSize >> 8) & 0xFF));
        for (uint16_t i = 0; i < rspSize; i++) {
          ring_write(vprRing, ((uint8_t*)&rsp)[i]);
        }
      }
    }
    
    // Yield to save power
    __asm__ volatile("wfi");
  }
}

// Weak default syscalls for bare-metal RISC-V
void __attribute__((weak)) _exit(int status) { while(1) __asm__ volatile("wfi"); }
int  __attribute__((weak)) _write(int fd, const char* buf, int len) { return len; }
int  __attribute__((weak)) _read(int fd, char* buf, int len) { return 0; }
void* __attribute__((weak)) _sbrk(int incr) { return (void*)-1; }
