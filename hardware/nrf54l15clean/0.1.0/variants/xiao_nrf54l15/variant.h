#ifndef VARIANT_H
#define VARIANT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pins_arduino.h"

typedef enum {
    XIAO_NRF54L15_ANTENNA_CERAMIC = 0,
    XIAO_NRF54L15_ANTENNA_EXTERNAL = 1,
} xiao_nrf54l15_antenna_t;

void xiaoNrf54l15SetAntenna(xiao_nrf54l15_antenna_t antenna);
xiao_nrf54l15_antenna_t xiaoNrf54l15GetAntenna(void);

#ifdef __cplusplus
}
#endif

#endif
