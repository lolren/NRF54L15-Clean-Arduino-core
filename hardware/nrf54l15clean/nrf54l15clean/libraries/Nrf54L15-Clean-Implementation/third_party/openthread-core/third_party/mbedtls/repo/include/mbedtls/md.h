#ifndef MBEDTLS_MD_H
#define MBEDTLS_MD_H
#include <stddef.h>
#include <stdint.h>
typedef enum { MBEDTLS_MD_SHA256 = 4 } mbedtls_md_type_t;
typedef struct mbedtls_md_info_t { mbedtls_md_type_t type; const char* name; int size; } mbedtls_md_info_t;
typedef struct mbedtls_md_context_t { const mbedtls_md_info_t* md_info; void* md_ctx; void* hmac_ctx; } mbedtls_md_context_t;
static inline const mbedtls_md_info_t* mbedtls_md_info_from_type(mbedtls_md_type_t type) { (void)type; return NULL; }
static inline int mbedtls_md_setup(mbedtls_md_context_t* ctx, const mbedtls_md_info_t* md_info, int hmac) { (void)ctx; (void)md_info; (void)hmac; return 0; }
static inline int mbedtls_md_hmac_starts(mbedtls_md_context_t* ctx, const unsigned char* key, size_t keylen) { (void)ctx; (void)key; (void)keylen; return 0; }
static inline int mbedtls_md_hmac_update(mbedtls_md_context_t* ctx, const unsigned char* input, size_t ilen) { (void)ctx; (void)input; (void)ilen; return 0; }
static inline int mbedtls_md_hmac_finish(mbedtls_md_context_t* ctx, unsigned char* output) { (void)ctx; (void)output; return 0; }
static inline void mbedtls_md_free(mbedtls_md_context_t* ctx) { (void)ctx; }
static inline int mbedtls_md_get_size(const mbedtls_md_info_t* md_info) { return md_info ? md_info->size : 0; }
#endif
