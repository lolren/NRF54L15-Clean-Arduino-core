// VPR sQSPI SoftPeripheral — Implementation

#include "vpr_sqspi.h"

namespace xiao_nrf54l15 {

VprSQspi::VprSQspi() : ready_(false) {}

bool VprSQspi::begin() {
  if (ready_) return true;
  
  if (!vpr_.begin()) return false;
  
  // Send sQSPI init command
  // Request: [io_level(1), clock_freq_mhz(1), quad_enable(1)]
  uint8_t req[3] = {3, 8, 1};  // IO3, 8MHz, quad enable
  uint8_t rsp[VPR_RPC_MAX_PAYLOAD];
  uint16_t rspLen = 0;
  
  if (!rpc(VPR_RPC_SQSPI_INIT, req, 3, rsp, sizeof(rsp), &rspLen)) {
    return false;
  }
  
  ready_ = (rspLen >= 1 && rsp[0] == 0x00);
  return ready_;
}

void VprSQspi::end(bool stopVpr) {
  if (ready_) {
    rpc(VPR_RPC_SQSPI_DEINIT, nullptr, 0, nullptr, 0, nullptr);
    ready_ = false;
  }
  if (stopVpr) vpr_.end();
}

bool VprSQspi::read(uint32_t addr, uint8_t* buffer, size_t len) {
  if (!ready_ || !buffer || len == 0) return false;
  
  // Build request: addr(3 bytes LE) + len(2 bytes LE)
  uint8_t req[8];
  req[0] = (uint8_t)(addr & 0xFF);
  req[1] = (uint8_t)((addr >> 8) & 0xFF);
  req[2] = (uint8_t)((addr >> 16) & 0xFF);
  req[3] = 0x00;  // addr MSB padding
  req[4] = (uint8_t)(len & 0xFF);
  req[5] = (uint8_t)((len >> 8) & 0xFF);
  
  // Allocate response buffer on stack for small reads,
  // or read in chunks for larger reads
  const size_t kChunkSize = 64;
  for (size_t offset = 0; offset < len; offset += kChunkSize) {
    size_t chunk = len - offset;
    if (chunk > kChunkSize) chunk = kChunkSize;
    
    // Update address and length for this chunk
    uint32_t chunkAddr = addr + offset;
    req[0] = (uint8_t)(chunkAddr & 0xFF);
    req[1] = (uint8_t)((chunkAddr >> 8) & 0xFF);
    req[2] = (uint8_t)((chunkAddr >> 16) & 0xFF);
    req[4] = (uint8_t)(chunk & 0xFF);
    req[5] = (uint8_t)((chunk >> 8) & 0xFF);
    
    uint8_t rsp[kChunkSize + 4];
    uint16_t rspLen = 0;
    
    if (!rpc(VPR_RPC_SQSPI_READ, req, 6, rsp, sizeof(rsp), &rspLen)) {
      return false;
    }
    
    // Response contains the read data directly
    if (rspLen < chunk) return false;
    memcpy(buffer + offset, rsp, chunk);
  }
  
  return true;
}

bool VprSQspi::write(uint32_t addr, const uint8_t* data, size_t len) {
  if (!ready_ || !data || len == 0) return false;
  
  // Build request: addr(4) + data
  const size_t kMaxWrite = VPR_RPC_MAX_PAYLOAD - 4;
  if (len > kMaxWrite) len = kMaxWrite;
  
  uint8_t req[VPR_RPC_MAX_PAYLOAD];
  req[0] = (uint8_t)(addr & 0xFF);
  req[1] = (uint8_t)((addr >> 8) & 0xFF);
  req[2] = (uint8_t)((addr >> 16) & 0xFF);
  req[3] = 0x00;
  memcpy(req + 4, data, len);
  
  uint8_t rsp[16];
  uint16_t rspLen = 0;
  return rpc(VPR_RPC_SQSPI_WRITE, req, 4 + len, rsp, sizeof(rsp), &rspLen);
}

bool VprSQspi::eraseSector(uint32_t addr) {
  if (!ready_) return false;
  
  uint8_t req[4];
  req[0] = (uint8_t)(addr & 0xFF);
  req[1] = (uint8_t)((addr >> 8) & 0xFF);
  req[2] = (uint8_t)((addr >> 16) & 0xFF);
  req[3] = 0x00;  // 0 = 4KB sector, 1 = 64KB block, 2 = chip
  
  uint8_t rsp[16];
  uint16_t rspLen = 0;
  return rpc(VPR_RPC_SQSPI_ERASE, req, 4, rsp, sizeof(rsp), &rspLen);
}

bool VprSQspi::eraseBlock(uint32_t addr) {
  if (!ready_) return false;
  
  uint8_t req[4];
  req[0] = (uint8_t)(addr & 0xFF);
  req[1] = (uint8_t)((addr >> 8) & 0xFF);
  req[2] = (uint8_t)((addr >> 16) & 0xFF);
  req[3] = 0x01;  // 64KB block
  
  uint8_t rsp[16];
  uint16_t rspLen = 0;
  return rpc(VPR_RPC_SQSPI_ERASE, req, 4, rsp, sizeof(rsp), &rspLen);
}

bool VprSQspi::eraseChip() {
  if (!ready_) return false;
  
  uint8_t req[4] = {0, 0, 0, 0x02};  // chip erase
  uint8_t rsp[16];
  uint16_t rspLen = 0;
  return rpc(VPR_RPC_SQSPI_ERASE, req, 4, rsp, sizeof(rsp), &rspLen);
}

bool VprSQspi::getJedecId(uint8_t idOut[3]) {
  if (!ready_ || !idOut) return false;
  
  uint8_t rsp[16];
  uint16_t rspLen = 0;
  if (!rpc(VPR_RPC_SQSPI_STATUS, nullptr, 0, rsp, sizeof(rsp), &rspLen)) {
    return false;
  }
  
  if (rspLen >= 3) {
    memcpy(idOut, rsp, 3);
    return true;
  }
  return false;
}

bool VprSQspi::getUniqueId(uint8_t idOut[8]) {
  if (!ready_ || !idOut) return false;
  
  uint8_t rsp[16];
  uint16_t rspLen = 0;
  
  // Request unique ID (sub-opcode 0x01 in status request)
  uint8_t req[1] = {0x01};
  if (!rpc(VPR_RPC_SQSPI_STATUS, req, 1, rsp, sizeof(rsp), &rspLen)) {
    return false;
  }
  
  if (rspLen >= 8) {
    memcpy(idOut, rsp, 8);
    return true;
  }
  return false;
}

bool VprSQspi::readStatus(uint8_t* statusOut) {
  if (!ready_ || !statusOut) return false;
  
  uint8_t rsp[16];
  uint16_t rspLen = 0;
  
  uint8_t req[1] = {0x02};  // sub-opcode: read status register
  if (!rpc(VPR_RPC_SQSPI_STATUS, req, 1, rsp, sizeof(rsp), &rspLen)) {
    return false;
  }
  
  if (rspLen >= 1) {
    *statusOut = rsp[0];
    return true;
  }
  return false;
}

bool VprSQspi::waitReady(uint32_t timeoutMs) {
  if (!ready_) return false;
  
  uint32_t start = millis();
  while ((millis() - start) < timeoutMs) {
    uint8_t status = 0;
    if (readStatus(&status) && (status & 0x01) == 0) {
      return true;  // not busy
    }
    delay(1);
  }
  return false;
}

bool VprSQspi::rpc(uint16_t opcode,
                   const uint8_t* req, uint16_t reqLen,
                   uint8_t* rsp, uint16_t rspMax, uint16_t* rspLen) {
  return vpr_.rpcCall(opcode, req, reqLen, rsp, rspMax, rspLen, 5000);
}

}  // namespace xiao_nrf54l15
