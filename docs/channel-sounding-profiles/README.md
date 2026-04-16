# Channel Sounding Calibration Profiles

This directory stores measured calibration artifacts for the real two-board
phase-based Channel Sounding path.

Each profile is intentionally explicit about:

- the board pair
- the physical reference distance used during the fit
- the metric that was fitted
- the resulting `scale` and `offset_m`

Current profiles:

- `xiao_nrf54l15_pair_20cm.json`
- `xiao_nrf54l15_pair_20cm.h`

Boundary:

- these are local measured profiles, not universal factory constants
- they are appropriate only for the named board pair, antenna path, and similar
  bench setup
- they do not replace the still-pending board-specific antenna-delay
  characterization work
