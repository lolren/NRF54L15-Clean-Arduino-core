/*
 * System startup for the nRF54L15 clean Arduino core.
 *
 * For secure builds, this mirrors the Zephyr/nrfx startup writes that were
 * required to reach the same low-power SYSTEM OFF behavior on XIAO nRF54L15.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cmsis.h"
#include "nrf54l15.h"

uint32_t SystemCoreClock = 64000000UL;

#if !defined(NRF_TRUSTZONE_NONSECURE)
static const NRF_FICR_Type *const kFicr =
    (const NRF_FICR_Type *)0x00FFC000UL;
static const uintptr_t kErrata37Reg = 0x5005340CUL;
static const uintptr_t kDeviceConfigReg = 0x50120440UL;
static const uintptr_t kErrata32CheckReg = 0x00FFC334UL;
static const uintptr_t kErrata32Reg = 0x50120640UL;
static const uintptr_t kErrata31Reg0 = 0x50120624UL;
static const uintptr_t kErrata31Reg1 = 0x5012063CUL;
static const uintptr_t kErrata40Reg = 0x5008A7ACUL;
static const uintptr_t kRramcLowPowerConfigReg = 0x5004B518UL;
static const uintptr_t kGlitchDetConfigReg = 0x5004B5A0UL;
static const uintptr_t kCacheEnableReg = 0xE0082404UL;
#endif

static inline volatile uint32_t *reg32(uintptr_t address)
{
    return (volatile uint32_t *)address;
}

static void setPllFrequency(uint32_t targetFrequency)
{
    NRF_OSCILLATORS->PLL.FREQ = targetFrequency;

    uint32_t guard = 0U;
    while ((((NRF_OSCILLATORS->PLL.CURRENTFREQ &
              OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_Msk) >>
             OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_Pos) != targetFrequency) &&
           (guard++ < 1000000UL)) {
        __NOP();
    }
}

void SystemCoreClockUpdate(void)
{
    uint32_t current = (NRF_OSCILLATORS->PLL.CURRENTFREQ &
                        OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_Msk) >>
                       OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_Pos;

    if (current == OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_CK128M) {
        SystemCoreClock = 128000000UL;
    } else {
        SystemCoreClock = 64000000UL;
    }
}

#if !defined(NRF_TRUSTZONE_NONSECURE)
static bool zephyrErrata31(void)
{
    return (kFicr->INFO.PART == 0x1CU) && (kFicr->INFO.VARIANT == 0x01U);
}

static bool zephyrErrata32(void)
{
    return (kFicr->INFO.PART == 0x1CU) && (kFicr->INFO.VARIANT == 0x01U);
}

static bool zephyrErrata37(void)
{
    return kFicr->INFO.PART == 0x1CU;
}

static bool zephyrErrata40(void)
{
    return (kFicr->INFO.PART == 0x1CU) && (kFicr->INFO.VARIANT == 0x01U);
}

static void zephyrCopyTrimConfig(void)
{
    for (uint32_t index = 0U; index < FICR_TRIMCNF_MaxCount; ++index) {
        const uint32_t address = kFicr->TRIMCNF[index].ADDR;
        if ((address == 0xFFFFFFFFUL) || (address == 0x00000000UL)) {
            break;
        }

        *reg32(address) = kFicr->TRIMCNF[index].DATA;
    }
}

static void zephyrApplySystemInitParity(void)
{
    if (zephyrErrata37()) {
        *reg32(kErrata37Reg) = 1U;
    }

    zephyrCopyTrimConfig();

    if (*reg32(kDeviceConfigReg) == 0U) {
        *reg32(kDeviceConfigReg) = 0xC8U;
    }

    if (zephyrErrata32() && (*reg32(kErrata32CheckReg) <= 0x180A1D00UL)) {
        *reg32(kErrata32Reg) = 0x1EA9E040UL;
    }

    if (zephyrErrata40()) {
        *reg32(kErrata40Reg) = 0x040A0078UL;
    }

    if (zephyrErrata31()) {
        *reg32(kErrata31Reg0) = 20U | (1U << 5);
        *reg32(kErrata31Reg1) &= ~(1UL << 19);
    }

    if ((NRF_RESET->RESETREAS & RESET_RESETREAS_RESETPIN_Msk) != 0U) {
        NRF_RESET->RESETREAS = ~RESET_RESETREAS_RESETPIN_Msk;
    }

    *reg32(kRramcLowPowerConfigReg) = 3U;
    *reg32(kGlitchDetConfigReg) = 0U;
}

static void zephyrApplyClockTrimParity(void)
{
    const uint32_t xosc32ktrim = kFicr->XOSC32KTRIM;
    const uint32_t slopeFieldK =
        (xosc32ktrim & FICR_XOSC32KTRIM_SLOPE_Msk) >> FICR_XOSC32KTRIM_SLOPE_Pos;
    const uint32_t slopeMaskK =
        FICR_XOSC32KTRIM_SLOPE_Msk >> FICR_XOSC32KTRIM_SLOPE_Pos;
    const uint32_t slopeSignK = slopeMaskK - (slopeMaskK >> 1U);
    const int32_t slopeK =
        (int32_t)(slopeFieldK ^ slopeSignK) - (int32_t)slopeSignK;
    const uint32_t offsetK =
        (xosc32ktrim & FICR_XOSC32KTRIM_OFFSET_Msk) >> FICR_XOSC32KTRIM_OFFSET_Pos;
    const uint32_t lfxoIntcapFemtoF = 16000UL;
    const uint32_t lfxoMidValue =
        (2UL * lfxoIntcapFemtoF - 12000UL) *
            (uint32_t)(slopeK + 392) +
        ((offsetK << 3U) * 1000UL);
    uint32_t lfxoIntcap = lfxoMidValue / 512000UL;
    if ((lfxoMidValue % 512000UL) >= 256000UL) {
        ++lfxoIntcap;
    }
    NRF_OSCILLATORS->XOSC32KI.INTCAP =
        (lfxoIntcap << OSCILLATORS_XOSC32KI_INTCAP_VAL_Pos) &
        OSCILLATORS_XOSC32KI_INTCAP_VAL_Msk;

    const uint32_t xosc32mtrim = kFicr->XOSC32MTRIM;
    const uint32_t slopeFieldM =
        (xosc32mtrim & FICR_XOSC32MTRIM_SLOPE_Msk) >> FICR_XOSC32MTRIM_SLOPE_Pos;
    const uint32_t slopeMaskM =
        FICR_XOSC32MTRIM_SLOPE_Msk >> FICR_XOSC32MTRIM_SLOPE_Pos;
    const uint32_t slopeSignM = slopeMaskM - (slopeMaskM >> 1U);
    const int32_t slopeM =
        (int32_t)(slopeFieldM ^ slopeSignM) - (int32_t)slopeSignM;
    const uint32_t offsetM =
        (xosc32mtrim & FICR_XOSC32MTRIM_OFFSET_Msk) >> FICR_XOSC32MTRIM_OFFSET_Pos;
    const uint32_t hfxoIntcapFemtoF = 16000UL;
    const uint32_t hfxoMidValue =
        (((hfxoIntcapFemtoF - 5500UL) * (uint32_t)(slopeM + 791)) +
         ((offsetM << 2U) * 1000UL)) >>
        8U;
    uint32_t hfxoIntcap = hfxoMidValue / 1000UL;
    if ((hfxoMidValue % 1000UL) >= 500UL) {
        ++hfxoIntcap;
    }
    NRF_OSCILLATORS->XOSC32M.CONFIG.INTCAP =
        (hfxoIntcap << OSCILLATORS_XOSC32M_CONFIG_INTCAP_VAL_Pos) &
        OSCILLATORS_XOSC32M_CONFIG_INTCAP_VAL_Msk;

    NRF_REGULATORS->VREGMAIN.DCDCEN = REGULATORS_VREGMAIN_DCDCEN_VAL_Enabled;
    *reg32(kCacheEnableReg) = 1U;
}
#endif

void SystemInit(void)
{
#if defined(ARDUINO_NRF54_CPU_128M)
    setPllFrequency(OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_CK128M);
#else
    setPllFrequency(OSCILLATORS_PLL_CURRENTFREQ_CURRENTFREQ_CK64M);
#endif

#if !defined(NRF_TRUSTZONE_NONSECURE)
    zephyrApplySystemInitParity();
    zephyrApplyClockTrimParity();
#endif

    SystemCoreClockUpdate();
}
