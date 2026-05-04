#ifndef MBEDTLS_ENTROPY_H
#define MBEDTLS_ENTROPY_H
#include <stddef.h>
typedef struct mbedtls_entropy_context { int dummy; } mbedtls_entropy_context;
static inline void mbedtls_entropy_init(mbedtls_entropy_context* ctx) { (void)ctx; }
static inline void mbedtls_entropy_free(mbedtls_entropy_context* ctx) { (void)ctx; }
#endif
