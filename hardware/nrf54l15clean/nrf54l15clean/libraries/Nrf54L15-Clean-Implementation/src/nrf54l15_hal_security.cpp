#include "nrf54l15_hal.h"
#include <string.h>

namespace xiao_nrf54l15 {

namespace {

constexpr uint32_t kKmuSlotMax = KMU_KEYSLOT_ID_Max;
constexpr uint32_t kTampcWriteKey =
    (TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_KEY_KEY
     << TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_KEY_Pos);
constexpr uint32_t kTampcWriteProtectionClear =
    (TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_WRITEPROTECTION_Clear
     << TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_WRITEPROTECTION_Pos);
constexpr uint32_t kTampcLockEnabled =
    (TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_LOCK_Enabled
     << TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_LOCK_Pos);

inline bool validKmuSlot(uint8_t slot) {
  return slot <= kKmuSlotMax;
}

bool waitRramcReady(NRF_RRAMC_Type* rramc, uint32_t spinLimit) {
  if (rramc == nullptr) {
    return false;
  }
  while (spinLimit-- > 0U) {
    if (((rramc->READY & RRAMC_READY_READY_Msk) >> RRAMC_READY_READY_Pos) ==
        RRAMC_READY_READY_Ready) {
      return true;
    }
  }
  return false;
}

}  // namespace

Kmu::Kmu(uint32_t base, uint32_t rramcBase)
    : kmu_(reinterpret_cast<NRF_KMU_Type*>(base)),
      rramc_(reinterpret_cast<NRF_RRAMC_Type*>(rramcBase)) {}

bool Kmu::ready() const {
#if defined(NRF_TRUSTZONE_NONSECURE)
  return false;
#else
  if (kmu_ == nullptr) {
    return false;
  }
  return ((kmu_->STATUS & KMU_STATUS_STATUS_Msk) >> KMU_STATUS_STATUS_Pos) ==
         KMU_STATUS_STATUS_Ready;
#endif
}

void Kmu::clearEvents() {
  if (kmu_ == nullptr) {
    return;
  }
  kmu_->EVENTS_PROVISIONED = 0U;
  kmu_->EVENTS_PUSHED = 0U;
  kmu_->EVENTS_REVOKED = 0U;
  kmu_->EVENTS_ERROR = 0U;
  kmu_->EVENTS_METADATAREAD = 0U;
  kmu_->EVENTS_PUSHBLOCKED = 0U;
}

bool Kmu::pollProvisioned(bool clearEvent) {
  if (kmu_ == nullptr) {
    return false;
  }
  const bool signaled = ((kmu_->EVENTS_PROVISIONED &
                          KMU_EVENTS_PROVISIONED_EVENTS_PROVISIONED_Msk) >>
                         KMU_EVENTS_PROVISIONED_EVENTS_PROVISIONED_Pos) ==
                        KMU_EVENTS_PROVISIONED_EVENTS_PROVISIONED_Generated;
  if (signaled && clearEvent) {
    kmu_->EVENTS_PROVISIONED = 0U;
  }
  return signaled;
}

bool Kmu::pollPushed(bool clearEvent) {
  if (kmu_ == nullptr) {
    return false;
  }
  const bool signaled =
      ((kmu_->EVENTS_PUSHED & KMU_EVENTS_PUSHED_EVENTS_PUSHED_Msk) >>
       KMU_EVENTS_PUSHED_EVENTS_PUSHED_Pos) ==
      KMU_EVENTS_PUSHED_EVENTS_PUSHED_Generated;
  if (signaled && clearEvent) {
    kmu_->EVENTS_PUSHED = 0U;
  }
  return signaled;
}

bool Kmu::pollRevoked(bool clearEvent) {
  if (kmu_ == nullptr) {
    return false;
  }
  const bool signaled =
      ((kmu_->EVENTS_REVOKED & KMU_EVENTS_REVOKED_EVENTS_REVOKED_Msk) >>
       KMU_EVENTS_REVOKED_EVENTS_REVOKED_Pos) ==
      KMU_EVENTS_REVOKED_EVENTS_REVOKED_Generated;
  if (signaled && clearEvent) {
    kmu_->EVENTS_REVOKED = 0U;
  }
  return signaled;
}

bool Kmu::pollMetadataRead(bool clearEvent) {
  if (kmu_ == nullptr) {
    return false;
  }
  const bool signaled =
      ((kmu_->EVENTS_METADATAREAD &
        KMU_EVENTS_METADATAREAD_EVENTS_METADATAREAD_Msk) >>
       KMU_EVENTS_METADATAREAD_EVENTS_METADATAREAD_Pos) ==
      KMU_EVENTS_METADATAREAD_EVENTS_METADATAREAD_Generated;
  if (signaled && clearEvent) {
    kmu_->EVENTS_METADATAREAD = 0U;
  }
  return signaled;
}

bool Kmu::pollPushBlocked(bool clearEvent) {
  if (kmu_ == nullptr) {
    return false;
  }
  const bool signaled =
      ((kmu_->EVENTS_PUSHBLOCKED &
        KMU_EVENTS_PUSHBLOCKED_EVENTS_PUSHBLOCKED_Msk) >>
       KMU_EVENTS_PUSHBLOCKED_EVENTS_PUSHBLOCKED_Pos) ==
      KMU_EVENTS_PUSHBLOCKED_EVENTS_PUSHBLOCKED_Generated;
  if (signaled && clearEvent) {
    kmu_->EVENTS_PUSHBLOCKED = 0U;
  }
  return signaled;
}

bool Kmu::pollError(bool clearEvent) {
  if (kmu_ == nullptr) {
    return false;
  }
  const bool signaled =
      ((kmu_->EVENTS_ERROR & KMU_EVENTS_ERROR_EVENTS_ERROR_Msk) >>
       KMU_EVENTS_ERROR_EVENTS_ERROR_Pos) ==
      KMU_EVENTS_ERROR_EVENTS_ERROR_Generated;
  if (signaled && clearEvent) {
    kmu_->EVENTS_ERROR = 0U;
  }
  return signaled;
}

bool Kmu::waitReady(uint32_t spinLimit) const {
  if (kmu_ == nullptr) {
    return false;
  }
  while (spinLimit-- > 0U) {
    if (ready()) {
      return true;
    }
  }
  return false;
}

bool Kmu::enableRramWrite(uint32_t* previousConfig, uint32_t spinLimit) const {
  if (previousConfig == nullptr || rramc_ == nullptr) {
    return false;
  }
  if (!waitRramcReady(rramc_, spinLimit)) {
    return false;
  }
  *previousConfig = rramc_->CONFIG;
  rramc_->CONFIG = *previousConfig | RRAMC_CONFIG_WEN_Msk;
  return waitRramcReady(rramc_, spinLimit);
}

void Kmu::restoreRramWrite(uint32_t previousConfig, uint32_t spinLimit) const {
  if (rramc_ == nullptr) {
    return;
  }
  (void)waitRramcReady(rramc_, spinLimit);
  rramc_->CONFIG = previousConfig;
  (void)waitRramcReady(rramc_, spinLimit);
}

bool Kmu::performSimpleTask(uint8_t slot,
                            volatile uint32_t& task,
                            volatile uint32_t& successEvent,
                            uint32_t spinLimit) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)slot;
  (void)task;
  (void)successEvent;
  (void)spinLimit;
  return false;
