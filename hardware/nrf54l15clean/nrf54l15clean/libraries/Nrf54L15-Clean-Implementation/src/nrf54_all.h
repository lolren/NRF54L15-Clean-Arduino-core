// nRF54L15 Arduino Core — Single Include Header
//
// Include this single header to access all nRF54L15 features:
//   Thread, Matter, BLE, Zigbee, ECC, Crypto, and Hardware HAL.
//
// Usage:
//   #include <nrf54_all.h>
//
// This replaces:
//   #include <nrf54_thread_experimental.h>
//   #include <matter_secp256r1.h>
//   #include <matter_pbkdf2.h>
//   #include <nrf54l15_hal.h>
//   ...etc

#pragma once

// Arduino core (auto-included by IDE, explicit for safety)
#include <Arduino.h>

// Hardware Abstraction Layer
#include "nrf54l15_hal.h"

// Thread (OpenThread staged)
#include "nrf54_thread_experimental.h"

// Matter Crypto
#include "matter_secp256r1.h"
#include "matter_pbkdf2.h"

// Matter PASE
#include "matter_pase_commissioning.h"

// Matter CASE
#include "matter_case_session.h"

// Matter Platform
#include "matter_platform_stage.h"

// VPR Softperipheral (RISC-V coprocessor)
#include "nrf54l15_vpr.h"
#include "vpr_softperipheral_manager.h"
#include "vpr_sqspi.h"
