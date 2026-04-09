#include "nrf54l15_vpr.h"

#include <string.h>

#include "vpr_cs_transport_stub_firmware.h"

namespace xiao_nrf54l15 {
namespace {

constexpr uint32_t kCtrlApIntRxReady = (1UL << 0U);
constexpr uint32_t kCtrlApIntTxDone = (1UL << 1U);
constexpr uint32_t kCpuSystemCacheBase = 0xE0082000UL;
constexpr uint32_t kMpc00SecureBase = 0x50041000UL;
constexpr uint32_t kSpu00SecureBase = 0x50040000UL;
constexpr uint32_t kVprSecureBase = 0x5004C000UL;
constexpr uint32_t kVprNonSecureBase = 0x4004C000UL;
constexpr uint8_t kVprMinTrigger = 16U;
constexpr uint8_t kVprMaxTrigger = 22U;
constexpr uint8_t kTransportTask = NRF54L15_VPR_TRANSPORT_HOST_TO_VPR_TASK;
constexpr uint8_t kTransportEvent = NRF54L15_VPR_TRANSPORT_VPR_TO_HOST_EVENT;
constexpr uint32_t kVprNordicCtrlOffset = VPRCSR_NORDIC_VPRNORDICCTRL;
constexpr uint32_t kVprNordicAxCacheOffset = VPRCSR_NORDIC_CACHE_AXCACHE;
constexpr uint32_t kVprNordicCacheCtrlOffset = VPRCSR_NORDIC_CACHE_CTRL;
constexpr uint32_t kVprNordicTasksOffset = VPRCSR_NORDIC_TASKS;
constexpr uint32_t kVprNordicEventsOffset = VPRCSR_NORDIC_EVENTSB;
constexpr uint32_t kVprNordicEventsStatusOffset = VPRCSR_NORDIC_EVENTSBS;
constexpr uint32_t kAddressSlavePos = 12U;
constexpr uint32_t kAddressSlaveMask = (0x3FUL << kAddressSlavePos);
constexpr uint32_t kMpcOverrideGranularity = 0x1000UL;
constexpr uint8_t kMpcTransportHostOverrideIndex = 0U;
constexpr uint8_t kMpcTransportVprOverrideIndex = 1U;

bool g_vprSecureAccessEnabled = false;

inline void dataSyncBarrier() {
  __DMB();
  __DSB();
}

inline void fullSyncBarrier() {
  __DMB();
  __DSB();
  __ISB();
}

inline volatile uint8_t* vprImageBase() {
  return reinterpret_cast<volatile uint8_t*>(static_cast<uintptr_t>(NRF54L15_VPR_IMAGE_BASE));
}

inline uint32_t currentVprBase() {
  return g_vprSecureAccessEnabled ? kVprSecureBase : kVprNonSecureBase;
}

inline volatile uint32_t& vprCsr(uint32_t offset) {
  return nrf54l15::reg32(currentVprBase() + offset);
}

inline NRF_CACHE_Type* cpuSystemCache() {
  return reinterpret_cast<NRF_CACHE_Type*>(static_cast<uintptr_t>(kCpuSystemCacheBase));
}

inline void invalidateCpuSystemCache() {
  NRF_CACHE_Type* cache = cpuSystemCache();
  cache->TASKS_INVALIDATECACHE = CACHE_TASKS_INVALIDATECACHE_TASKS_INVALIDATECACHE_Trigger;
  dataSyncBarrier();
  uint32_t spinLimit = 100000U;
  while (((cache->STATUS & CACHE_STATUS_READY_Msk) == CACHE_STATUS_READY_Busy) &&
         (spinLimit-- > 0U)) {
    __NOP();
  }
  fullSyncBarrier();
}

inline NRF_SPU_Type* spu00() {
  return reinterpret_cast<NRF_SPU_Type*>(static_cast<uintptr_t>(kSpu00SecureBase));
}

inline NRF_MPC_Type* mpc00() {
  return reinterpret_cast<NRF_MPC_Type*>(static_cast<uintptr_t>(kMpc00SecureBase));
}

inline uint8_t vprSlaveIndex() {
  return static_cast<uint8_t>((kVprSecureBase & kAddressSlaveMask) >> kAddressSlavePos);
}

inline uint32_t vprPermValue() {
  return spu00()->PERIPH[vprSlaveIndex()].PERM;
}

inline bool configureSecureVprPeripheralAccess() {
  NRF_SPU_Type* periphSpu = spu00();
  const uint8_t index = vprSlaveIndex();
  uint32_t perm = periphSpu->PERIPH[index].PERM;
  const uint32_t secureMapping =
      (perm & SPU_PERIPH_PERM_SECUREMAPPING_Msk) >> SPU_PERIPH_PERM_SECUREMAPPING_Pos;
  if (secureMapping == SPU_PERIPH_PERM_SECUREMAPPING_NonSecure) {
    g_vprSecureAccessEnabled = false;
    return false;
  }

  if (secureMapping == SPU_PERIPH_PERM_SECUREMAPPING_UserSelectable ||
      secureMapping == SPU_PERIPH_PERM_SECUREMAPPING_Split) {
    perm = (perm & ~SPU_PERIPH_PERM_SECATTR_Msk) |
           (SPU_PERIPH_PERM_SECATTR_Secure << SPU_PERIPH_PERM_SECATTR_Pos);

    const uint32_t dmaCapability =
        (perm & SPU_PERIPH_PERM_DMA_Msk) >> SPU_PERIPH_PERM_DMA_Pos;
    if (dmaCapability == SPU_PERIPH_PERM_DMA_SeparateAttribute) {
      perm = (perm & ~SPU_PERIPH_PERM_DMASEC_Msk) |
             (SPU_PERIPH_PERM_DMASEC_Secure << SPU_PERIPH_PERM_DMASEC_Pos);
    }

    periphSpu->PERIPH[index].PERM = perm;
    dataSyncBarrier();
    perm = periphSpu->PERIPH[index].PERM;
  }

  g_vprSecureAccessEnabled =
      (((perm & SPU_PERIPH_PERM_SECATTR_Msk) >> SPU_PERIPH_PERM_SECATTR_Pos) ==
       SPU_PERIPH_PERM_SECATTR_Secure);
  fullSyncBarrier();
  return g_vprSecureAccessEnabled;
}

inline void clearMpcMemAccErrInternal() {
  NRF_MPC_Type* mpc = mpc00();
  mpc->EVENTS_MEMACCERR = 0U;
  dataSyncBarrier();
}

inline void configureMpcOverrideRegion(uint8_t index, uint32_t address) {
  if (index > 4U) {
    return;
  }
  NRF_MPC_Type* mpc = mpc00();
  volatile NRF_MPC_OVERRIDE_Type* region = &mpc->OVERRIDE[index];
  const uint32_t alignedAddress = address & ~(kMpcOverrideGranularity - 1U);
  region->CONFIG = 0U;
  region->STARTADDR = alignedAddress;
  region->ENDADDR = alignedAddress;
  region->PERM = (MPC_OVERRIDE_PERM_READ_Allowed << MPC_OVERRIDE_PERM_READ_Pos) |
                 (MPC_OVERRIDE_PERM_WRITE_Allowed << MPC_OVERRIDE_PERM_WRITE_Pos) |
                 (MPC_OVERRIDE_PERM_SECATTR_NonSecure << MPC_OVERRIDE_PERM_SECATTR_Pos);
  region->PERMMASK =
      (MPC_OVERRIDE_PERMMASK_READ_UnMasked << MPC_OVERRIDE_PERMMASK_READ_Pos) |
      (MPC_OVERRIDE_PERMMASK_WRITE_UnMasked << MPC_OVERRIDE_PERMMASK_WRITE_Pos) |
      (MPC_OVERRIDE_PERMMASK_SECATTR_UnMasked << MPC_OVERRIDE_PERMMASK_SECATTR_Pos);
  region->CONFIG = (MPC_OVERRIDE_CONFIG_ENABLE_Enabled << MPC_OVERRIDE_CONFIG_ENABLE_Pos);
}

inline void configureTransportMpcWindows() {
  clearMpcMemAccErrInternal();
  configureMpcOverrideRegion(kMpcTransportHostOverrideIndex,
                             NRF54L15_VPR_TRANSPORT_HOST_BASE);
  configureMpcOverrideRegion(kMpcTransportVprOverrideIndex,
                             NRF54L15_VPR_TRANSPORT_VPR_BASE);
  fullSyncBarrier();
}

}  // namespace

NRF_CTRLAPPERI_Type* CtrlApMailbox::regs() {
  return reinterpret_cast<NRF_CTRLAPPERI_Type*>(static_cast<uintptr_t>(nrf54l15::CTRLAPPERI_BASE));
}

void CtrlApMailbox::clearEvents() {
  NRF_CTRLAPPERI_Type* periph = regs();
  periph->EVENTS_RXREADY = 0U;
  periph->EVENTS_TXDONE = 0U;
  dataSyncBarrier();
}

bool CtrlApMailbox::pollRxReady(bool clearEvent) {
  NRF_CTRLAPPERI_Type* periph = regs();
  if (periph->EVENTS_RXREADY == 0U) {
    return false;
  }
  if (clearEvent) {
    periph->EVENTS_RXREADY = 0U;
    dataSyncBarrier();
  }
  return true;
}

bool CtrlApMailbox::pollTxDone(bool clearEvent) {
  NRF_CTRLAPPERI_Type* periph = regs();
  if (periph->EVENTS_TXDONE == 0U) {
    return false;
  }
  if (clearEvent) {
    periph->EVENTS_TXDONE = 0U;
    dataSyncBarrier();
  }
  return true;
}

bool CtrlApMailbox::rxPending() {
  return (regs()->MAILBOX.RXSTATUS & CTRLAPPERI_MAILBOX_RXSTATUS_RXSTATUS_Msk) != 0U;
}

bool CtrlApMailbox::txPending() {
  return (regs()->MAILBOX.TXSTATUS & CTRLAPPERI_MAILBOX_TXSTATUS_TXSTATUS_Msk) != 0U;
}

bool CtrlApMailbox::read(uint32_t* value) {
  if (value == nullptr || !rxPending()) {
    return false;
  }
  *value = regs()->MAILBOX.RXDATA;
  dataSyncBarrier();
  return true;
}

bool CtrlApMailbox::write(uint32_t value) {
  if (txPending()) {
    return false;
  }
  regs()->MAILBOX.TXDATA = value;
  dataSyncBarrier();
  return true;
}

void CtrlApMailbox::enableInterrupts(bool rxReady, bool txDone) {
  NRF_CTRLAPPERI_Type* periph = regs();
  const uint32_t mask = (rxReady ? kCtrlApIntRxReady : 0U) | (txDone ? kCtrlApIntTxDone : 0U);
  periph->INTENCLR = kCtrlApIntRxReady | kCtrlApIntTxDone;
  if (mask != 0U) {
    periph->INTENSET = mask;
  }
  dataSyncBarrier();
}

NRF_VPR_Type* VprControl::regs() {
  return reinterpret_cast<NRF_VPR_Type*>(static_cast<uintptr_t>(currentVprBase()));
}

bool VprControl::validTriggerIndex(uint8_t index) {
  return index >= kVprMinTrigger && index <= kVprMaxTrigger;
}

bool VprControl::setInitPc(uint32_t address) {
  if (address < NRF54L15_VPR_IMAGE_BASE ||
      address >= (NRF54L15_VPR_IMAGE_BASE + NRF54L15_VPR_IMAGE_SIZE)) {
    return false;
  }
  regs()->INITPC = address;
  fullSyncBarrier();
  return true;
}

uint32_t VprControl::initPc() { return regs()->INITPC; }

void VprControl::prepareForLaunch() {
  (void)configureSecureVprPeripheralAccess();
  stop();
  fullSyncBarrier();
}

bool VprControl::start(uint32_t address) {
  if (!configureSecureVprPeripheralAccess()) {
    return false;
  }
  stop();
  fullSyncBarrier();
  enableRtPeripherals();
  if (!setInitPc(address)) {
    return false;
  }
  regs()->CPURUN = VPR_CPURUN_EN_Running;
  fullSyncBarrier();
  return isRunning();
}

void VprControl::stop() {
  regs()->CPURUN = VPR_CPURUN_EN_Stopped;
  fullSyncBarrier();
}

bool VprControl::isRunning() {
  return (regs()->CPURUN & VPR_CPURUN_EN_Msk) != 0U;
}

bool VprControl::secureAccessEnabled() { return g_vprSecureAccessEnabled; }

uint32_t VprControl::spuPerm() { return vprPermValue(); }

void VprControl::enableRtPeripherals() {
  const uint32_t value =
      (VPRCSR_NORDIC_VPRNORDICCTRL_NORDICKEY_Enabled
       << VPRCSR_NORDIC_VPRNORDICCTRL_NORDICKEY_Pos) |
      VPRCSR_NORDIC_VPRNORDICCTRL_ENABLERTPERIPH_Msk;
  vprCsr(kVprNordicCtrlOffset) = value;
  fullSyncBarrier();
}

void VprControl::configureDataAccessNonCacheable() {
  uint32_t axcache = vprCsr(kVprNordicAxCacheOffset);
  axcache &= ~(VPRCSR_NORDIC_CACHE_AXCACHE_AWCACHE_Msk |
               VPRCSR_NORDIC_CACHE_AXCACHE_DARCACHE_Msk);
  axcache |=
      (VPRCSR_NORDIC_CACHE_AXCACHE_AWCACHE_NNONCACHENONBUFF
       << VPRCSR_NORDIC_CACHE_AXCACHE_AWCACHE_Pos) |
      (VPRCSR_NORDIC_CACHE_AXCACHE_DARCACHE_NNONCACHENONBUFF
       << VPRCSR_NORDIC_CACHE_AXCACHE_DARCACHE_Pos);
  vprCsr(kVprNordicAxCacheOffset) = axcache;
  fullSyncBarrier();
}

void VprControl::clearCache() {
  vprCsr(kVprNordicCacheCtrlOffset) =
      VPRCSR_NORDIC_CACHE_CTRL_ENABLE_Msk | VPRCSR_NORDIC_CACHE_CTRL_CACHECLR_Msk;
  fullSyncBarrier();
  vprCsr(kVprNordicCacheCtrlOffset) = VPRCSR_NORDIC_CACHE_CTRL_ENABLE_Msk;
  fullSyncBarrier();
}

void VprControl::clearBufferedSignals() {
  vprCsr(kVprNordicTasksOffset) = 0U;
  vprCsr(kVprNordicEventsOffset) = 0U;
  fullSyncBarrier();
  for (uint8_t index = kVprMinTrigger; index <= kVprMaxTrigger; ++index) {
    regs()->EVENTS_TRIGGERED[index] = 0U;
  }
  dataSyncBarrier();
}

void VprControl::clearDebugHaltState() {
  regs()->DEBUGIF.DMCONTROL =
      (VPR_DEBUGIF_DMCONTROL_DMACTIVE_Enabled << VPR_DEBUGIF_DMCONTROL_DMACTIVE_Pos) |
      (VPR_DEBUGIF_DMCONTROL_ACKHAVERESET_Clear << VPR_DEBUGIF_DMCONTROL_ACKHAVERESET_Pos) |
      (VPR_DEBUGIF_DMCONTROL_CLRRESETHALTREQ_Clear
       << VPR_DEBUGIF_DMCONTROL_CLRRESETHALTREQ_Pos) |
      (VPR_DEBUGIF_DMCONTROL_RESUMEREQ_Resumed << VPR_DEBUGIF_DMCONTROL_RESUMEREQ_Pos);
  fullSyncBarrier();
  regs()->DEBUGIF.DMCONTROL =
      (VPR_DEBUGIF_DMCONTROL_DMACTIVE_Enabled << VPR_DEBUGIF_DMCONTROL_DMACTIVE_Pos);
  fullSyncBarrier();
}

uint32_t VprControl::debugStatus() { return regs()->DEBUGIF.DMSTATUS; }

uint32_t VprControl::haltSummary0() { return regs()->DEBUGIF.HALTSUM0; }

uint32_t VprControl::haltSummary1() { return regs()->DEBUGIF.HALTSUM1; }

uint32_t VprControl::rawNordicAxCache() { return vprCsr(kVprNordicAxCacheOffset); }

uint32_t VprControl::rawNordicTasks() { return vprCsr(kVprNordicTasksOffset); }

uint32_t VprControl::rawNordicEvents() { return vprCsr(kVprNordicEventsOffset); }

uint32_t VprControl::rawNordicEventStatus() { return vprCsr(kVprNordicEventsStatusOffset); }

uint32_t VprControl::rawNordicCacheCtrl() { return vprCsr(kVprNordicCacheCtrlOffset); }

uint32_t VprControl::rawMpcMemAccErrEvent() { return mpc00()->EVENTS_MEMACCERR; }

uint32_t VprControl::rawMpcMemAccErrAddress() { return mpc00()->MEMACCERR.ADDRESS; }

uint32_t VprControl::rawMpcMemAccErrInfo() { return mpc00()->MEMACCERR.INFO; }

void VprControl::clearMpcMemAccErr() { clearMpcMemAccErrInternal(); }

bool VprControl::triggerTask(uint8_t index) {
  if (!validTriggerIndex(index)) {
    return false;
  }
  regs()->TASKS_TRIGGER[index] = VPR_TASKS_TRIGGER_TASKS_TRIGGER_Trigger;
  dataSyncBarrier();
  return true;
}

bool VprControl::pollEvent(uint8_t index, bool clearEvent) {
  if (!validTriggerIndex(index) || regs()->EVENTS_TRIGGERED[index] == 0U) {
    return false;
  }
  if (clearEvent) {
    regs()->EVENTS_TRIGGERED[index] = 0U;
    dataSyncBarrier();
  }
  return true;
}

bool VprControl::clearEvent(uint8_t index) {
  if (!validTriggerIndex(index)) {
    return false;
  }
  regs()->EVENTS_TRIGGERED[index] = 0U;
  dataSyncBarrier();
  return true;
}

bool VprControl::enableEventInterrupt(uint8_t index, bool enable) {
  if (!validTriggerIndex(index)) {
    return false;
  }
  const uint32_t mask = (1UL << index);
  if (enable) {
    regs()->INTENSET = mask;
  } else {
    regs()->INTENCLR = mask;
  }
  dataSyncBarrier();
  return true;
}

VprSharedTransportStream::VprSharedTransportStream() : rxBuffer_{0}, rxLen_(0U), rxIndex_(0U) {}

volatile Nrf54l15VprTransportHostShared* VprSharedTransportStream::hostShared() const {
  return nrf54l15_vpr_transport_host_shared();
}

volatile Nrf54l15VprTransportVprShared* VprSharedTransportStream::vprShared() const {
  return nrf54l15_vpr_transport_vpr_shared();
}

bool VprSharedTransportStream::resetSharedState(bool clearScripts) {
  volatile Nrf54l15VprTransportHostShared* host = hostShared();
  volatile Nrf54l15VprTransportVprShared* vpr = vprShared();
  const size_t hostPrefixLen = offsetof(Nrf54l15VprTransportHostShared, scripts);
  memset(reinterpret_cast<void*>(const_cast<Nrf54l15VprTransportHostShared*>(host)), 0,
         hostPrefixLen);
  if (clearScripts) {
    memset(reinterpret_cast<void*>(&const_cast<Nrf54l15VprTransportHostShared*>(host)->scripts[0]),
           0, sizeof(host->scripts));
  }
  memset(reinterpret_cast<void*>(const_cast<Nrf54l15VprTransportVprShared*>(vpr)), 0,
         sizeof(*vpr));
  host->magic = NRF54L15_VPR_TRANSPORT_MAGIC;
  host->version = NRF54L15_VPR_TRANSPORT_VERSION;
  vpr->magic = NRF54L15_VPR_TRANSPORT_MAGIC;
  vpr->version = NRF54L15_VPR_TRANSPORT_VERSION;
  vpr->status = NRF54L15_VPR_TRANSPORT_STATUS_STOPPED;
  dataSyncBarrier();
  rxLen_ = 0U;
  rxIndex_ = 0U;
  return true;
}

bool VprSharedTransportStream::clearScripts() {
  volatile Nrf54l15VprTransportHostShared* host = hostShared();
  memset(reinterpret_cast<void*>(&const_cast<Nrf54l15VprTransportHostShared*>(host)->scripts[0]), 0,
         sizeof(host->scripts));
  host->scriptCount = 0U;
  dataSyncBarrier();
  return true;
}

bool VprSharedTransportStream::addScriptResponse(uint16_t opcode,
                                                 const uint8_t* response,
                                                 size_t len) {
  if (response == nullptr || len == 0U || len > NRF54L15_VPR_TRANSPORT_MAX_SCRIPT_RESPONSE) {
    return false;
  }
  volatile Nrf54l15VprTransportHostShared* host = hostShared();
  if (host->scriptCount >= NRF54L15_VPR_TRANSPORT_MAX_SCRIPT_COUNT) {
    return false;
  }
  Nrf54l15VprTransportScript* script =
      &const_cast<Nrf54l15VprTransportHostShared*>(host)->scripts[host->scriptCount];
  memset(script, 0, sizeof(*script));
  script->opcode = opcode;
  script->responseLen = static_cast<uint16_t>(len);
  memcpy(script->response, response, len);
  ++host->scriptCount;
  dataSyncBarrier();
  return true;
}

bool VprSharedTransportStream::loadFirmware(const uint8_t* image, size_t len) {
  if (image == nullptr || len == 0U || len > NRF54L15_VPR_IMAGE_SIZE) {
    return false;
  }
  VprControl::stop();
  memset(reinterpret_cast<void*>(const_cast<uint8_t*>(vprImageBase())), 0, NRF54L15_VPR_IMAGE_SIZE);
  memcpy(reinterpret_cast<void*>(const_cast<uint8_t*>(vprImageBase())), image, len);
  fullSyncBarrier();
  invalidateCpuSystemCache();
  return true;
}

bool VprSharedTransportStream::bootLoadedFirmware() {
  configureTransportMpcWindows();
  vprShared()->status = NRF54L15_VPR_TRANSPORT_STATUS_BOOTING;
  dataSyncBarrier();
  if (!VprControl::start(NRF54L15_VPR_IMAGE_BASE)) {
    return false;
  }
  return true;
}

bool VprSharedTransportStream::loadFirmwareAndStart(const uint8_t* image, size_t len) {
  if (!loadFirmware(image, len) || !bootLoadedFirmware()) {
    return false;
  }
  return waitReady();
}

bool VprSharedTransportStream::loadDefaultCsTransportStubImage() {
  return loadFirmware(kVprCsTransportStubFirmware, kVprCsTransportStubFirmwareSize);
}

bool VprSharedTransportStream::loadDefaultCsTransportStub() {
  return loadDefaultCsTransportStubImage() && bootLoadedFirmware() && waitReady();
}

void VprSharedTransportStream::stop() {
  VprControl::stop();
  vprShared()->status = NRF54L15_VPR_TRANSPORT_STATUS_STOPPED;
  dataSyncBarrier();
  rxLen_ = 0U;
  rxIndex_ = 0U;
}

bool VprSharedTransportStream::waitReady(uint32_t spinLimit) {
  volatile Nrf54l15VprTransportVprShared* state = vprShared();
  uint32_t pollCount = 0U;
  invalidateCpuSystemCache();
  while (spinLimit-- > 0U) {
    (void)VprControl::pollEvent(kTransportEvent);
    if ((pollCount++ & 0x3FFFU) == 0U) {
      invalidateCpuSystemCache();
    }
    if (state->magic == NRF54L15_VPR_TRANSPORT_MAGIC &&
        state->version == NRF54L15_VPR_TRANSPORT_VERSION &&
        state->status == NRF54L15_VPR_TRANSPORT_STATUS_READY) {
      return true;
    }
    if (state->status == NRF54L15_VPR_TRANSPORT_STATUS_ERROR || !VprControl::isRunning()) {
      return false;
    }
  }
  return false;
}

bool VprSharedTransportStream::pullResponse() {
  volatile Nrf54l15VprTransportVprShared* state = vprShared();
  const bool eventReady = VprControl::pollEvent(kTransportEvent);
  if (!eventReady && (state->vprFlags & NRF54L15_VPR_TRANSPORT_FLAG_PENDING) == 0U) {
    return false;
  }
  if ((state->vprFlags & NRF54L15_VPR_TRANSPORT_FLAG_PENDING) == 0U) {
    return false;
  }
  const size_t copyLen = (state->vprLen <= sizeof(rxBuffer_)) ? state->vprLen : sizeof(rxBuffer_);
  memcpy(rxBuffer_, reinterpret_cast<const void*>(const_cast<const uint8_t*>(state->vprData)), copyLen);
  rxLen_ = copyLen;
  rxIndex_ = 0U;
  state->vprFlags = 0U;
  state->vprLen = 0U;
  dataSyncBarrier();
  return copyLen > 0U;
}

bool VprSharedTransportStream::poll() {
  if (rxIndex_ < rxLen_) {
    return true;
  }
  invalidateCpuSystemCache();
  return pullResponse();
}

uint32_t VprSharedTransportStream::heartbeat() const { return vprShared()->heartbeat; }

uint16_t VprSharedTransportStream::lastOpcode() const {
  return static_cast<uint16_t>(vprShared()->lastOpcode & 0xFFFFU);
}

uint32_t VprSharedTransportStream::transportStatus() const { return vprShared()->status; }

uint32_t VprSharedTransportStream::lastError() const { return vprShared()->lastError; }

uint32_t VprSharedTransportStream::initPc() const { return VprControl::initPc(); }

bool VprSharedTransportStream::isRunning() const { return VprControl::isRunning(); }

bool VprSharedTransportStream::secureAccessEnabled() const {
  return VprControl::secureAccessEnabled();
}

uint32_t VprSharedTransportStream::spuPerm() const { return VprControl::spuPerm(); }

uint32_t VprSharedTransportStream::debugStatus() const { return VprControl::debugStatus(); }

uint32_t VprSharedTransportStream::haltSummary0() const { return VprControl::haltSummary0(); }

uint32_t VprSharedTransportStream::haltSummary1() const { return VprControl::haltSummary1(); }

uint32_t VprSharedTransportStream::rawNordicTasks() const { return VprControl::rawNordicTasks(); }

uint32_t VprSharedTransportStream::rawNordicEvents() const { return VprControl::rawNordicEvents(); }

uint32_t VprSharedTransportStream::rawNordicEventStatus() const {
  return VprControl::rawNordicEventStatus();
}

uint32_t VprSharedTransportStream::rawNordicCacheCtrl() const {
  return VprControl::rawNordicCacheCtrl();
}

uint32_t VprSharedTransportStream::rawMpcMemAccErrEvent() const {
  return VprControl::rawMpcMemAccErrEvent();
}

uint32_t VprSharedTransportStream::rawMpcMemAccErrAddress() const {
  return VprControl::rawMpcMemAccErrAddress();
}

uint32_t VprSharedTransportStream::rawMpcMemAccErrInfo() const {
  return VprControl::rawMpcMemAccErrInfo();
}

int VprSharedTransportStream::available() {
  (void)poll();
  return static_cast<int>((rxIndex_ < rxLen_) ? (rxLen_ - rxIndex_) : 0U);
}

int VprSharedTransportStream::read() {
  if (available() <= 0) {
    return -1;
  }
  return rxBuffer_[rxIndex_++];
}

int VprSharedTransportStream::peek() {
  if (available() <= 0) {
    return -1;
  }
  return rxBuffer_[rxIndex_];
}

void VprSharedTransportStream::flush() {}

size_t VprSharedTransportStream::write(uint8_t byte) { return write(&byte, 1U); }

size_t VprSharedTransportStream::write(const uint8_t* buffer, size_t len) {
  if (buffer == nullptr || len == 0U || len > NRF54L15_VPR_TRANSPORT_MAX_HOST_DATA) {
    return 0U;
  }
  volatile Nrf54l15VprTransportHostShared* host = hostShared();
  volatile Nrf54l15VprTransportVprShared* vpr = vprShared();
  if (vpr->status != NRF54L15_VPR_TRANSPORT_STATUS_READY ||
      (host->hostFlags & NRF54L15_VPR_TRANSPORT_FLAG_PENDING) != 0U ||
      (vpr->vprFlags & NRF54L15_VPR_TRANSPORT_FLAG_PENDING) != 0U ||
      rxIndex_ < rxLen_) {
    return 0U;
  }
  (void)VprControl::clearEvent(kTransportEvent);
  memcpy(reinterpret_cast<void*>(const_cast<uint8_t*>(host->hostData)), buffer, len);
  host->hostLen = static_cast<uint32_t>(len);
  ++host->hostSeq;
  host->hostFlags = NRF54L15_VPR_TRANSPORT_FLAG_PENDING;
  dataSyncBarrier();
  if (!VprControl::triggerTask(kTransportTask)) {
    host->hostFlags = 0U;
    host->hostLen = 0U;
    dataSyncBarrier();
    return 0U;
  }
  return len;
}

}  // namespace xiao_nrf54l15
