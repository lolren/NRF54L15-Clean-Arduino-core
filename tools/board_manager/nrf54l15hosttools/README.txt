Nrf54L15 host tools

This package is downloaded automatically through Arduino Boards Manager.

What it provides:
- pinned pyOCD bootstrap requirements for the advanced recovery uploader
- bundled offline wheelhouses for common host Python versions
- Linux and Windows helper scripts for the host-side setup path
- the CMSIS-DAP udev rule for XIAO nRF54L15 on Linux

Normal compile and default upload should work without this package being used
directly. It exists so recovery and protected-target workflows do not depend on
manually locating setup files in the repository.

Current offline wheelhouse coverage:
- CPython `3.10`, `3.11`, and `3.12` on the packaged host targets
- if the local Python version is outside that set, the helper falls back to the
  normal online `pip` install path