#else
  if (kmu_ == nullptr || !validKmuSlot(slot) || !waitReady(spinLimit)) {
    return false;
  }

  clearEvents();
  kmu_->KEYSLOT = static_cast<uint32_t>(slot);
  successEvent = 0U;
  kmu_->EVENTS_ERROR = 0U;
  task = 1U;

  while (spinLimit-- > 0U) {
    if ((successEvent & 1U) != 0U) {
      return true;
    }
    if ((kmu_->EVENTS_ERROR & 1U) != 0U) {
      return false;
    }
  }
  return false;
#endif
}

bool Kmu::readMetadata(uint8_t slot, uint32_t* metadata, uint32_t spinLimit) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)slot;
  (void)metadata;
  (void)spinLimit;
  return false;
#else
  if (metadata == nullptr) {
    return false;
  }
  if (!performSimpleTask(slot, kmu_->TASKS_READMETADATA,
                         kmu_->EVENTS_METADATAREAD, spinLimit)) {
    return false;
  }
  *metadata = kmu_->METADATA;
  return true;
#endif
}

bool Kmu::provision(uint8_t slot, const KmuProvisionSource& source,
                    uint32_t spinLimit) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)slot;
  (void)source;
  (void)spinLimit;
  return false;
#else
  if (kmu_ == nullptr || !validKmuSlot(slot) || !waitReady(spinLimit)) {
    return false;
  }
  if ((reinterpret_cast<uintptr_t>(&source) & 0xFUL) != 0U ||
      (source.destination & 0xFUL) != 0U) {
    return false;
  }

  uint32_t previousConfig = 0U;
  if (!enableRramWrite(&previousConfig, spinLimit)) {
    return false;
  }

  clearEvents();
  kmu_->KEYSLOT = static_cast<uint32_t>(slot);
  kmu_->SRC = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&source));
  kmu_->TASKS_PROVISION = KMU_TASKS_PROVISION_TASKS_PROVISION_Trigger;

  bool ok = false;
  while (spinLimit-- > 0U) {
    if ((kmu_->EVENTS_PROVISIONED & 1U) != 0U) {
      ok = true;
      break;
    }
    if ((kmu_->EVENTS_ERROR & 1U) != 0U) {
      break;
    }
  }

  restoreRramWrite(previousConfig, 600000UL);
  return ok;
#endif
}

bool Kmu::push(uint8_t slot, uint32_t spinLimit) {
  return performSimpleTask(slot, kmu_->TASKS_PUSH, kmu_->EVENTS_PUSHED,
                           spinLimit);
}

bool Kmu::revoke(uint8_t slot, uint32_t spinLimit) {
  return performSimpleTask(slot, kmu_->TASKS_REVOKE, kmu_->EVENTS_REVOKED,
                           spinLimit);
}

bool Kmu::pushBlock(uint8_t slot, uint32_t spinLimit) {
  return performSimpleTask(slot, kmu_->TASKS_PUSHBLOCK,
                           kmu_->EVENTS_PUSHBLOCKED, spinLimit);
}

CracenIkg::CracenIkg(uint32_t controlBase, uint32_t coreBase)
    : cracen_(reinterpret_cast<NRF_CRACEN_Type*>(controlBase)),
      core_(reinterpret_cast<NRF_CRACENCORE_Type*>(coreBase)),
      active_(false) {}

bool CracenIkg::begin(uint32_t spinLimit) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)spinLimit;
  return false;
#else
  if (cracen_ == nullptr || core_ == nullptr) {
    return false;
  }

  cracen_->ENABLE |= CRACEN_ENABLE_PKEIKG_Msk | CRACEN_ENABLE_CRYPTOMASTER_Msk;
  active_ = true;
  clearEvent();
  return waitReady(spinLimit);
#endif
}

void CracenIkg::end() {
#if defined(NRF_TRUSTZONE_NONSECURE)
  active_ = false;
#else
  if (cracen_ == nullptr) {
    active_ = false;
    return;
  }
  cracen_->ENABLE &= ~(CRACEN_ENABLE_PKEIKG_Msk | CRACEN_ENABLE_CRYPTOMASTER_Msk);
  clearEvent();
  active_ = false;
#endif
}

bool CracenIkg::active() const { return active_; }

void CracenIkg::clearEvent() {
  if (cracen_ == nullptr) {
    return;
  }
  cracen_->EVENTS_PKEIKG = 0U;
}

uint32_t CracenIkg::status() const {
  return (core_ == nullptr) ? 0xFFFFFFFFUL : core_->IKG.STATUS;
}

uint32_t CracenIkg::pkeStatus() const {
  return (core_ == nullptr) ? 0xFFFFFFFFUL : core_->IKG.PKESTATUS;
}

uint32_t CracenIkg::hwConfig() const {
  return (core_ == nullptr) ? 0xFFFFFFFFUL : core_->IKG.HWCONFIG;
}

