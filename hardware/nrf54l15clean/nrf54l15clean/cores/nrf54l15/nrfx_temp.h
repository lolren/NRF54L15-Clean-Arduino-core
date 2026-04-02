#ifndef NRFX_TEMP_H_
#define NRFX_TEMP_H_

#include <stdint.h>

typedef int32_t nrfx_err_t;

static constexpr nrfx_err_t NRFX_SUCCESS = 0;
static constexpr nrfx_err_t NRFX_ERROR_INVALID_STATE = 1;

typedef struct {
  uint8_t dummy;
} nrfx_temp_config_t;

#define NRFX_TEMP_DEFAULT_CONFIG \
  { 0U }

typedef void (*nrfx_temp_handler_t)(int32_t raw_temp);

#ifdef __cplusplus
extern "C" {
#endif

nrfx_err_t nrfx_temp_init(const nrfx_temp_config_t* config, nrfx_temp_handler_t handler);
void nrfx_temp_uninit(void);
void nrfx_temp_measure(void);
int32_t nrfx_temp_result_get(void);

#ifdef __cplusplus
}
#endif

#endif  // NRFX_TEMP_H_
