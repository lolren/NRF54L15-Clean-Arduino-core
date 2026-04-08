#pragma once

#include "nrf54l15_hal.h"

namespace xiao_nrf54l15::hal_internal {

xiao_nrf54l15_antenna_t boardAntennaSelectionFromPath(BoardAntennaPath path);
BoardAntennaPath boardDesiredAntennaPath();
void setBoardDesiredAntennaPath(BoardAntennaPath path);

}  // namespace xiao_nrf54l15::hal_internal