uint8_t CracenIkg::symmetricKeyCapacity() const {
  const uint32_t config = hwConfig();
  if (config == 0xFFFFFFFFUL) {
    return 0U;
  }
  return static_cast<uint8_t>(
      (config & CRACENCORE_IKG_HWCONFIG_NBSYMKEYS_Msk) >>
      CRACENCORE_IKG_HWCONFIG_NBSYMKEYS_Pos);
}

uint8_t CracenIkg::privateKeyCapacity() const {
  const uint32_t config = hwConfig();
  if (config == 0xFFFFFFFFUL) {
    return 0U;
  }
  return static_cast<uint8_t>(
      (config & CRACENCORE_IKG_HWCONFIG_NBPRIVKEYS_Msk) >>
      CRACENCORE_IKG_HWCONFIG_NBPRIVKEYS_Pos);
}

bool CracenIkg::okay() const {
  return (status() & CRACENCORE_IKG_STATUS_OKAY_Msk) != 0U;
}

bool CracenIkg::seedError() const {
  return (status() & CRACENCORE_IKG_STATUS_SEEDERROR_Msk) != 0U;
}

bool CracenIkg::entropyError() const {
  return (status() & CRACENCORE_IKG_STATUS_ENTROPYERROR_Msk) != 0U;
}

bool CracenIkg::catastrophicError() const {
  return (status() & CRACENCORE_IKG_STATUS_CATASTROPHICERROR_Msk) != 0U;
}

bool CracenIkg::ctrDrbgBusy() const {
  return (status() & CRACENCORE_IKG_STATUS_CTRDRBGBUSY_Msk) != 0U;
}

bool CracenIkg::symmetricKeysStored() const {
  return (status() & CRACENCORE_IKG_STATUS_SYMKEYSTORED_Msk) != 0U;
}

bool CracenIkg::privateKeysStored() const {
  return (status() & CRACENCORE_IKG_STATUS_PRIVKEYSTORED_Msk) != 0U;
}

bool CracenIkg::markSeedValid(bool valid) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)valid;
  return false;
#else
  if (cracen_ == nullptr) {
    return false;
  }
  cracen_->SEEDVALID = valid ? CRACEN_SEEDVALID_VALID_Enabled
                             : CRACEN_SEEDVALID_VALID_Disabled;
  return true;
#endif
}

bool CracenIkg::lockSeed() {
#if defined(NRF_TRUSTZONE_NONSECURE)
  return false;
#else
  if (cracen_ == nullptr) {
    return false;
  }
  cracen_->SEEDLOCK = CRACEN_SEEDLOCK_ENABLE_Enabled;
  return true;
#endif
}

bool CracenIkg::lockProtectedRam() {
#if defined(NRF_TRUSTZONE_NONSECURE)
  return false;
#else
  if (cracen_ == nullptr) {
    return false;
  }
  cracen_->PROTECTEDRAMLOCK = CRACEN_PROTECTEDRAMLOCK_ENABLE_Enabled;
  return true;
#endif
}

bool CracenIkg::softResetKeys(uint32_t spinLimit) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)spinLimit;
  return false;
#else
  if (core_ == nullptr) {
    return false;
  }
  core_->IKG.SOFTRST = CRACENCORE_IKG_SOFTRST_SOFTRST_KEY;
  core_->IKG.SOFTRST = CRACENCORE_IKG_SOFTRST_SOFTRST_NORMAL;
  return waitReady(spinLimit);
#endif
}

bool CracenIkg::initInput() {
#if defined(NRF_TRUSTZONE_NONSECURE)
  return false;
#else
  if (core_ == nullptr) {
    return false;
  }
  core_->IKG.INITDATA = CRACENCORE_IKG_INITDATA_INITDATA_Msk;
  return true;
#endif
}

bool CracenIkg::writeNonce(const uint32_t* words, size_t wordCount) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)words;
  (void)wordCount;
  return false;
#else
  if (core_ == nullptr || (wordCount != 0U && words == nullptr)) {
    return false;
  }
  for (size_t i = 0; i < wordCount; ++i) {
    core_->IKG.NONCE = words[i];
  }
  return true;
#endif
}

bool CracenIkg::writePersonalization(const uint32_t* words, size_t wordCount) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)words;
  (void)wordCount;
  return false;
#else
  if (core_ == nullptr || (wordCount != 0U && words == nullptr)) {
    return false;
  }
  for (size_t i = 0; i < wordCount; ++i) {
    core_->IKG.PERSONALISATIONSTRING = words[i];
  }
  return true;
#endif
}

bool CracenIkg::setReseedInterval(uint64_t interval) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)interval;
  return false;
#else
  if (core_ == nullptr) {
    return false;
  }
  core_->IKG.RESEEDINTERVALLSB = static_cast<uint32_t>(interval & 0xFFFFFFFFULL);
  core_->IKG.RESEEDINTERVALMSB =
      static_cast<uint32_t>((interval >> 32) & 0xFFFFULL);
  return true;
#endif
}

bool CracenIkg::start(uint32_t spinLimit) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)spinLimit;
  return false;
#else
  if (!active_ && !begin(spinLimit)) {
    return false;
  }
  if (core_ == nullptr) {
    return false;
  }

  clearEvent();
  core_->IKG.START = CRACENCORE_IKG_START_START_Msk;
  return waitGenerationComplete(spinLimit);
#endif
}

bool CracenIkg::waitReady(uint32_t spinLimit) const {
  if (core_ == nullptr) {
    return false;
  }
  while (spinLimit-- > 0U) {
    const uint32_t ikgStatus = core_->IKG.STATUS;
    const uint32_t pke = core_->IKG.PKESTATUS;
    if ((ikgStatus & CRACENCORE_IKG_STATUS_CATASTROPHICERROR_Msk) != 0U) {
      return false;
    }
    if ((ikgStatus & CRACENCORE_IKG_STATUS_CTRDRBGBUSY_Msk) == 0U &&
        (pke & CRACENCORE_IKG_PKESTATUS_ERASEBUSY_Msk) == 0U) {
      return true;
    }
  }
  return false;
}

