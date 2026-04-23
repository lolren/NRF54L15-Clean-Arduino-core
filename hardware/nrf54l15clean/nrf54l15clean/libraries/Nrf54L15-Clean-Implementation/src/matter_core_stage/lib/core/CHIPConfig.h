#pragma once

// Minimal Matter core-stage config seam for the first hidden Arduino bring-up
// of upstream CHIP core error formatting. This is intentionally narrow and
// does not claim a general-purpose connectedhomeip config surface yet.

#ifndef CHIP_HAVE_CONFIG_H
#define CHIP_HAVE_CONFIG_H 0
#endif

#ifndef CHIP_CONFIG_ERROR_FORMAT_AS_STRING
#define CHIP_CONFIG_ERROR_FORMAT_AS_STRING 1
#endif

#ifndef CHIP_CONFIG_ERROR_SOURCE
#define CHIP_CONFIG_ERROR_SOURCE 0
#endif

#ifndef CHIP_CONFIG_ERROR_STD_SOURCE_LOCATION
#define CHIP_CONFIG_ERROR_STD_SOURCE_LOCATION 0
#endif

#ifndef CHIP_CONFIG_ERROR_SOURCE_NO_ERROR
#define CHIP_CONFIG_ERROR_SOURCE_NO_ERROR 0
#endif

#ifndef CHIP_CONFIG_ERROR_STR_SIZE
#define CHIP_CONFIG_ERROR_STR_SIZE 160
#endif

#ifndef CHIP_CONFIG_SHORT_ERROR_STR
#define CHIP_CONFIG_SHORT_ERROR_STR 0
#endif

#ifndef CHIP_CONFIG_CUSTOM_ERROR_FORMATTER
#define CHIP_CONFIG_CUSTOM_ERROR_FORMATTER 0
#endif

#ifndef CHIP_SYSTEM_CONFIG_THREAD_LOCAL_STORAGE
#define CHIP_SYSTEM_CONFIG_THREAD_LOCAL_STORAGE 0
#endif
