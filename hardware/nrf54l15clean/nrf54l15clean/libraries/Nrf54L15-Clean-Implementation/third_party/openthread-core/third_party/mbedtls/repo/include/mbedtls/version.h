#ifndef MBEDTLS_VERSION_H
#define MBEDTLS_VERSION_H

#define MBEDTLS_VERSION_MAJOR 2
#define MBEDTLS_VERSION_MINOR 28
#define MBEDTLS_VERSION_PATCH 0
#define MBEDTLS_VERSION_NUMBER 0x021C0000
#define MBEDTLS_VERSION_STRING "2.28.0"
#define MBEDTLS_VERSION_STRING_FULL "mbed TLS 2.28.0"

static inline unsigned int mbedtls_version_get_number(void) { return MBEDTLS_VERSION_NUMBER; }
static inline void mbedtls_version_get_string(char* string) {
  if (string) { string[0] = MBEDTLS_VERSION_STRING[0]; string[1] = '\0'; }
}
static inline void mbedtls_version_get_string_full(char* string) {
  if (string) { string[0] = MBEDTLS_VERSION_STRING_FULL[0]; string[1] = '\0'; }
}

#endif