bool CracenIkg::waitGenerationComplete(uint32_t spinLimit) const {
  if (core_ == nullptr) {
    return false;
  }
  while (spinLimit-- > 0U) {
    const uint32_t currentStatus = core_->IKG.STATUS;
    if ((currentStatus & (CRACENCORE_IKG_STATUS_SEEDERROR_Msk |
                          CRACENCORE_IKG_STATUS_ENTROPYERROR_Msk |
                          CRACENCORE_IKG_STATUS_CATASTROPHICERROR_Msk)) != 0U) {
      return false;
    }
    if ((currentStatus & CRACENCORE_IKG_STATUS_OKAY_Msk) != 0U &&
        (currentStatus & CRACENCORE_IKG_STATUS_CTRDRBGBUSY_Msk) == 0U) {
      return true;
    }
  }
  return false;
}

// ─── PKE / PK Engine direct access ───────────────────────────

bool CracenIkg::pkStart() {
  if (core_ == nullptr) return false;
  core_->PK.CONTROL = CRACENCORE_PK_CONTROL_START_Msk;
  return true;
}

bool CracenIkg::pkBusy() const {
  if (core_ == nullptr) return true;
  return (core_->PK.STATUS & CRACENCORE_PK_STATUS_PKBUSY_Msk) != 0U;
}

void CracenIkg::pkClearIrq() {
  if (core_ == nullptr) return;
  core_->PK.CONTROL = CRACENCORE_PK_CONTROL_CLEARIRQ_Msk;
}

void CracenIkg::pkSetCommand(uint32_t cmd) {
  if (core_ == nullptr) return;
  core_->PK.COMMAND = cmd;
}

void CracenIkg::pkSetPointers(uint8_t a, uint8_t b, uint8_t c, uint8_t n) {
  if (core_ == nullptr) return;
  uint32_t ptrs = ((uint32_t)(a & 0xF) << CRACENCORE_PK_POINTERS_OPPTRA_Pos) |
                  ((uint32_t)(b & 0xF) << CRACENCORE_PK_POINTERS_OPPTRB_Pos) |
                  ((uint32_t)(c & 0xF) << CRACENCORE_PK_POINTERS_OPPTRC_Pos) |
                  ((uint32_t)(n & 0xF) << CRACENCORE_PK_POINTERS_OPPTRN_Pos);
  core_->PK.POINTERS = ptrs;
}

void CracenIkg::pkSetOpsize(uint32_t size) {
  if (core_ == nullptr) return;
  core_->PK.OPSIZE = size;
}

void CracenIkg::pkWriteOperand(int slot, const uint8_t* data, size_t len) {
  if (core_ == nullptr || data == nullptr || slot < 0 || slot > 15) return;
  // PK data memory at 0x51808000, 256 bytes per slot, word-aligned access
  volatile uint32_t* pkRam = (volatile uint32_t*)(0x51808000UL + (uint32_t)(slot * 256));
  size_t words = (len + 3) / 4;
  for (size_t i = 0; i < words; i++) {
    uint32_t w = 0;
    if (i*4 < len) w |= (uint32_t)data[i*4];
    if (i*4+1 < len) w |= (uint32_t)data[i*4+1] << 8;
    if (i*4+2 < len) w |= (uint32_t)data[i*4+2] << 16;
    if (i*4+3 < len) w |= (uint32_t)data[i*4+3] << 24;
    pkRam[i] = w;
  }
}

bool CracenIkg::pkReadOperand(int slot, uint8_t* data, size_t len) {
  if (core_ == nullptr || data == nullptr || slot < 0 || slot > 15) return false;
  // Disable protected RAM lock so CPU can read PK data memory
  if (cracen_ != nullptr) {
    cracen_->PROTECTEDRAMLOCK = CRACEN_PROTECTEDRAMLOCK_ENABLE_Disabled;
  }
  __asm volatile("dsb 0xF" ::: "memory");
  volatile const uint32_t* pkRam = (volatile const uint32_t*)(0x51808000UL + (uint32_t)(slot * 256));
  size_t words = (len + 3) / 4;
  for (size_t i = 0; i < words; i++) {
    uint32_t w = pkRam[i];
    if (i*4 < len) data[i*4] = (uint8_t)(w & 0xFF);
    if (i*4+1 < len) data[i*4+1] = (uint8_t)((w>>8) & 0xFF);
    if (i*4+2 < len) data[i*4+2] = (uint8_t)((w>>16) & 0xFF);
    if (i*4+3 < len) data[i*4+3] = (uint8_t)((w>>24) & 0xFF);
  }
  return true;
}

bool CracenIkg::pkWaitComplete(uint32_t spinLimit) {
  while (spinLimit-- > 0U) {
    if (!pkBusy()) {
      uint32_t st = core_->PK.STATUS;
      return (st & CRACENCORE_PK_STATUS_ERRORFLAGS_Msk) == 0U;
    }
  }
  return false;
}

// ─── IKG high-level ECC operations ─────────────────────────────

bool CracenIkg::ikgGenerateKey() {
  if (core_ == nullptr) return false;
  // Set up IKG for P-256 key generation
  // Write personalization and nonce for DRBG seeding
  uint32_t pers[8] = {};
  uint32_t nonce[8] = {};
  for (int i = 0; i < 8; i++) {
    pers[i] = 0x4E524635UL + i;  // "NRF5" pattern
    nonce[i] = (uint32_t)(i * 0x9E3779B9UL + 0x12345678UL);
  }
  if (!initInput()) return false;
  if (!writePersonalization(pers, 8)) return false;
  if (!writeNonce(nonce, 8)) return false;
  if (!markSeedValid(true)) return false;
  if (!start(1000000UL)) return false;
  return waitGenerationComplete(1000000UL) && privateKeysStored();
}

