#ifndef MBEDTLS_SSL_H
#define MBEDTLS_SSL_H

#include <stddef.h>
#include <stdint.h>

#include "net_sockets.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBEDTLS_SSL_VERIFY_NONE 0
#define MBEDTLS_SSL_VERIFY_OPTIONAL 1
#define MBEDTLS_SSL_VERIFY_REQUIRED 2
#define MBEDTLS_SSL_IS_CLIENT 0
#define MBEDTLS_SSL_IS_SERVER 1
#define MBEDTLS_SSL_PRESET_DEFAULT 0
#define MBEDTLS_SSL_TRANSPORT_DATAGRAM 1
#define MBEDTLS_SSL_MAX_CONTENT_LEN 16384

#define MBEDTLS_ERR_SSL_TIMEOUT -0x6800
#define MBEDTLS_ERR_SSL_WANT_READ -0x6880
#define MBEDTLS_ERR_SSL_WANT_WRITE -0x6900
#define MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY -0x7880

typedef struct mbedtls_ssl_context mbedtls_ssl_context;
typedef struct mbedtls_ssl_config mbedtls_ssl_config;
typedef struct mbedtls_ssl_session mbedtls_ssl_session;
typedef struct mbedtls_x509_crt mbedtls_x509_crt;
typedef struct mbedtls_pk_context mbedtls_pk_context;
typedef struct mbedtls_ctr_drbg_context mbedtls_ctr_drbg_context;
typedef struct mbedtls_entropy_context mbedtls_entropy_context;

typedef int (*mbedtls_ssl_send_t)(void*, const unsigned char*, size_t);
typedef int (*mbedtls_ssl_recv_t)(void*, unsigned char*, size_t);
typedef int (*mbedtls_ssl_recv_timeout_t)(void*, unsigned char*, size_t, uint32_t);

struct mbedtls_ssl_config {
  int endpoint;
  int transport;
  int authmode;
  void* p_ biodata;
  mbedtls_ssl_send_t f_send;
  mbedtls_ssl_recv_t f_recv;
  mbedtls_ssl_recv_timeout_t f_recv_timeout;
  mbedtls_x509_crt* ca_chain;
  mbedtls_x509_crt* own_cert;
  mbedtls_pk_context* pk_ctx;
  void* psk_identity;
  size_t psk_identity_len;
  const unsigned char* psk;
  size_t psk_len;
};

struct mbedtls_ssl_context {
  mbedtls_ssl_config* conf;
  int state;
  int major_ver;
  int minor_ver;
  void* handshake;
  void* transform_in;
  void* transform_out;
  void* session_in;
  void* session_out;
  void* p_bio;
};

static inline void mbedtls_ssl_init(mbedtls_ssl_context* ssl) { (void)ssl; }
static inline void mbedtls_ssl_config_init(mbedtls_ssl_config* conf) { (void)conf; }
static inline int mbedtls_ssl_config_defaults(mbedtls_ssl_config* conf, int endpoint,
                                              int transport, int preset) {
  (void)conf; (void)endpoint; (void)transport; (void)preset; return 0;
}
static inline int mbedtls_ssl_setup(mbedtls_ssl_context* ssl,
                                    const mbedtls_ssl_config* conf) {
  (void)ssl; (void)conf; return 0;
}
static inline void mbedtls_ssl_set_bio(mbedtls_ssl_context* ssl, void* p_bio,
                                       mbedtls_ssl_send_t f_send,
                                       mbedtls_ssl_recv_t f_recv,
                                       mbedtls_ssl_recv_timeout_t f_recv_timeout) {
  (void)ssl; (void)p_bio; (void)f_send; (void)f_recv; (void)f_recv_timeout;
}
static inline void mbedtls_ssl_conf_authmode(mbedtls_ssl_config* conf, int authmode) {
  (void)conf; (void)authmode;
}
static inline int mbedtls_ssl_set_hs_ecjpake_password(
    mbedtls_ssl_context* ssl, const unsigned char* pw, size_t pw_len) {
  (void)ssl; (void)pw; (void)pw_len; return 0;
}
static inline int mbedtls_ssl_set_hs_own_cert(mbedtls_ssl_context* ssl,
                                              mbedtls_x509_crt* own_cert,
                                              mbedtls_pk_context* pk_key) {
  (void)ssl; (void)own_cert; (void)pk_key; return 0;
}
static inline int mbedtls_ssl_set_hs_ca_chain(mbedtls_ssl_context* ssl,
                                              mbedtls_x509_crt* ca_chain,
                                              mbedtls_x509_crt* crl) {
  (void)ssl; (void)ca_chain; (void)crl; return 0;
}
static inline int mbedtls_ssl_handshake(mbedtls_ssl_context* ssl) {
  (void)ssl; return 0;
}
static inline int mbedtls_ssl_read(mbedtls_ssl_context* ssl, unsigned char* buf, size_t len) {
  (void)ssl; (void)buf; (void)len; return -1;
}
static inline int mbedtls_ssl_write(mbedtls_ssl_context* ssl, const unsigned char* buf,
                                     size_t len) {
  (void)ssl; (void)buf; (void)len; return -1;
}
static inline int mbedtls_ssl_close_notify(mbedtls_ssl_context* ssl) {
  (void)ssl; return 0;
}
static inline void mbedtls_ssl_free(mbedtls_ssl_context* ssl) { (void)ssl; }
static inline void mbedtls_ssl_config_free(mbedtls_ssl_config* conf) { (void)conf; }
static inline int mbedtls_ssl_get_verify_result(const mbedtls_ssl_context* ssl) {
  (void)ssl; return 0;
}
static inline const char* mbedtls_ssl_get_version(const mbedtls_ssl_context* ssl) {
  (void)ssl; return "TLSv1.2";
}
static inline const char* mbedtls_ssl_get_ciphersuite(const mbedtls_ssl_context* ssl) {
  (void)ssl; return "TLS-ECJPAKE-WITH-AES-128-CCM-8";
}
static inline int mbedtls_ssl_export_keys(mbedtls_ssl_context* ssl,
                                          const char* label, size_t label_len,
                                          const unsigned char* context, size_t context_len,
                                          int use_context,
                                          unsigned char* keyblock, size_t keyblock_len,
                                          size_t* olen) {
  (void)ssl; (void)label; (void)label_len; (void)context; (void)context_len;
  (void)use_context; (void)keyblock; (void)keyblock_len; (void)olen; return 0;
}

#ifdef __cplusplus
}
#endif

#endif
