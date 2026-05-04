#ifndef MBEDTLS_ECDSA_H
#define MBEDTLS_ECDSA_H
#include <stddef.h>
#include <stdint.h>
typedef struct mbedtls_ecdsa_context { int dummy; } mbedtls_ecdsa_context;
static inline void mbedtls_ecdsa_init(mbedtls_ecdsa_context* ctx) { (void)ctx; }
static inline void mbedtls_ecdsa_free(mbedtls_ecdsa_context* ctx) { (void)ctx; }
static inline int mbedtls_ecdsa_from_keypair(mbedtls_ecdsa_context* ctx, const void* key) { (void)ctx; (void)key; return 0; }
#endif