bool CracenIkg::ikgEcdsaSign(const uint8_t hash[32]) {
  if (core_ == nullptr || hash == nullptr) return false;
  if (!privateKeysStored()) return false;
  // Let IKG configure PK
  
  pkWriteOperand(1, hash, 32);
  
  // Set IKG command: ECDSA sign with key slot 0
  core_->IKG.PKECOMMAND = 
      CRACENCORE_IKG_PKECOMMAND_SECUREMODE_ACTIVATED << CRACENCORE_IKG_PKECOMMAND_SECUREMODE_Pos |
      (0U << CRACENCORE_IKG_PKECOMMAND_SELECTEDKEY_Pos) |
      CRACENCORE_IKG_PKECOMMAND_OPSEL_ECDSA << CRACENCORE_IKG_PKECOMMAND_OPSEL_Pos;
  
  core_->IKG.PKECONTROL = CRACENCORE_IKG_PKECONTROL_PKESTART_Msk;
  
  // Wait for completion
  uint32_t spin = 5000000UL;
  while (spin-- > 0U) {
    uint32_t st = core_->IKG.PKESTATUS;
    if ((st & 0x1) != 0) return false;  // ERROR (bit 0)
    if ((st & 0x4) == 0) return true;   // IKGPKBUSY cleared = done
  }
  return false;
}

bool CracenIkg::ikgPointMul(const uint8_t scalar[32], const uint8_t pointX[32], const uint8_t pointY[32]) {
  if (core_ == nullptr) return false;
  // Let IKG configure PK
  
  pkWriteOperand(0, scalar, 32);
  pkWriteOperand(4, pointX, 32);
  pkWriteOperand(5, pointY, 32);
  
  // Set IKG command: PTMUL
  core_->IKG.PKECOMMAND = 
      CRACENCORE_IKG_PKECOMMAND_SECUREMODE_ACTIVATED << CRACENCORE_IKG_PKECOMMAND_SECUREMODE_Pos |
      CRACENCORE_IKG_PKECOMMAND_OPSEL_PTMUL << CRACENCORE_IKG_PKECOMMAND_OPSEL_Pos;
  
  core_->IKG.PKECONTROL = CRACENCORE_IKG_PKECONTROL_PKESTART_Msk;
  
  uint32_t spin = 5000000UL;
  while (spin-- > 0U) {
    uint32_t st = core_->IKG.PKESTATUS;
    if ((st & 0x1) != 0) return false;
    if ((st & 0x4) == 0) return true;
  }
  return false;
}

bool CracenIkg::ikgReadPublicKey(uint8_t pubKey[65]) {
  if (core_ == nullptr || pubKey == nullptr) return false;
  if (!privateKeysStored()) return false;
  
  // Let IKG configure PK
  
  core_->IKG.PKECOMMAND = 
      CRACENCORE_IKG_PKECOMMAND_SECUREMODE_ACTIVATED << CRACENCORE_IKG_PKECOMMAND_SECUREMODE_Pos |
      (0U << CRACENCORE_IKG_PKECOMMAND_SELECTEDKEY_Pos) |
      CRACENCORE_IKG_PKECOMMAND_OPSEL_PUBKEY << CRACENCORE_IKG_PKECOMMAND_OPSEL_Pos;
  
  // Trigger
  core_->IKG.PKECONTROL = CRACENCORE_IKG_PKECONTROL_PKESTART_Msk;
  
  // Wait for completion (IKGPKBUSY bit 2 = 0x4)
  uint32_t spin = 10000000UL;
  while (spin-- > 0U) {
    uint32_t st = core_->IKG.PKESTATUS;
    if (st & 0x1) return false;  // ERROR
    if ((st & 0x4) == 0 && spin < 9999990UL) break;  // IKGPKBUSY cleared (with debounce: skip first few iterations)
  }
  if (spin == 0) return false;
  
  pubKey[0] = 0x04;
  if (!pkReadOperand(7, pubKey + 1, 32)) return false;
  if (!pkReadOperand(8, pubKey + 33, 32)) return false;
  return true;
}

bool CracenIkg::ikgReadEcdsaSignature(uint8_t r[32], uint8_t s[32]) {
  // ECDSA result: r in slot 2, s in slot 3
  if (!pkReadOperand(2, r, 32)) return false;
  if (!pkReadOperand(3, s, 32)) return false;
  return true;
}

bool CracenIkg::ikgReadPointMulResult(uint8_t x[32], uint8_t y[32]) {
  // PTMUL result: x in slot 7, y in slot 8
  if (!pkReadOperand(7, x, 32)) return false;
  if (!pkReadOperand(8, y, 32)) return false;
  return true;
}

uint32_t CracenIkg::pkStatus() const {
  return (core_ == nullptr) ? 0xFFFFFFFFUL : core_->PK.STATUS;
}

uint32_t CracenIkg::ikgPkeStatus() const {
  return (core_ == nullptr) ? 0xFFFFFFFFUL : core_->IKG.PKESTATUS;
}

uint32_t CracenIkg::ikgStatus() const {
  return (core_ == nullptr) ? 0xFFFFFFFFUL : core_->IKG.STATUS;
}

uint32_t CracenIkg::pkCommand() const {
  return (core_ == nullptr) ? 0xFFFFFFFFUL : core_->PK.COMMAND;
}

bool CracenIkg::pkConfigureP256() {
  if (core_ == nullptr || !active_) return false;
  // Only set curve and size fields — OPEADDR is set by IKG operation.
  // Read current command to preserve OPEADDR (default 0xF is fine)
  uint32_t cmd = core_->PK.COMMAND;
  // Clear fields we're setting
  cmd &= ~(CRACENCORE_PK_COMMAND_SELCURVE_Msk |
           CRACENCORE_PK_COMMAND_OPBYTESM1_Msk |
           CRACENCORE_PK_COMMAND_FIELDF_Msk);
  // Set P-256 configuration
  cmd |= (CRACENCORE_PK_COMMAND_SELCURVE_P256 << CRACENCORE_PK_COMMAND_SELCURVE_Pos);
  cmd |= (31U << CRACENCORE_PK_COMMAND_OPBYTESM1_Pos);  // 32 bytes
  cmd |= (0U << CRACENCORE_PK_COMMAND_FIELDF_Pos);  // GF(p)
  core_->PK.COMMAND = cmd;
  core_->PK.OPSIZE = 0x0100;
  return true;
}

Tampc::Tampc(uint32_t base)
    : tampc_(base == 0U ? nullptr : reinterpret_cast<NRF_TAMPC_Type*>(base)) {}

uint32_t Tampc::status() const {
  return (tampc_ == nullptr) ? 0U : tampc_->STATUS;
}

