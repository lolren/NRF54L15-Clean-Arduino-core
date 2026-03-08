#include <Arduino.h>

#include "zigbee_persistence.h"
#include "zigbee_security.h"
#include "zigbee_stack.h"

using namespace xiao_nrf54l15;

static uint32_t g_passCount = 0U;
static uint32_t g_totalCount = 0U;

static void reportResult(const char* name, bool pass, const char* detail) {
  Serial.print(pass ? "[PASS] " : "[FAIL] ");
  Serial.print(name);
  if (detail != nullptr && detail[0] != '\0') {
    Serial.print(" : ");
    Serial.print(detail);
  }
  Serial.print("\r\n");
  ++g_totalCount;
  if (pass) {
    ++g_passCount;
  }
}

static bool containsByteSequence(const uint8_t* payload, uint8_t payloadLength,
                                 const uint8_t* sequence,
                                 uint8_t sequenceLength) {
  if (payload == nullptr || sequence == nullptr || sequenceLength == 0U ||
      payloadLength < sequenceLength) {
    return false;
  }
  for (uint8_t i = 0U; i <= static_cast<uint8_t>(payloadLength - sequenceLength); ++i) {
    bool match = true;
    for (uint8_t j = 0U; j < sequenceLength; ++j) {
      if (payload[i + j] != sequence[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

static bool testNwkCodec() {
  static const uint8_t kPayload[] = {0x08U, 0x07U, 0x06U, 0x05U};

  ZigbeeNetworkFrame frame{};
  frame.frameType = ZigbeeNwkFrameType::kData;
  frame.destinationShort = 0x0000U;
  frame.sourceShort = 0x3344U;
  frame.radius = 30U;
  frame.sequence = 0x11U;

  uint8_t encoded[127] = {0};
  uint8_t encodedLength = 0U;
  const bool built = ZigbeeCodec::buildNwkFrame(frame, kPayload, sizeof(kPayload),
                                                encoded, &encodedLength);

  ZigbeeNetworkFrame parsed{};
  const bool parsedOk = built &&
                        ZigbeeCodec::parseNwkFrame(encoded, encodedLength, &parsed) &&
                        parsed.valid &&
                        parsed.destinationShort == frame.destinationShort &&
                        parsed.sourceShort == frame.sourceShort &&
                        parsed.radius == frame.radius &&
                        parsed.sequence == frame.sequence &&
                        parsed.payloadLength == sizeof(kPayload) &&
                        memcmp(parsed.payload, kPayload, sizeof(kPayload)) == 0;

  reportResult("NWK Codec", parsedOk, "build+parse");
  return parsedOk;
}

static bool testApsCodec() {
  static const uint8_t kPayload[] = {0x18U, 0x44U, 0x55U};

  ZigbeeApsDataFrame frame{};
  frame.frameType = ZigbeeApsFrameType::kData;
  frame.deliveryMode = kZigbeeApsDeliveryUnicast;
  frame.destinationEndpoint = 1U;
  frame.clusterId = kZigbeeClusterOnOff;
  frame.profileId = kZigbeeProfileHomeAutomation;
  frame.sourceEndpoint = 1U;
  frame.counter = 0x20U;

  uint8_t encoded[127] = {0};
  uint8_t encodedLength = 0U;
  const bool built = ZigbeeCodec::buildApsDataFrame(frame, kPayload, sizeof(kPayload),
                                                    encoded, &encodedLength);

  ZigbeeApsDataFrame parsed{};
  const bool parsedOk =
      built && ZigbeeCodec::parseApsDataFrame(encoded, encodedLength, &parsed) &&
      parsed.valid && parsed.destinationEndpoint == frame.destinationEndpoint &&
      parsed.clusterId == frame.clusterId &&
      parsed.profileId == frame.profileId &&
      parsed.sourceEndpoint == frame.sourceEndpoint &&
      parsed.counter == frame.counter &&
      parsed.payloadLength == sizeof(kPayload) &&
      memcmp(parsed.payload, kPayload, sizeof(kPayload)) == 0;

  ZigbeeApsDataFrame groupFrame{};
  groupFrame.frameType = ZigbeeApsFrameType::kData;
  groupFrame.deliveryMode = kZigbeeApsDeliveryGroup;
  groupFrame.destinationGroup = 0x1001U;
  groupFrame.clusterId = kZigbeeClusterOnOff;
  groupFrame.profileId = kZigbeeProfileHomeAutomation;
  groupFrame.sourceEndpoint = 1U;
  groupFrame.counter = 0x21U;

  uint8_t groupEncoded[127] = {0};
  uint8_t groupEncodedLength = 0U;
  const bool groupBuilt = ZigbeeCodec::buildApsDataFrame(
      groupFrame, kPayload, sizeof(kPayload), groupEncoded, &groupEncodedLength);

  ZigbeeApsDataFrame groupParsed{};
  const bool groupParsedOk =
      groupBuilt &&
      ZigbeeCodec::parseApsDataFrame(groupEncoded, groupEncodedLength,
                                     &groupParsed) &&
      groupParsed.valid &&
      groupParsed.deliveryMode == kZigbeeApsDeliveryGroup &&
      groupParsed.destinationGroup == groupFrame.destinationGroup &&
      groupParsed.clusterId == groupFrame.clusterId &&
      groupParsed.profileId == groupFrame.profileId &&
      groupParsed.sourceEndpoint == groupFrame.sourceEndpoint &&
      groupParsed.counter == groupFrame.counter &&
      groupParsed.payloadLength == sizeof(kPayload) &&
      memcmp(groupParsed.payload, kPayload, sizeof(kPayload)) == 0;

  const bool ok = parsedOk && groupParsedOk;
  reportResult("APS Codec", ok, "unicast+group");
  return ok;
}

static bool testApsCommandCodec() {
  static const uint8_t kPayload[] = {0xAAU, 0x55U, 0x01U};

  ZigbeeApsCommandFrame command{};
  command.frameType = ZigbeeApsFrameType::kCommand;
  command.deliveryMode = kZigbeeApsDeliveryUnicast;
  command.counter = 0x31U;
  command.commandId = kZigbeeApsCommandTransportKey;

  uint8_t encoded[127] = {0U};
  uint8_t encodedLength = 0U;
  bool ok = ZigbeeCodec::buildApsCommandFrame(command, kPayload,
                                              sizeof(kPayload), encoded,
                                              &encodedLength);

  ZigbeeApsCommandFrame parsed{};
  ok = ok && ZigbeeCodec::parseApsCommandFrame(encoded, encodedLength, &parsed) &&
       parsed.valid &&
       parsed.frameType == ZigbeeApsFrameType::kCommand &&
       parsed.deliveryMode == kZigbeeApsDeliveryUnicast &&
       parsed.counter == command.counter &&
       parsed.commandId == command.commandId &&
       parsed.payloadLength == sizeof(kPayload) &&
       memcmp(parsed.payload, kPayload, sizeof(kPayload)) == 0;

  ZigbeeApsTransportKey transportKey{};
  transportKey.valid = true;
  transportKey.keyType = kZigbeeApsTransportKeyStandardNetworkKey;
  for (uint8_t i = 0U; i < sizeof(transportKey.key); ++i) {
    transportKey.key[i] = static_cast<uint8_t>(0x10U + i);
  }
  transportKey.keySequence = 0x02U;
  transportKey.destinationIeee = 0x00124B0001ABCDEFULL;
  transportKey.sourceIeee = 0x00124B000054A11FULL;

  ok = ok && ZigbeeCodec::buildApsTransportKeyCommand(
                   transportKey, 0x32U, encoded, &encodedLength);
  uint8_t parsedCounter = 0U;
  ZigbeeApsTransportKey parsedKey{};
  ok = ok && ZigbeeCodec::parseApsTransportKeyCommand(
                   encoded, encodedLength, &parsedKey, &parsedCounter) &&
       parsedCounter == 0x32U && parsedKey.valid &&
       parsedKey.keyType == transportKey.keyType &&
       memcmp(parsedKey.key, transportKey.key, sizeof(parsedKey.key)) == 0 &&
       parsedKey.keySequence == transportKey.keySequence &&
       parsedKey.destinationIeee == transportKey.destinationIeee &&
       parsedKey.sourceIeee == transportKey.sourceIeee;

  uint8_t linkKey[16] = {0U};
  ZigbeeApsSecurityHeader apsSecurity{};
  apsSecurity.valid = true;
  apsSecurity.securityControl = kZigbeeSecurityControlApsEncMic32;
  apsSecurity.frameCounter = 0x01020304UL;
  apsSecurity.sourceIeee = 0x00124B000054A11FULL;
  ok = ok && ZigbeeSecurity::loadZigbeeAlliance09LinkKey(linkKey) &&
       ZigbeeSecurity::buildSecuredApsTransportKeyCommand(
           transportKey, apsSecurity, linkKey, 0x34U, encoded, &encodedLength);
  ZigbeeApsTransportKey parsedSecuredKey{};
  ZigbeeApsSecurityHeader parsedApsSecurity{};
  parsedCounter = 0U;
  ok = ok && ZigbeeSecurity::parseSecuredApsTransportKeyCommand(
                   encoded, encodedLength, linkKey, &parsedSecuredKey,
                   &parsedApsSecurity, &parsedCounter) &&
       parsedCounter == 0x34U && parsedSecuredKey.valid &&
       parsedSecuredKey.keySequence == transportKey.keySequence &&
       memcmp(parsedSecuredKey.key, transportKey.key,
              sizeof(parsedSecuredKey.key)) == 0 &&
       parsedSecuredKey.destinationIeee == transportKey.destinationIeee &&
       parsedSecuredKey.sourceIeee == transportKey.sourceIeee &&
       parsedApsSecurity.valid &&
       parsedApsSecurity.frameCounter == apsSecurity.frameCounter &&
       parsedApsSecurity.sourceIeee == apsSecurity.sourceIeee;

  ZigbeeApsUpdateDevice updateDevice{};
  updateDevice.valid = true;
  updateDevice.deviceIeee = 0x00124B0001AC1001ULL;
  updateDevice.deviceShort = 0x3344U;
  updateDevice.status = kZigbeeApsUpdateDeviceStatusStandardSecureRejoin;
  ok = ok && ZigbeeCodec::buildApsUpdateDeviceCommand(
                   updateDevice, 0x33U, encoded, &encodedLength);
  ZigbeeApsUpdateDevice parsedDevice{};
  parsedCounter = 0U;
  ok = ok && ZigbeeCodec::parseApsUpdateDeviceCommand(
                   encoded, encodedLength, &parsedDevice, &parsedCounter) &&
       parsedCounter == 0x33U && parsedDevice.valid &&
       parsedDevice.deviceIeee == updateDevice.deviceIeee &&
       parsedDevice.deviceShort == updateDevice.deviceShort &&
       parsedDevice.status == updateDevice.status;

  reportResult("APS Command Codec", ok, "command+transport_key+update_device");
  return ok;
}

static bool testMacCodec() {
  uint8_t encoded[127] = {0};
  uint8_t encodedLength = 0U;
  bool ok = ZigbeeCodec::buildAssociationRequest(0x21U, 0x1A2BU, 0x0000U,
                                                 0x00124B0001ABCDEFULL, 0x8EU,
                                                 encoded, &encodedLength);

  ZigbeeMacAssociationRequestView assocReq{};
  ok = ok &&
       ZigbeeCodec::parseAssociationRequest(encoded, encodedLength, &assocReq) &&
       assocReq.valid && assocReq.sequence == 0x21U &&
       assocReq.coordinatorPanId == 0x1A2BU &&
       assocReq.coordinatorShort == 0x0000U &&
       assocReq.deviceExtended == 0x00124B0001ABCDEFULL &&
       assocReq.capabilityInformation == 0x8EU;

  ok = ok &&
       ZigbeeCodec::buildAssociationResponse(0x22U, 0x1A2BU,
                                             0x00124B0001ABCDEFULL, 0x0000U,
                                             0x3344U, 0x00U, encoded,
                                             &encodedLength);
  ZigbeeMacAssociationResponseView assocRsp{};
  ok = ok &&
       ZigbeeCodec::parseAssociationResponse(encoded, encodedLength, &assocRsp) &&
       assocRsp.valid && assocRsp.sequence == 0x22U &&
       assocRsp.panId == 0x1A2BU &&
       assocRsp.coordinatorShort == 0x0000U &&
       assocRsp.destinationExtended == 0x00124B0001ABCDEFULL &&
       assocRsp.assignedShort == 0x3344U &&
       assocRsp.status == 0x00U;

  ok = ok && ZigbeeCodec::buildBeaconRequest(0x23U, encoded, &encodedLength);
  ZigbeeMacFrame beaconReq{};
  ok = ok && ZigbeeCodec::parseMacFrame(encoded, encodedLength, &beaconReq) &&
       beaconReq.valid &&
       beaconReq.frameType == ZigbeeMacFrameType::kCommand &&
       beaconReq.commandId == kZigbeeMacCommandBeaconRequest &&
       beaconReq.destination.mode == ZigbeeMacAddressMode::kShort &&
       beaconReq.destination.panId == 0xFFFFU &&
       beaconReq.destination.shortAddress == 0xFFFFU;

  ok = ok && ZigbeeCodec::buildDataRequest(0x24U, 0x1A2BU, 0x0000U,
                                           0x00124B0001ABCDEFULL, encoded,
                                           &encodedLength);
  ZigbeeMacFrame dataReq{};
  ok = ok && ZigbeeCodec::parseMacFrame(encoded, encodedLength, &dataReq) &&
       dataReq.valid &&
       dataReq.frameType == ZigbeeMacFrameType::kCommand &&
       dataReq.commandId == kZigbeeMacCommandDataRequest;

  reportResult("MAC Codec", ok, "assoc+beacon+datareq");
  return ok;
}

static bool testBeaconAndZdoClientCodec() {
  ZigbeeMacBeaconPayload beaconPayload{};
  beaconPayload.valid = true;
  beaconPayload.protocolId = 0U;
  beaconPayload.stackProfile = 2U;
  beaconPayload.protocolVersion = 2U;
  beaconPayload.routerCapacity = true;
  beaconPayload.endDeviceCapacity = true;
  beaconPayload.extendedPanId = 0x00124B000054C0DEULL;
  beaconPayload.txOffset = 0x0055AA33UL;
  beaconPayload.updateId = 7U;

  uint8_t encoded[127] = {0};
  uint8_t encodedLength = 0U;
  bool ok = ZigbeeCodec::buildBeaconFrame(0x30U, 0x1A2BU, 0x0000U,
                                          beaconPayload, encoded,
                                          &encodedLength);

  ZigbeeMacBeaconView parsedBeacon{};
  ok = ok &&
       ZigbeeCodec::parseBeaconFrame(encoded, encodedLength, &parsedBeacon) &&
       parsedBeacon.valid && parsedBeacon.sequence == 0x30U &&
       parsedBeacon.panId == 0x1A2BU &&
       parsedBeacon.sourceShort == 0x0000U &&
       parsedBeacon.associationPermit &&
       parsedBeacon.panCoordinator &&
       parsedBeacon.network.stackProfile == 2U &&
       parsedBeacon.network.protocolVersion == 2U &&
       parsedBeacon.network.extendedPanId == 0x00124B000054C0DEULL &&
       parsedBeacon.network.txOffset == 0x0055AA33UL &&
       parsedBeacon.network.updateId == 7U;

  uint8_t zdoPayload[127] = {0};
  uint8_t zdoLength = 0U;
  ok = ok &&
       ZigbeeCodec::buildZdoActiveEndpointsRequest(0x31U, 0x3344U, zdoPayload,
                                                   &zdoLength) &&
       zdoLength == 3U && zdoPayload[0] == 0x31U &&
       zdoPayload[1] == 0x44U && zdoPayload[2] == 0x33U;

  ok = ok &&
       ZigbeeCodec::buildZdoSimpleDescriptorRequest(0x32U, 0x3344U, 0x01U,
                                                    zdoPayload, &zdoLength) &&
       zdoLength == 4U && zdoPayload[0] == 0x32U &&
       zdoPayload[3] == 0x01U;

  const uint8_t activeEpResponse[] = {0x41U, 0x00U, 0x44U, 0x33U, 0x02U, 0x01U, 0x0BU};
  ZigbeeZdoActiveEndpointsResponseView activeEp{};
  ok = ok &&
       ZigbeeCodec::parseZdoActiveEndpointsResponse(activeEpResponse,
                                                    sizeof(activeEpResponse),
                                                    &activeEp) &&
       activeEp.valid && activeEp.status == 0x00U &&
       activeEp.nwkAddressOfInterest == 0x3344U &&
       activeEp.endpointCount == 2U &&
       activeEp.endpoints[0] == 0x01U &&
       activeEp.endpoints[1] == 0x0BU;

  const uint8_t simpleDescResponse[] = {
      0x42U, 0x00U, 0x44U, 0x33U, 0x10U, 0x01U, 0x04U, 0x01U, 0x00U, 0x01U,
      0x02U, 0x00U, 0x00U, 0x03U, 0x00U, 0x06U, 0x00U, 0x01U, 0x19U, 0x00U};
  ZigbeeZdoSimpleDescriptorResponseView simpleDesc{};
  ok = ok &&
       ZigbeeCodec::parseZdoSimpleDescriptorResponse(
           simpleDescResponse, sizeof(simpleDescResponse), &simpleDesc) &&
       simpleDesc.valid && simpleDesc.status == 0x00U &&
       simpleDesc.nwkAddressOfInterest == 0x3344U &&
       simpleDesc.endpoint == 0x01U &&
       simpleDesc.profileId == 0x0104U &&
       simpleDesc.deviceId == 0x0100U &&
       simpleDesc.inputClusterCount == 2U &&
       simpleDesc.inputClusters[0] == 0x0000U &&
       simpleDesc.inputClusters[1] == 0x0003U &&
       simpleDesc.outputClusterCount == 1U &&
       simpleDesc.outputClusters[0] == 0x0019U;

  reportResult("Beacon+ZDO Client", ok, "beacon+requests+parsers");
  return ok;
}

static bool testNwkSecurityCodec() {
  uint8_t key[16] = {0U};
  static const uint8_t kAlliance09Key[16] = {
      'Z', 'i', 'g', 'B', 'e', 'e', 'A', 'l',
      'l', 'i', 'a', 'n', 'c', 'e', '0', '9'};
  bool ok = ZigbeeSecurity::loadZigbeeAlliance09LinkKey(key) &&
            memcmp(key, kAlliance09Key, sizeof(key)) == 0;

  ZigbeeNwkSecurityHeader security{};
  security.valid = true;
  security.securityControl = kZigbeeSecurityControlNwkEncMic32;
  security.frameCounter = 0x01020304UL;
  security.sourceIeee = 0x00124B0001ABCDEFULL;
  security.keySequence = 0x01U;

  uint8_t encodedSecurity[32] = {0};
  uint8_t encodedSecurityLength = 0U;
  ok = ok && ZigbeeSecurity::buildNwkSecurityHeader(
                   security, encodedSecurity, &encodedSecurityLength);

  ZigbeeNwkSecurityHeader parsedSecurity{};
  uint8_t parsedSecurityLength = 0U;
  ok = ok && ZigbeeSecurity::parseNwkSecurityHeader(
                   encodedSecurity, encodedSecurityLength, &parsedSecurity,
                   &parsedSecurityLength) &&
       parsedSecurity.valid &&
       parsedSecurity.securityControl == security.securityControl &&
       parsedSecurity.frameCounter == security.frameCounter &&
       parsedSecurity.sourceIeee == security.sourceIeee &&
       parsedSecurity.keySequence == security.keySequence &&
       parsedSecurityLength == encodedSecurityLength;

  uint8_t nonce[13] = {0U};
  ok = ok && ZigbeeSecurity::buildNwkNonce(
                   security.sourceIeee, security.frameCounter,
                   security.securityControl, nonce);

  static const uint8_t kAad[] = {0x48U, 0x02U, 0x34U, 0x12U, 0x78U, 0x56U};
  static const uint8_t kPayload[] = {0x18U, 0x52U, 0x01U, 0x00U, 0x00U};
  uint8_t ciphertext[32] = {0U};
  uint8_t ciphertextLength = 0U;
  ok = ok &&
       ZigbeeSecurity::encryptCcmStar(key, nonce, kAad, sizeof(kAad), kPayload,
                                      sizeof(kPayload), ciphertext,
                                      &ciphertextLength);

  uint8_t plaintext[32] = {0U};
  uint8_t plaintextLength = 0U;
  ok = ok &&
       ZigbeeSecurity::decryptCcmStar(key, nonce, kAad, sizeof(kAad), ciphertext,
                                      ciphertextLength, plaintext,
                                      &plaintextLength) &&
       plaintextLength == sizeof(kPayload) &&
       memcmp(plaintext, kPayload, sizeof(kPayload)) == 0;

  ZigbeeNetworkFrame frame{};
  frame.frameType = ZigbeeNwkFrameType::kData;
  frame.securityEnabled = true;
  frame.destinationShort = 0x0000U;
  frame.sourceShort = 0x3344U;
  frame.radius = 30U;
  frame.sequence = 0x11U;

  uint8_t securedFrame[127] = {0U};
  uint8_t securedFrameLength = 0U;
  ok = ok && ZigbeeSecurity::buildSecuredNwkFrame(
                   frame, security, key, kPayload, sizeof(kPayload), securedFrame,
                   &securedFrameLength);

  ZigbeeNetworkFrame parsedFrame{};
  ZigbeeNwkSecurityHeader parsedFrameSecurity{};
  uint8_t parsedPayload[127] = {0U};
  uint8_t parsedPayloadLength = 0U;
  ok = ok && ZigbeeSecurity::parseSecuredNwkFrame(
                   securedFrame, securedFrameLength, key, &parsedFrame,
                   &parsedFrameSecurity, parsedPayload, &parsedPayloadLength) &&
       parsedFrame.valid && parsedFrame.securityEnabled &&
       parsedFrame.destinationShort == frame.destinationShort &&
       parsedFrame.sourceShort == frame.sourceShort &&
       parsedFrame.radius == frame.radius &&
       parsedFrame.sequence == frame.sequence &&
       parsedPayloadLength == sizeof(kPayload) &&
       memcmp(parsedPayload, kPayload, sizeof(kPayload)) == 0 &&
       parsedFrameSecurity.frameCounter == security.frameCounter &&
       parsedFrameSecurity.sourceIeee == security.sourceIeee &&
       parsedFrameSecurity.keySequence == security.keySequence;

  uint8_t tamperedFrame[127] = {0U};
  memcpy(tamperedFrame, securedFrame, securedFrameLength);
  tamperedFrame[securedFrameLength - 1U] ^= 0x01U;
  parsedPayloadLength = 0U;
  ok = ok &&
       !ZigbeeSecurity::parseSecuredNwkFrame(
           tamperedFrame, securedFrameLength, key, &parsedFrame,
           &parsedFrameSecurity, parsedPayload, &parsedPayloadLength);

  reportResult("NWK Security", ok, "aux+ccm+secured_frame");
  return ok;
}

static bool testZdoBindingCodecAndFlow() {
  uint8_t payload[127] = {0};
  uint8_t payloadLength = 0U;
  bool ok = ZigbeeCodec::buildZdoBindRequest(
      0x33U, 0x00124B0001ABCDEFULL, 0x01U, kZigbeeClusterOnOff,
      ZigbeeBindingAddressMode::kExtended, 0U, 0x00124B000054A11FULL, 0x01U,
      payload, &payloadLength);
  ok = ok && payloadLength == 22U && payload[0] == 0x33U && payload[9] == 0x01U &&
       payload[10] == 0x06U && payload[11] == 0x00U &&
       payload[12] == static_cast<uint8_t>(ZigbeeBindingAddressMode::kExtended) &&
       payload[21] == 0x01U;

  ok = ok && ZigbeeCodec::buildZdoUnbindRequest(
                   0x34U, 0x00124B0001ABCDEFULL, 0x01U, kZigbeeClusterOnOff,
                   ZigbeeBindingAddressMode::kExtended, 0U,
                   0x00124B000054A11FULL, 0x01U, payload, &payloadLength) &&
       payloadLength == 22U && payload[0] == 0x34U;

  uint8_t transactionSequence = 0U;
  uint8_t status = 0xFFU;
  const uint8_t bindRsp[] = {0x44U, 0x00U};
  ok = ok &&
       ZigbeeCodec::parseZdoStatusResponse(bindRsp, sizeof(bindRsp),
                                           &transactionSequence, &status) &&
       transactionSequence == 0x44U && status == 0x00U;

  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-BIND";
  basic.swBuildId = "0.3.0";
  basic.powerSource = 0x01U;
  ZigbeeHomeAutomationDevice device;
  ok = ok &&
       device.configureOnOffLight(1U, 0x00124B0001ABCDEFULL, 0x1234U, 0x1A2BU,
                                  basic, 0x0000U);

  payloadLength = 0U;
  ok = ok &&
       ZigbeeCodec::buildZdoBindRequest(
           0x45U, 0x00124B0001ABCDEFULL, 0x01U, kZigbeeClusterOnOff,
           ZigbeeBindingAddressMode::kExtended, 0U,
           0x00124B000054A11FULL, 0x02U, payload, &payloadLength);
  uint16_t responseClusterId = 0U;
  uint8_t response[127] = {0};
  uint8_t responseLength = 0U;
  ok = ok && device.handleZdoRequest(kZigbeeZdoBindRequest, payload,
                                     payloadLength, &responseClusterId,
                                     response, &responseLength) &&
       responseClusterId == kZigbeeZdoBindResponse &&
       ZigbeeCodec::parseZdoStatusResponse(response, responseLength,
                                           &transactionSequence, &status) &&
       status == 0x00U && device.bindingCount() == 1U;

  uint64_t boundIeee = 0U;
  uint8_t boundEndpoint = 0U;
  ok = ok &&
       device.resolveBindingDestination(0x01U, kZigbeeClusterOnOff, &boundIeee,
                                        &boundEndpoint) &&
       boundIeee == 0x00124B000054A11FULL && boundEndpoint == 0x02U;

  ok = ok && device.handleZdoRequest(kZigbeeZdoUnbindRequest, payload,
                                     payloadLength, &responseClusterId,
                                     response, &responseLength) &&
       responseClusterId == kZigbeeZdoUnbindResponse &&
       ZigbeeCodec::parseZdoStatusResponse(response, responseLength,
                                           &transactionSequence, &status) &&
       status == 0x00U && device.bindingCount() == 0U;

  reportResult("ZDO Binding", ok, "bind+unbind");
  return ok;
}

static bool testZdoDescriptors() {
  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-LIGHT";
  basic.swBuildId = "0.1.0";
  basic.powerSource = 0x01U;

  ZigbeeHomeAutomationDevice device;
  const bool configured =
      device.configureOnOffLight(1U, 0x00124B0001ABCDEFULL, 0x1234U, 0x1A2BU,
                                 basic, 0x0000U);

  uint8_t request[8] = {0x44U, 0x34U, 0x12U};
  uint16_t responseClusterId = 0U;
  uint8_t payload[127] = {0};
  uint8_t payloadLength = 0U;

  bool ok = configured &&
            device.handleZdoRequest(kZigbeeZdoActiveEndpointsRequest, request, 3U,
                                    &responseClusterId, payload, &payloadLength) &&
            responseClusterId == kZigbeeZdoActiveEndpointsResponse &&
            payloadLength == 5U &&
            payload[0] == 0x44U &&
            payload[1] == 0x00U &&
            payload[2] == 0x34U &&
            payload[3] == 0x12U &&
            payload[4] == 0x01U;

  uint8_t simpleRequest[8] = {0x45U, 0x34U, 0x12U, 0x01U};
  payloadLength = 0U;
  responseClusterId = 0U;
  ok = ok &&
       device.handleZdoRequest(kZigbeeZdoSimpleDescriptorRequest, simpleRequest, 4U,
                               &responseClusterId, payload, &payloadLength) &&
       responseClusterId == kZigbeeZdoSimpleDescriptorResponse &&
       payload[0] == 0x45U &&
       payload[1] == 0x00U &&
       payload[3] == 0x12U &&
       payload[4] >= 8U;

  reportResult("ZDO Descriptors", ok, "active_ep+simple_desc");
  return ok;
}

static bool testOnOffLightResponses() {
  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-LIGHT";
  basic.swBuildId = "0.1.0";
  basic.powerSource = 0x01U;

  ZigbeeHomeAutomationDevice device;
  if (!device.configureOnOffLight(1U, 0x00124B0001ABCDEFULL, 0x1234U, 0x1A2BU,
                                  basic, 0x0000U)) {
    reportResult("HA Light", false, "configure");
    return false;
  }

  const uint8_t readBasicReq[] = {0x00U, 0x34U, 0x04U, 0x00U, 0x05U, 0x00U};
  uint8_t response[127] = {0};
  uint8_t responseLength = 0U;
  bool ok = device.handleZclRequest(kZigbeeClusterBasic, readBasicReq,
                                    sizeof(readBasicReq), response,
                                    &responseLength);
  ZigbeeZclFrame parsed{};
  ok = ok && ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x01U &&
       parsed.transactionSequence == 0x34U;
  static const uint8_t kManufacturerTag[] = {
      0x04U, 0x00U, 0x00U, 0x42U, 0x09U, 'C', 'l', 'e', 'a', 'n', 'C', 'o', 'r', 'e'};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kManufacturerTag,
                                  sizeof(kManufacturerTag));

  const uint8_t onReq[] = {0x01U, 0x35U, 0x01U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterOnOff, onReq, sizeof(onReq),
                                     response, &responseLength) &&
       device.onOff();

  const uint8_t readOnOffReq[] = {0x00U, 0x36U, 0x00U, 0x00U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterOnOff, readOnOffReq,
                                     sizeof(readOnOffReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid;
  static const uint8_t kOnValue[] = {0x00U, 0x00U, 0x00U, 0x10U, 0x01U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kOnValue, sizeof(kOnValue));

  reportResult("HA Light", ok, "basic_read+onoff");
  return ok;
}

static bool testDimmableLightResponses() {
  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-DIM";
  basic.swBuildId = "0.3.0";
  basic.powerSource = 0x01U;

  ZigbeeHomeAutomationDevice device;
  bool ok =
      device.configureDimmableLight(1U, 0x00124B0001ABCDF0ULL, 0x1245U, 0x1A2BU,
                                    basic, 0x0000U) &&
      device.level() == 0xFEU && !device.onOff();

  const uint8_t readLevelReq[] = {0x00U, 0x41U, 0x00U, 0x00U, 0x02U, 0x00U,
                                  0x03U, 0x00U};
  uint8_t response[127] = {0};
  uint8_t responseLength = 0U;
  ZigbeeZclFrame parsed{};
  ok = ok && device.handleZclRequest(kZigbeeClusterLevelControl, readLevelReq,
                                     sizeof(readLevelReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x01U;
  static const uint8_t kCurrentLevelValue[] = {0x00U, 0x00U, 0x00U, 0x20U, 0xFEU};
  static const uint8_t kMinLevelValue[] = {0x02U, 0x00U, 0x00U, 0x20U, 0x01U};
  static const uint8_t kMaxLevelValue[] = {0x03U, 0x00U, 0x00U, 0x20U, 0xFEU};
  ok = ok &&
       containsByteSequence(parsed.payload, parsed.payloadLength, kCurrentLevelValue,
                            sizeof(kCurrentLevelValue)) &&
       containsByteSequence(parsed.payload, parsed.payloadLength, kMinLevelValue,
                            sizeof(kMinLevelValue)) &&
       containsByteSequence(parsed.payload, parsed.payloadLength, kMaxLevelValue,
                            sizeof(kMaxLevelValue));

  const uint8_t moveToLevelReq[] = {0x01U, 0x42U, 0x04U, 0x40U, 0x00U, 0x00U};
  responseLength = 0U;
  ok = ok &&
       device.handleZclRequest(kZigbeeClusterLevelControl, moveToLevelReq,
                               sizeof(moveToLevelReq), response, &responseLength) &&
       device.onOff() && device.level() == 0x40U;

  const uint8_t stepReq[] = {0x01U, 0x43U, 0x06U, 0x00U, 0x10U, 0x00U, 0x00U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterLevelControl, stepReq,
                                     sizeof(stepReq), response, &responseLength) &&
       device.level() == 0x50U;

  const uint8_t offReq[] = {0x01U, 0x44U, 0x00U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterOnOff, offReq, sizeof(offReq),
                                     response, &responseLength) &&
       !device.onOff() && device.level() == 0x50U;

  const uint8_t onReq[] = {0x01U, 0x45U, 0x01U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterOnOff, onReq, sizeof(onReq),
                                     response, &responseLength) &&
       device.onOff() && device.level() == 0x50U;

  const uint8_t configureReq[] = {0x00U, 0x46U, 0x00U, 0x00U, 0x00U,
                                  0x20U, 0x00U, 0x00U, 0x1EU, 0x00U, 0x10U};
  responseLength = 0U;
  ok = ok &&
       device.handleZclRequest(kZigbeeClusterLevelControl, configureReq,
                               sizeof(configureReq), response, &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x07U &&
       parsed.payloadLength == 1U && parsed.payload[0] == 0x00U &&
       device.reportingConfigurationCount() == 1U;

  responseLength = 0U;
  ok = ok && device.buildAttributeReport(kZigbeeClusterLevelControl, 0x47U,
                                         response, &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x0AU;
  static const uint8_t kLevelReport[] = {0x00U, 0x00U, 0x20U, 0x50U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kLevelReport, sizeof(kLevelReport));

  reportResult("HA Dimmable", ok, "level_read+cmd+report");
  return ok;
}

static bool testIdentifyGroupsScenesFlow() {
  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-SCENE";
  basic.swBuildId = "0.3.0";
  basic.powerSource = 0x01U;

  ZigbeeHomeAutomationDevice device;
  bool ok =
      device.configureDimmableLight(1U, 0x00124B0001ABCDD1ULL, 0x1246U, 0x1A2BU,
                                    basic, 0x0000U);

  uint8_t response[127] = {0};
  uint8_t responseLength = 0U;
  ZigbeeZclFrame parsed{};

  const uint8_t identifyReq[] = {0x01U, 0x51U, 0x00U, 0x0AU, 0x00U};
  ok = ok && device.handleZclRequest(kZigbeeClusterIdentify, identifyReq,
                                     sizeof(identifyReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x0BU;

  const uint8_t readIdentifyReq[] = {0x00U, 0x52U, 0x00U, 0x00U};
  responseLength = 0U;
  ok = ok &&
       device.handleZclRequest(kZigbeeClusterIdentify, readIdentifyReq,
                               sizeof(readIdentifyReq), response,
                               &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x01U;
  static const uint8_t kIdentifyTimeValue[] = {0x00U, 0x00U, 0x00U, 0x21U, 0x0AU,
                                               0x00U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kIdentifyTimeValue,
                                  sizeof(kIdentifyTimeValue));

  const uint8_t addGroupReq[] = {0x01U, 0x53U, 0x00U, 0x22U, 0x22U, 0x05U,
                                 'L',   'i',   'v',   'i',   'n'};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterGroups, addGroupReq,
                                     sizeof(addGroupReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x00U &&
       device.isInGroup(0x2222U);
  static const uint8_t kAddGroupRsp[] = {0x00U, 0x22U, 0x22U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kAddGroupRsp, sizeof(kAddGroupRsp));

  const uint8_t addGroupIfIdentifyingReq[] = {0x01U, 0x54U, 0x05U, 0x33U, 0x33U,
                                              0x04U, 'T',   'e',   's',   't'};
  responseLength = 0U;
  ok = ok &&
       device.handleZclRequest(kZigbeeClusterGroups, addGroupIfIdentifyingReq,
                               sizeof(addGroupIfIdentifyingReq), response,
                               &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x0BU &&
       device.isInGroup(0x3333U);

  const uint8_t viewGroupReq[] = {0x01U, 0x55U, 0x01U, 0x33U, 0x33U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterGroups, viewGroupReq,
                                     sizeof(viewGroupReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x01U;
  static const uint8_t kViewGroupPrefix[] = {0x00U, 0x33U, 0x33U, 0x04U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kViewGroupPrefix,
                                  sizeof(kViewGroupPrefix));

  const uint8_t addSceneReq[] = {
      0x01U, 0x56U, 0x00U, 0x22U, 0x22U, 0x07U, 0x10U, 0x00U, 0x07U,
      'E',   'v',   'e',   'n',   'i',   'n',   'g', 0x06U, 0x00U,
      0x01U, 0x01U, 0x08U, 0x00U, 0x01U, 0x33U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterScenes, addSceneReq,
                                     sizeof(addSceneReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x00U;
  static const uint8_t kAddSceneRsp[] = {0x00U, 0x22U, 0x22U, 0x07U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kAddSceneRsp, sizeof(kAddSceneRsp));

  device.setOnOff(false);
  device.setLevel(0x80U);
  const uint8_t recallSceneReq[] = {0x01U, 0x57U, 0x05U, 0x22U, 0x22U, 0x07U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterScenes, recallSceneReq,
                                     sizeof(recallSceneReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x0BU &&
       device.onOff() && device.level() == 0x33U;

  const uint8_t readScenesReq[] = {0x00U, 0x58U, 0x01U, 0x00U, 0x02U, 0x00U,
                                   0x03U, 0x00U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterScenes, readScenesReq,
                                     sizeof(readScenesReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x01U;
  static const uint8_t kCurrentSceneValue[] = {0x01U, 0x00U, 0x00U, 0x20U, 0x07U};
  static const uint8_t kCurrentGroupValue[] = {0x02U, 0x00U, 0x00U, 0x21U, 0x22U,
                                               0x22U};
  static const uint8_t kSceneValidValue[] = {0x03U, 0x00U, 0x00U, 0x10U, 0x01U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kCurrentSceneValue,
                                  sizeof(kCurrentSceneValue)) &&
       containsByteSequence(parsed.payload, parsed.payloadLength,
                            kCurrentGroupValue,
                            sizeof(kCurrentGroupValue)) &&
       containsByteSequence(parsed.payload, parsed.payloadLength,
                            kSceneValidValue, sizeof(kSceneValidValue));

  const uint8_t membershipReq[] = {0x01U, 0x59U, 0x06U, 0x22U, 0x22U};
  responseLength = 0U;
  ok = ok && device.handleZclRequest(kZigbeeClusterScenes, membershipReq,
                                     sizeof(membershipReq), response,
                                     &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x06U;
  static const uint8_t kMembershipPrefix[] = {0x00U, 0x07U, 0x22U, 0x22U, 0x01U,
                                              0x07U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kMembershipPrefix,
                                  sizeof(kMembershipPrefix));

  reportResult("Identify/Groups/Scenes", ok, "identify+groups+scenes");
  return ok;
}

static bool testTemperatureSensorResponses() {
  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-TEMP";
  basic.swBuildId = "0.1.0";
  basic.powerSource = 0x03U;

  ZigbeeHomeAutomationDevice device;
  bool ok =
      device.configureTemperatureSensor(1U, 0x00124B0001ABCD01ULL, 0x2233U,
                                        0x1A2BU, basic, 0x0000U) &&
      device.setBatteryStatus(29U, 188U) &&
      device.setTemperatureState(2315, -4000, 12500, 35U);

  const uint8_t readTempReq[] = {0x00U, 0x52U, 0x00U, 0x00U, 0x03U, 0x00U};
  uint8_t response[127] = {0};
  uint8_t responseLength = 0U;
  ZigbeeZclFrame parsed{};
  ok = ok &&
       device.handleZclRequest(kZigbeeClusterTemperatureMeasurement, readTempReq,
                               sizeof(readTempReq), response, &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x01U;
  static const uint8_t kMeasuredValue[] = {0x00U, 0x00U, 0x00U, 0x29U, 0x0BU, 0x09U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kMeasuredValue, sizeof(kMeasuredValue));

  const uint8_t readBatteryReq[] = {0x00U, 0x53U, 0x20U, 0x00U, 0x21U, 0x00U};
  responseLength = 0U;
  ok = ok &&
       device.handleZclRequest(kZigbeeClusterPowerConfiguration, readBatteryReq,
                               sizeof(readBatteryReq), response, &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid;
  static const uint8_t kBatteryVoltage[] = {0x20U, 0x00U, 0x00U, 0x20U, 29U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kBatteryVoltage, sizeof(kBatteryVoltage));

  reportResult("Temp Sensor", ok, "temp+battery");
  return ok;
}

static bool testZclClientCodec() {
  uint8_t encoded[127] = {0};
  uint8_t encodedLength = 0U;
  const uint16_t attributeIds[] = {0x0004U, 0x0005U, 0x0007U};
  bool ok = ZigbeeCodec::buildReadAttributesRequest(
      attributeIds,
      static_cast<uint8_t>(sizeof(attributeIds) / sizeof(attributeIds[0])),
      0x71U, encoded, &encodedLength);

  ZigbeeZclFrame request{};
  ok = ok && ZigbeeCodec::parseZclFrame(encoded, encodedLength, &request) &&
       request.valid && request.commandId == 0x00U &&
       request.transactionSequence == 0x71U &&
       request.payloadLength == 6U;

  ZigbeeReadAttributeRecord records[2];
  records[0].attributeId = 0x0004U;
  records[0].status = 0x00U;
  records[0].value.type = ZigbeeZclDataType::kCharString;
  records[0].value.stringValue = "CleanCore";
  records[0].value.stringLength = 9U;
  records[1].attributeId = 0x0007U;
  records[1].status = 0x00U;
  records[1].value.type = ZigbeeZclDataType::kUint8;
  records[1].value.data.u8 = 0x01U;

  ok = ok && ZigbeeCodec::buildReadAttributesResponse(records, 2U, 0x72U,
                                                      encoded, &encodedLength);
  ZigbeeZclFrame response{};
  ok = ok && ZigbeeCodec::parseZclFrame(encoded, encodedLength, &response) &&
       response.valid && response.commandId == 0x01U &&
       response.transactionSequence == 0x72U;

  ZigbeeReadAttributeRecord parsedRecords[4];
  uint8_t parsedRecordCount = 0U;
  ok = ok && ZigbeeCodec::parseReadAttributesResponse(
                   response.payload, response.payloadLength, parsedRecords, 4U,
                   &parsedRecordCount) &&
       parsedRecordCount == 2U && parsedRecords[0].attributeId == 0x0004U &&
       parsedRecords[0].value.type == ZigbeeZclDataType::kCharString &&
       parsedRecords[0].value.stringLength == 9U &&
       parsedRecords[1].attributeId == 0x0007U &&
       parsedRecords[1].value.data.u8 == 0x01U;

  ZigbeeReportingConfiguration reporting{};
  reporting.used = true;
  reporting.clusterId = kZigbeeClusterOnOff;
  reporting.attributeId = 0x0000U;
  reporting.dataType = ZigbeeZclDataType::kBoolean;
  reporting.minimumIntervalSeconds = 0U;
  reporting.maximumIntervalSeconds = 30U;
  reporting.reportableChange = 0U;
  ok = ok && ZigbeeCodec::buildConfigureReportingRequest(
                   &reporting, 1U, 0x73U, encoded, &encodedLength) &&
       ZigbeeCodec::parseZclFrame(encoded, encodedLength, &request) &&
       request.valid && request.commandId == 0x06U &&
       request.transactionSequence == 0x73U;

  ZigbeeAttributeReportRecord reportRecord{};
  reportRecord.attributeId = 0x0000U;
  reportRecord.value.type = ZigbeeZclDataType::kBoolean;
  reportRecord.value.data.boolValue = true;
  ok = ok && ZigbeeCodec::buildAttributeReport(&reportRecord, 1U, 0x74U,
                                               encoded, &encodedLength) &&
       ZigbeeCodec::parseZclFrame(encoded, encodedLength, &response) &&
       response.valid && response.commandId == 0x0AU &&
       response.transactionSequence == 0x74U;

  ZigbeeAttributeReportRecord parsedReports[2];
  uint8_t parsedReportCount = 0U;
  ok = ok && ZigbeeCodec::parseAttributeReport(
                   response.payload, response.payloadLength, parsedReports, 2U,
                   &parsedReportCount) &&
       parsedReportCount == 1U && parsedReports[0].attributeId == 0x0000U &&
       parsedReports[0].value.type == ZigbeeZclDataType::kBoolean &&
       parsedReports[0].value.data.boolValue;

  reportResult("ZCL Client Codec", ok, "read+cfg+report");
  return ok;
}

static bool testReportingFlow() {
  ZigbeeBasicClusterConfig basic{};
  basic.manufacturerName = "CleanCore";
  basic.modelIdentifier = "X54-LIGHT";
  basic.swBuildId = "0.1.0";
  basic.powerSource = 0x01U;

  ZigbeeHomeAutomationDevice device;
  bool ok = device.configureOnOffLight(1U, 0x00124B0001ABCDEFULL, 0x1234U,
                                       0x1A2BU, basic, 0x0000U);

  const uint8_t configureReq[] = {
      0x00U, 0x61U, 0x00U, 0x00U, 0x00U, 0x10U, 0x00U, 0x00U, 0x1EU, 0x00U};
  uint8_t response[127] = {0};
  uint8_t responseLength = 0U;
  ZigbeeZclFrame parsed{};
  ok = ok &&
       device.handleZclRequest(kZigbeeClusterOnOff, configureReq,
                               sizeof(configureReq), response, &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x07U &&
       parsed.payloadLength == 1U && parsed.payload[0] == 0x00U &&
       device.reportingConfigurationCount() == 1U;

  device.setOnOff(true);
  responseLength = 0U;
  ok = ok && device.buildAttributeReport(kZigbeeClusterOnOff, 0x62U, response,
                                         &responseLength) &&
       ZigbeeCodec::parseZclFrame(response, responseLength, &parsed) &&
       parsed.valid && parsed.commandId == 0x0AU;
  static const uint8_t kOnReport[] = {0x00U, 0x00U, 0x10U, 0x01U};
  ok = ok && containsByteSequence(parsed.payload, parsed.payloadLength,
                                  kOnReport, sizeof(kOnReport));

  reportResult("Reporting", ok, "configure+report");
  return ok;
}

static bool testPersistenceStore() {
  ZigbeePersistentStateStore store;
  bool ok = store.begin("zbstack");

  ZigbeePersistentState state{};
  ZigbeePersistentStateStore::initialize(&state);
  state.channel = 15U;
  state.panId = 0x1A2BU;
  state.nwkAddress = 0x3344U;
  state.ieeeAddress = 0x00124B0001ABCDEFULL;
  state.onOffState = true;
  state.levelState = 77U;
  state.incomingNwkFrameCounter = 0x01020304UL;
  state.reportingCount = 1U;
  state.reporting[0].used = true;
  state.reporting[0].clusterId = kZigbeeClusterOnOff;
  state.reporting[0].attributeId = 0x0000U;
  state.reporting[0].dataType = ZigbeeZclDataType::kBoolean;
  state.reporting[0].maximumIntervalSeconds = 30U;
  state.bindingCount = 1U;
  state.bindings[0].used = true;
  state.bindings[0].sourceEndpoint = 1U;
  state.bindings[0].clusterId = kZigbeeClusterOnOff;
  state.bindings[0].destinationAddressMode =
      ZigbeeBindingAddressMode::kExtended;
  state.bindings[0].destinationIeee = 0x00124B000054A11FULL;
  state.bindings[0].destinationEndpoint = 2U;

  ok = ok && store.save(state);

  ZigbeePersistentState loaded{};
  ok = ok && store.load(&loaded) && loaded.panId == state.panId &&
       loaded.nwkAddress == state.nwkAddress &&
       loaded.ieeeAddress == state.ieeeAddress &&
       loaded.onOffState == state.onOffState &&
       loaded.levelState == state.levelState &&
       loaded.incomingNwkFrameCounter == state.incomingNwkFrameCounter &&
       loaded.reportingCount == 1U &&
       loaded.reporting[0].clusterId == kZigbeeClusterOnOff &&
       loaded.bindingCount == 1U &&
       loaded.bindings[0].clusterId == kZigbeeClusterOnOff &&
       loaded.bindings[0].destinationIeee == 0x00124B000054A11FULL &&
       loaded.bindings[0].destinationEndpoint == 2U;

  ok = ok && store.clear();
  store.end();

  reportResult("Persistence", ok, "save+load+clear");
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.print("\r\nZigbeeStackCodecSelfTest\r\n");

  testNwkCodec();
  testApsCodec();
  testApsCommandCodec();
  testMacCodec();
  testBeaconAndZdoClientCodec();
  testNwkSecurityCodec();
  testZdoBindingCodecAndFlow();
  testZdoDescriptors();
  testOnOffLightResponses();
  testDimmableLightResponses();
  testIdentifyGroupsScenesFlow();
  testTemperatureSensorResponses();
  testZclClientCodec();
  testReportingFlow();
  testPersistenceStore();

  char summary[64];
  snprintf(summary, sizeof(summary), "SUMMARY: %lu/%lu PASS\r\n",
           static_cast<unsigned long>(g_passCount),
           static_cast<unsigned long>(g_totalCount));
  Serial.print(summary);
}

void loop() {
  static uint32_t lastMs = 0U;
  const uint32_t now = millis();
  if ((now - lastMs) < 2000U) {
    return;
  }
  lastMs = now;
  Serial.print("alive\r\n");
}
