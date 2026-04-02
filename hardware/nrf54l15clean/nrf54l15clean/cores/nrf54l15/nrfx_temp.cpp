#include "nrfx_temp.h"

#include "nrf54l15.h"

namespace {
nrfx_temp_handler_t g_temp_handler = nullptr;
bool g_temp_initialized = false;
int32_t g_last_temp_qc = 0;

#ifdef NRF_TRUSTZONE_NONSECURE
static constexpr uintptr_t kTempBase = 0x400D7000UL;
#else
static constexpr uintptr_t kTempBase = 0x500D7000UL;
#endif

NRF_TEMP_Type* tempPeripheral() {
  return reinterpret_cast<NRF_TEMP_Type*>(kTempBase);
}
}  // namespace

extern "C" nrfx_err_t nrfx_temp_init(const nrfx_temp_config_t* config,
                                     nrfx_temp_handler_t handler) {
  (void)config;
  g_temp_handler = handler;
  g_temp_initialized = true;
  g_last_temp_qc = 0;
  return NRFX_SUCCESS;
}

extern "C" void nrfx_temp_uninit(void) {
  g_temp_handler = nullptr;
  g_temp_initialized = false;
  g_last_temp_qc = 0;
}

extern "C" void nrfx_temp_measure(void) {
  if (!g_temp_initialized) {
    return;
  }

  NRF_TEMP_Type* const temp = tempPeripheral();
  temp->EVENTS_DATARDY = 0U;
  temp->TASKS_START = TEMP_TASKS_START_TASKS_START_Trigger;
  for (uint32_t spin = 0U; spin < 200000UL; ++spin) {
    if (temp->EVENTS_DATARDY != 0U) {
      break;
    }
  }
  temp->TASKS_STOP = TEMP_TASKS_STOP_TASKS_STOP_Trigger;
  if (temp->EVENTS_DATARDY == 0U) {
    return;
  }

  g_last_temp_qc = static_cast<int32_t>(temp->TEMP);
  temp->EVENTS_DATARDY = 0U;
  if (g_temp_handler != nullptr) {
    g_temp_handler(g_last_temp_qc);
  }
}

extern "C" int32_t nrfx_temp_result_get(void) { return g_last_temp_qc; }