bool Tampc::tamperDetected() const {
  return (status() & (TAMPC_STATUS_ACTIVESHIELD_Msk | TAMPC_STATUS_PROTECT_Msk |
                      TAMPC_STATUS_CRACENTAMP_Msk |
                      TAMPC_STATUS_GLITCHSLOWDOMAIN0_Msk |
                      TAMPC_STATUS_GLITCHFASTDOMAIN0_Msk |
                      TAMPC_STATUS_GLITCHFASTDOMAIN1_Msk |
                      TAMPC_STATUS_GLITCHFASTDOMAIN2_Msk |
                      TAMPC_STATUS_GLITCHFASTDOMAIN3_Msk)) != 0U;
}

bool Tampc::writeErrorDetected() const {
  if (tampc_ == nullptr) {
    return false;
  }
  return ((tampc_->EVENTS_WRITEERROR &
           TAMPC_EVENTS_WRITEERROR_EVENTS_WRITEERROR_Msk) >>
          TAMPC_EVENTS_WRITEERROR_EVENTS_WRITEERROR_Pos) ==
         TAMPC_EVENTS_WRITEERROR_EVENTS_WRITEERROR_Generated;
}

bool Tampc::pollTamper(bool clearEvent) {
  if (tampc_ == nullptr) {
    return false;
  }
  const bool signaled =
      ((tampc_->EVENTS_TAMPER & TAMPC_EVENTS_TAMPER_EVENTS_TAMPER_Msk) >>
       TAMPC_EVENTS_TAMPER_EVENTS_TAMPER_Pos) ==
      TAMPC_EVENTS_TAMPER_EVENTS_TAMPER_Generated;
  if (signaled && clearEvent) {
    tampc_->EVENTS_TAMPER = 0U;
  }
  return signaled;
}

bool Tampc::pollWriteError(bool clearEvent) {
  if (tampc_ == nullptr) {
    return false;
  }
  const bool signaled =
      ((tampc_->EVENTS_WRITEERROR &
        TAMPC_EVENTS_WRITEERROR_EVENTS_WRITEERROR_Msk) >>
       TAMPC_EVENTS_WRITEERROR_EVENTS_WRITEERROR_Pos) ==
      TAMPC_EVENTS_WRITEERROR_EVENTS_WRITEERROR_Generated;
  if (signaled && clearEvent) {
    tampc_->EVENTS_WRITEERROR = 0U;
  }
  return signaled;
}

void Tampc::clearEvents() {
  if (tampc_ == nullptr) {
    return;
  }
  tampc_->EVENTS_TAMPER = 0U;
  tampc_->EVENTS_WRITEERROR = 0U;
}

void Tampc::enableInterrupts(bool tamper, bool writeError) {
  if (tampc_ == nullptr) {
    return;
  }
  uint32_t enableMask = 0U;
  uint32_t clearMask = 0U;
  if (tamper) {
    enableMask |= TAMPC_INTENSET_TAMPER_Msk;
  } else {
    clearMask |= TAMPC_INTENCLR_TAMPER_Msk;
  }
  if (writeError) {
    enableMask |= TAMPC_INTENSET_WRITEERROR_Msk;
  } else {
    clearMask |= TAMPC_INTENCLR_WRITEERROR_Msk;
  }
  if (enableMask != 0U) {
    tampc_->INTENSET = enableMask;
  }
  if (clearMask != 0U) {
    tampc_->INTENCLR = clearMask;
  }
}

bool Tampc::pendingTamperInterrupt() const {
  return (tampc_ != nullptr) &&
         ((tampc_->INTPEND & TAMPC_INTPEND_TAMPER_Msk) != 0U);
}

bool Tampc::pendingWriteErrorInterrupt() const {
  return (tampc_ != nullptr) &&
         ((tampc_->INTPEND & TAMPC_INTPEND_WRITEERROR_Msk) != 0U);
}

bool Tampc::writeProtectedControl(volatile uint32_t& ctrlRegister,
                                  uint32_t valueHigh,
                                  uint32_t valueLow,
                                  bool enable,
                                  bool lock) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)ctrlRegister;
  (void)valueHigh;
  (void)valueLow;
  (void)enable;
  (void)lock;
  return false;
#else
  if (tampc_ == nullptr) {
    return false;
  }
  ctrlRegister = kTampcWriteKey | kTampcWriteProtectionClear;
  ctrlRegister = kTampcWriteKey |
                 (enable ? valueHigh : valueLow) |
                 (lock ? kTampcLockEnabled : 0U);
  return !pollWriteError(false);
#endif
}

bool Tampc::setInternalResetOnTamper(bool enable, bool lock) {
  return writeProtectedControl(
      tampc_->PROTECT.INTRESETEN.CTRL,
      (TAMPC_PROTECT_INTRESETEN_CTRL_VALUE_High
       << TAMPC_PROTECT_INTRESETEN_CTRL_VALUE_Pos),
      (TAMPC_PROTECT_INTRESETEN_CTRL_VALUE_Low
       << TAMPC_PROTECT_INTRESETEN_CTRL_VALUE_Pos),
      enable, lock);
}

bool Tampc::setExternalResetOnTamper(bool enable, bool lock) {
  return writeProtectedControl(
      tampc_->PROTECT.EXTRESETEN.CTRL,
      (TAMPC_PROTECT_EXTRESETEN_CTRL_VALUE_High
       << TAMPC_PROTECT_EXTRESETEN_CTRL_VALUE_Pos),
      (TAMPC_PROTECT_EXTRESETEN_CTRL_VALUE_Low
       << TAMPC_PROTECT_EXTRESETEN_CTRL_VALUE_Pos),
      enable, lock);
}

bool Tampc::setEraseProtect(bool enable, bool lock) {
  return writeProtectedControl(
      tampc_->PROTECT.ERASEPROTECT.CTRL,
      (TAMPC_PROTECT_ERASEPROTECT_CTRL_VALUE_High
       << TAMPC_PROTECT_ERASEPROTECT_CTRL_VALUE_Pos),
      (TAMPC_PROTECT_ERASEPROTECT_CTRL_VALUE_Low
       << TAMPC_PROTECT_ERASEPROTECT_CTRL_VALUE_Pos),
      enable, lock);
}

