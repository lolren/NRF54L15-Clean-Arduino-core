#ifndef MBEDTLS_PK_H
#define MBEDTLS_PK_H
#include <stddef.h>
#include "ecdsa.h"
typedef enum { MBEDTLS_PK_ECKEY = 3 } mbedtls_pk_type_t;
typedef struct mbedtls_pk_info_t { mbedtls_pk_type_t type; const char* name; } mbedtls_pk_info_t;
typedef struct mbedtls_pk_context { const mbedtls_pk_info_t* pk_info; void* pk_ctx; } mbedtls_pk_context;
static inline void mbedtls_pk_init(mbedtls_pk_context* ctx) { (void)ctx; }
static inline void mbedtls_pk_free(mbedtls_pk_context* ctx) { (void)ctx; }
static inline int mbedtls_pk_setup(mbedtls_pk_context* ctx, const mbedtls_pk_info_t* info) { (void)ctx; (void)info; return 0; }
static inline const mbedtls_pk_info_t* mbedtls_pk_info_from_type(mbedtls_pk_type_t type) { (void)type; return NULL; }
#endif
