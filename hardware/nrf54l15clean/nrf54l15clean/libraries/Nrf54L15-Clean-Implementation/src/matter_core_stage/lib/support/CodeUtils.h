#pragma once

// Minimal Matter core-stage shim for the first hidden Arduino bring-up of
// upstream CHIP units that only need basic VerifyOrReturn* helpers.

#define VerifyOrDie(expr)       \
  do {                          \
    if (!(expr)) {              \
      __builtin_trap();         \
    }                           \
  } while (false)

#define VerifyOrReturnValue(expr, code, ...) \
  do {                                       \
    if (!(expr)) {                           \
      __VA_ARGS__;                           \
      return code;                           \
    }                                        \
  } while (false)

#define VerifyOrReturnError(expr, code, ...) \
  VerifyOrReturnValue(expr, code, ##__VA_ARGS__)

#define ReturnErrorOnFailure(expr)          \
  do {                                      \
    const CHIP_ERROR __err = (expr);        \
    if (__err != CHIP_NO_ERROR) {           \
      return __err;                         \
    }                                       \
  } while (false)
