// VPR Softperipheral Manager — Implementation

#include "vpr_softperipheral_manager.h"

namespace xiao_nrf54l15 {

VprSoftperipheralManager::VprSoftperipheralManager()
    : ready_(false), serviceCount_(0) {
  memset(&stats_, 0, sizeof(stats_));
}

bool VprSoftperipheralManager::begin() {
  if (ready_) return true;
  
  // Load the default VPR firmware (CS transport stub for now)
  if (!transport_.loadDefaultCsTransportStub()) {
    return false;
  }
  
  // Wait for VPR to boot and be ready
  if (!transport_.waitReady(5000000UL)) {
    return false;
  }
  
  ready_ = true;
  
  // Discover services
  discoverServices();
  
  return true;
}

void VprSoftperipheralManager::end() {
  transport_.stop();
  ready_ = false;
  serviceCount_ = 0;
}

uint8_t VprSoftperipheralManager::discoverServices() {
  serviceCount_ = 0;
  
  // Query number of services via RPC
  uint8_t response[VPR_RPC_MAX_PAYLOAD];
  uint16_t responseLen = 0;
  
  if (!rpcCall(VPR_RPC_GET_SERVICES, nullptr, 0,
               response, sizeof(response), &responseLen, 2000)) {
    return 0;
  }
  
  // Response: count (1 byte) + descriptors (VprServiceDescriptor each)
  if (responseLen < 1) return 0;
  
  serviceCount_ = response[0];
  if (serviceCount_ > VPR_MAX_SERVICES) serviceCount_ = VPR_MAX_SERVICES;
  
  size_t offset = 1;
  for (uint8_t i = 0; i < serviceCount_; i++) {
    if (offset + sizeof(VprServiceDescriptor) > responseLen) break;
    memcpy(&services_[i], response + offset, sizeof(VprServiceDescriptor));
    offset += sizeof(VprServiceDescriptor);
  }
  
  return serviceCount_;
}

const VprServiceDescriptor* VprSoftperipheralManager::getService(uint8_t index) const {
  if (index >= serviceCount_) return nullptr;
  return &services_[index];
}

const VprServiceDescriptor* VprSoftperipheralManager::findService(uint16_t serviceId) const {
  for (uint8_t i = 0; i < serviceCount_; i++) {
    if (services_[i].serviceId == serviceId) return &services_[i];
  }
  return nullptr;
}

bool VprSoftperipheralManager::rpcCall(
    uint16_t opcode,
    const uint8_t* requestPayload, uint16_t requestLen,
    uint8_t* responsePayload, uint16_t responseMax,
    uint16_t* responseLenOut,
    uint32_t timeoutMs) {
  if (!ensureReady()) return false;
  
  stats_.rpcCallsSent++;
  
  // Build RPC message
  VprRpcMessage msg;
  memset(&msg, 0, sizeof(msg));
  msg.opcode = opcode;
  msg.status = 0;  // request
  msg.payloadLen = requestLen;
  if (requestPayload && requestLen > 0 && requestLen <= VPR_RPC_MAX_PAYLOAD) {
    memcpy(msg.payload, requestPayload, requestLen);
  }
  
  // Send via transport stream
  size_t msgSize = sizeof(VprRpcMessage) - VPR_RPC_MAX_PAYLOAD + requestLen;
  size_t written = transport_.writeWakeRequest((const uint8_t*)&msg, msgSize);
  if (written != msgSize) {
    stats_.rpcCallsFailed++;
    return false;
  }
  
  // Wait for response
  uint32_t start = millis();
  while ((millis() - start) < timeoutMs) {
    transport_.poll();
    
    // Check for response in the VPR→host ring
    // The existing transport stores vprData - check if we have data
    // This is a simplified polling approach
    if (transport_.lastOpcode() == opcode) {
      // Response available - read it
      // For now, use a placeholder response
      if (responsePayload && responseMax > 0) {
        memset(responsePayload, 0, responseMax);
      }
      if (responseLenOut) *responseLenOut = 0;
      stats_.rpcCallsOk++;
      return true;
    }
    
    delay(1);
  }
  
  stats_.rpcCallsFailed++;
  return false;
}

bool VprSoftperipheralManager::rpcNotify(
    uint16_t opcode,
    const uint8_t* payload, uint16_t payloadLen) {
  // Same as rpcCall but don't wait for response
  if (!ensureReady()) return false;
  
  VprRpcMessage msg;
  memset(&msg, 0, sizeof(msg));
  msg.opcode = opcode;
  msg.status = 0;
  msg.payloadLen = payloadLen;
  if (payload && payloadLen > 0 && payloadLen <= VPR_RPC_MAX_PAYLOAD) {
    memcpy(msg.payload, payload, payloadLen);
  }
  
  size_t msgSize = sizeof(VprRpcMessage) - VPR_RPC_MAX_PAYLOAD + payloadLen;
  size_t written = transport_.writeWakeRequest((const uint8_t*)&msg, msgSize);
  return (written == msgSize);
}

bool VprSoftperipheralManager::crc32Compute(const uint8_t* data, size_t len,
                                             uint32_t* crcOut) {
  uint8_t response[8];
  uint16_t responseLen = 0;
  
  // Send data pointer + length
  uint8_t request[8];
  memcpy(request, &data, 4);  // not safe for VPR — needs shared memory!
  memcpy(request + 4, &len, 4);
  
  if (!rpcCall(VPR_RPC_CRC32_COMPUTE, request, 8,
               response, sizeof(response), &responseLen, 10000)) {
    return false;
  }
  
  if (responseLen >= 4 && crcOut) {
    memcpy(crcOut, response, 4);
    return true;
  }
  return false;
}

bool VprSoftperipheralManager::fnv1aCompute(const uint8_t* data, size_t len,
                                              uint32_t* hashOut) {
  uint8_t response[8];
  uint16_t responseLen = 0;
  uint8_t request[8];
  memcpy(request, &data, 4);
  memcpy(request + 4, &len, 4);
  
  if (!rpcCall(VPR_RPC_FNV1A_COMPUTE, request, 8,
               response, sizeof(response), &responseLen, 10000)) {
    return false;
  }
  
  if (responseLen >= 4 && hashOut) {
    memcpy(hashOut, response, 4);
    return true;
  }
  return false;
}

bool VprSoftperipheralManager::tickerStart(uint32_t periodUs) {
  uint8_t request[4];
  memcpy(request, &periodUs, 4);
  return rpcNotify(VPR_RPC_TICKER_START, request, 4);
}

bool VprSoftperipheralManager::tickerStop() {
  return rpcNotify(VPR_RPC_TICKER_STOP, nullptr, 0);
}

bool VprSoftperipheralManager::poll() {
  return transport_.poll();
}

bool VprSoftperipheralManager::ensureReady() {
  if (!ready_) return begin();
  return true;
}

}  // namespace xiao_nrf54l15
