#ifndef MBEDTLS_CTR_DRBG_H
#define MBEDTLS_CTR_DRBG_H
#include <stddef.h>
typedef struct mbedtls_ctr_drbg_context { int dummy; } mbedtls_ctr_drbg_context;
static inline void mbedtls_ctr_drbg_init(mbedtls_ctr_drbg_context* ctx) { (void)ctx; }
static inline void mbedtls_ctr_drbg_free(mbedtls_ctr_drbg_context* ctx) { (void)ctx; }
#endif