bool Tampc::setCracenTamperMonitor(bool enable, bool lock) {
  return writeProtectedControl(
      tampc_->PROTECT.CRACENTAMP.CTRL,
      (TAMPC_PROTECT_CRACENTAMP_CTRL_VALUE_High
       << TAMPC_PROTECT_CRACENTAMP_CTRL_VALUE_Pos),
      (TAMPC_PROTECT_CRACENTAMP_CTRL_VALUE_Low
       << TAMPC_PROTECT_CRACENTAMP_CTRL_VALUE_Pos),
      enable, lock);
}

bool Tampc::setActiveShieldMonitor(bool enable, bool lock) {
  return writeProtectedControl(
      tampc_->PROTECT.ACTIVESHIELD.CTRL,
      (TAMPC_PROTECT_ACTIVESHIELD_CTRL_VALUE_High
       << TAMPC_PROTECT_ACTIVESHIELD_CTRL_VALUE_Pos),
      (TAMPC_PROTECT_ACTIVESHIELD_CTRL_VALUE_Low
       << TAMPC_PROTECT_ACTIVESHIELD_CTRL_VALUE_Pos),
      enable, lock);
}

bool Tampc::setGlitchSlowMonitor(bool enable, bool lock) {
  return writeProtectedControl(
      tampc_->PROTECT.GLITCHSLOWDOMAIN.CTRL,
      (TAMPC_PROTECT_GLITCHSLOWDOMAIN_CTRL_VALUE_High
       << TAMPC_PROTECT_GLITCHSLOWDOMAIN_CTRL_VALUE_Pos),
      (TAMPC_PROTECT_GLITCHSLOWDOMAIN_CTRL_VALUE_Low
       << TAMPC_PROTECT_GLITCHSLOWDOMAIN_CTRL_VALUE_Pos),
      enable, lock);
}

bool Tampc::setGlitchFastMonitor(bool enable, bool lock) {
  return writeProtectedControl(
      tampc_->PROTECT.GLITCHFASTDOMAIN.CTRL,
      (TAMPC_PROTECT_GLITCHFASTDOMAIN_CTRL_VALUE_High
       << TAMPC_PROTECT_GLITCHFASTDOMAIN_CTRL_VALUE_Pos),
      (TAMPC_PROTECT_GLITCHFASTDOMAIN_CTRL_VALUE_Low
       << TAMPC_PROTECT_GLITCHFASTDOMAIN_CTRL_VALUE_Pos),
      enable, lock);
}

bool Tampc::setActiveShieldChannelMask(uint8_t channelMask) {
#if defined(NRF_TRUSTZONE_NONSECURE)
  (void)channelMask;
  return false;
#else
  if (tampc_ == nullptr) {
    return false;
  }
  tampc_->ACTIVESHIELD.CHEN = static_cast<uint32_t>(channelMask & 0x0FU);
  return true;
#endif
}

uint8_t Tampc::activeShieldChannelMask() const {
  if (tampc_ == nullptr) {
    return 0U;
  }
  return static_cast<uint8_t>(tampc_->ACTIVESHIELD.CHEN & 0x0FU);
}

bool Tampc::activeShieldChannelEnabled(uint8_t channel) const {
  if (channel > 3U) {
    return false;
  }
  return (activeShieldChannelMask() & static_cast<uint8_t>(1U << channel)) != 0U;
}

bool Tampc::setActiveShieldChannelEnabled(uint8_t channel, bool enable) {
  if (channel > 3U) {
    return false;
  }
  uint8_t mask = activeShieldChannelMask();
  if (enable) {
    mask = static_cast<uint8_t>(mask | static_cast<uint8_t>(1U << channel));
  } else {
    mask = static_cast<uint8_t>(mask & ~static_cast<uint8_t>(1U << channel));
  }
  return setActiveShieldChannelMask(mask);
}

bool Tampc::internalResetOnTamperEnabled() const {
  return protectedSignalEnabled(tampc_->PROTECT.INTRESETEN.CTRL);
}

bool Tampc::externalResetOnTamperEnabled() const {
  return protectedSignalEnabled(tampc_->PROTECT.EXTRESETEN.CTRL);
}

bool Tampc::eraseProtectEnabled() const {
  return protectedSignalEnabled(tampc_->PROTECT.ERASEPROTECT.CTRL);
}

bool Tampc::cracenTamperMonitorEnabled() const {
  return protectedSignalEnabled(tampc_->PROTECT.CRACENTAMP.CTRL);
}

bool Tampc::activeShieldMonitorEnabled() const {
  return protectedSignalEnabled(tampc_->PROTECT.ACTIVESHIELD.CTRL);
}

bool Tampc::glitchSlowMonitorEnabled() const {
  return protectedSignalEnabled(tampc_->PROTECT.GLITCHSLOWDOMAIN.CTRL);
}

bool Tampc::glitchFastMonitorEnabled() const {
  return protectedSignalEnabled(tampc_->PROTECT.GLITCHFASTDOMAIN.CTRL);
}

bool Tampc::domainDbgenEnabled(uint8_t domain) const {
  return validDomainIndex(domain) &&
         protectedSignalEnabled(tampc_->PROTECT.DOMAIN[domain].DBGEN.CTRL);
}

bool Tampc::domainNidenEnabled(uint8_t domain) const {
  return validDomainIndex(domain) &&
         protectedSignalEnabled(tampc_->PROTECT.DOMAIN[domain].NIDEN.CTRL);
}

bool Tampc::domainSpidenEnabled(uint8_t domain) const {
  return validDomainIndex(domain) &&
         protectedSignalEnabled(tampc_->PROTECT.DOMAIN[domain].SPIDEN.CTRL);
}

bool Tampc::domainSpnidenEnabled(uint8_t domain) const {
  return validDomainIndex(domain) &&
         protectedSignalEnabled(tampc_->PROTECT.DOMAIN[domain].SPNIDEN.CTRL);
}

bool Tampc::apDbgenEnabled(uint8_t ap) const {
  return validApIndex(ap) &&
         protectedSignalEnabled(tampc_->PROTECT.AP[ap].DBGEN.CTRL);
}

