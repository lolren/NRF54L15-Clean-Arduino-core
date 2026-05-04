#ifndef MBEDTLS_SSL_COOKIE_H
#define MBEDTLS_SSL_COOKIE_H
#include <stddef.h>
typedef struct mbedtls_ssl_cookie_ctx { int dummy; } mbedtls_ssl_cookie_ctx;
static inline void mbedtls_ssl_cookie_init(mbedtls_ssl_cookie_ctx* ctx) { (void)ctx; }
static inline void mbedtls_ssl_cookie_free(mbedtls_ssl_cookie_ctx* ctx) { (void)ctx; }
#endif
