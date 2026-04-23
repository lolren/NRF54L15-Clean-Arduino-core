#if defined(NRF54L15_CLEAN_MATTER_CORE_ENABLE) && \
    (NRF54L15_CLEAN_MATTER_CORE_ENABLE != 0)

#if !defined(NRF54L15_CLEAN_MATTER_SUPPORT_SEED_AVAILABLE)
#error "Enable the staged Matter seam with build.matter_seam_flags so the bridge gets the staged support seed include path."
#endif

#include "../third_party/connectedhomeip/src/lib/support/Base64.cpp"

#endif