bool Tampc::setDomainDbgen(bool enable, bool lock, uint8_t domain) {
  return validDomainIndex(domain) &&
         writeProtectedControl(
             tampc_->PROTECT.DOMAIN[domain].DBGEN.CTRL,
             (TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_VALUE_High
              << TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_VALUE_Pos),
             (TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_VALUE_Low
              << TAMPC_PROTECT_DOMAIN_DBGEN_CTRL_VALUE_Pos),
             enable, lock);
}

bool Tampc::setDomainNiden(bool enable, bool lock, uint8_t domain) {
  return validDomainIndex(domain) &&
         writeProtectedControl(
             tampc_->PROTECT.DOMAIN[domain].NIDEN.CTRL,
             (TAMPC_PROTECT_DOMAIN_NIDEN_CTRL_VALUE_High
              << TAMPC_PROTECT_DOMAIN_NIDEN_CTRL_VALUE_Pos),
             (TAMPC_PROTECT_DOMAIN_NIDEN_CTRL_VALUE_Low
              << TAMPC_PROTECT_DOMAIN_NIDEN_CTRL_VALUE_Pos),
             enable, lock);
}

bool Tampc::setDomainSpiden(bool enable, bool lock, uint8_t domain) {
  return validDomainIndex(domain) &&
         writeProtectedControl(
             tampc_->PROTECT.DOMAIN[domain].SPIDEN.CTRL,
             (TAMPC_PROTECT_DOMAIN_SPIDEN_CTRL_VALUE_High
              << TAMPC_PROTECT_DOMAIN_SPIDEN_CTRL_VALUE_Pos),
             (TAMPC_PROTECT_DOMAIN_SPIDEN_CTRL_VALUE_Low
              << TAMPC_PROTECT_DOMAIN_SPIDEN_CTRL_VALUE_Pos),
             enable, lock);
}

bool Tampc::setDomainSpniden(bool enable, bool lock, uint8_t domain) {
  return validDomainIndex(domain) &&
         writeProtectedControl(
             tampc_->PROTECT.DOMAIN[domain].SPNIDEN.CTRL,
             (TAMPC_PROTECT_DOMAIN_SPNIDEN_CTRL_VALUE_High
              << TAMPC_PROTECT_DOMAIN_SPNIDEN_CTRL_VALUE_Pos),
             (TAMPC_PROTECT_DOMAIN_SPNIDEN_CTRL_VALUE_Low
              << TAMPC_PROTECT_DOMAIN_SPNIDEN_CTRL_VALUE_Pos),
             enable, lock);
}

bool Tampc::setApDbgen(bool enable, bool lock, uint8_t ap) {
  return validApIndex(ap) &&
         writeProtectedControl(
             tampc_->PROTECT.AP[ap].DBGEN.CTRL,
             (TAMPC_PROTECT_AP_DBGEN_CTRL_VALUE_High
              << TAMPC_PROTECT_AP_DBGEN_CTRL_VALUE_Pos),
             (TAMPC_PROTECT_AP_DBGEN_CTRL_VALUE_Low
              << TAMPC_PROTECT_AP_DBGEN_CTRL_VALUE_Pos),
             enable, lock);
}

bool Tampc::protectStatusError() const {
  return tamperDetected() || writeErrorDetected();
}

bool Tampc::cracenTamperStatusError() const {
  return protectedStatusError(tampc_->PROTECT.CRACENTAMP.STATUS);
}

bool Tampc::activeShieldStatusError() const {
  return protectedStatusError(tampc_->PROTECT.ACTIVESHIELD.STATUS);
}

bool Tampc::glitchSlowStatusError() const {
  return protectedStatusError(tampc_->PROTECT.GLITCHSLOWDOMAIN.STATUS);
}

bool Tampc::glitchFastStatusError() const {
  return protectedStatusError(tampc_->PROTECT.GLITCHFASTDOMAIN.STATUS);
}

bool Tampc::intResetStatusError() const {
  return protectedStatusError(tampc_->PROTECT.INTRESETEN.STATUS);
}

bool Tampc::extResetStatusError() const {
  return protectedStatusError(tampc_->PROTECT.EXTRESETEN.STATUS);
}

bool Tampc::eraseProtectStatusError() const {
  return protectedStatusError(tampc_->PROTECT.ERASEPROTECT.STATUS);
}

bool Tampc::domainDbgenStatusError(uint8_t domain) const {
  return validDomainIndex(domain) &&
         protectedStatusError(tampc_->PROTECT.DOMAIN[domain].DBGEN.STATUS);
}

bool Tampc::domainNidenStatusError(uint8_t domain) const {
  return validDomainIndex(domain) &&
         protectedStatusError(tampc_->PROTECT.DOMAIN[domain].NIDEN.STATUS);
}

bool Tampc::domainSpidenStatusError(uint8_t domain) const {
  return validDomainIndex(domain) &&
         protectedStatusError(tampc_->PROTECT.DOMAIN[domain].SPIDEN.STATUS);
}

bool Tampc::domainSpnidenStatusError(uint8_t domain) const {
  return validDomainIndex(domain) &&
         protectedStatusError(tampc_->PROTECT.DOMAIN[domain].SPNIDEN.STATUS);
}

bool Tampc::apDbgenStatusError(uint8_t ap) const {
  return validApIndex(ap) &&
         protectedStatusError(tampc_->PROTECT.AP[ap].DBGEN.STATUS);
}

bool Tampc::protectedSignalEnabled(const volatile uint32_t& ctrlRegister) const {
  return (static_cast<uint32_t>(ctrlRegister) & 0x1U) != 0U;
}

bool Tampc::protectedStatusError(const volatile uint32_t& statusRegister) const {
  return (static_cast<uint32_t>(statusRegister) & 0x1U) != 0U;
}

bool Tampc::validDomainIndex(uint8_t domain) const {
  return (tampc_ != nullptr) && (domain < TAMPC_PROTECT_DOMAIN_MaxCount);
}

bool Tampc::validApIndex(uint8_t ap) const {
  return (tampc_ != nullptr) && (ap < TAMPC_PROTECT_AP_MaxCount);
}

}  // namespace xiao_nrf54l15
