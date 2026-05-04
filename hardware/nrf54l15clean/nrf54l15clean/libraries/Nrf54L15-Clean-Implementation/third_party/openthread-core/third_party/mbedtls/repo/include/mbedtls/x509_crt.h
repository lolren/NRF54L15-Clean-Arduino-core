#ifndef MBEDTLS_X509_CRT_H
#define MBEDTLS_X509_CRT_H
#include <stddef.h>
#include <stdint.h>
typedef struct mbedtls_x509_crt { int dummy; } mbedtls_x509_crt;
static inline void mbedtls_x509_crt_init(mbedtls_x509_crt* crt) { (void)crt; }
static inline void mbedtls_x509_crt_free(mbedtls_x509_crt* crt) { (void)crt; }
#endif
