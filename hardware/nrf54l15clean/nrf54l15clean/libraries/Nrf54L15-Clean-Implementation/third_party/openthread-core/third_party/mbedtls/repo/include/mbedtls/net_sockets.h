#ifndef MBEDTLS_NET_SOCKETS_H
#define MBEDTLS_NET_SOCKETS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbedtls_net_context {
  int fd;
} mbedtls_net_context;

typedef enum {
  MBEDTLS_NET_PROTO_TCP = 0,
  MBEDTLS_NET_PROTO_UDP = 1,
} mbedtls_net_proto;

static inline void mbedtls_net_init(mbedtls_net_context* ctx) { (void)ctx; }
static inline int mbedtls_net_connect(mbedtls_net_context* ctx, const char* host,
                                      const char* port, int proto) {
  (void)ctx; (void)host; (void)port; (void)proto; return -1;
}
static inline int mbedtls_net_bind(mbedtls_net_context* ctx, const char* bind_ip,
                                   const char* port, int proto) {
  (void)ctx; (void)bind_ip; (void)port; (void)proto; return -1;
}
static inline int mbedtls_net_accept(mbedtls_net_context* bind_ctx,
                                     mbedtls_net_context* client_ctx,
                                     void* client_ip, size_t buf_size,
                                     size_t* ip_len) {
  (void)bind_ctx; (void)client_ctx; (void)client_ip; (void)buf_size; (void)ip_len; return -1;
}
static inline int mbedtls_net_set_block(mbedtls_net_context* ctx) { (void)ctx; return -1; }
static inline int mbedtls_net_set_nonblock(mbedtls_net_context* ctx) { (void)ctx; return -1; }
static inline int mbedtls_net_recv(void* ctx, unsigned char* buf, size_t len) {
  (void)ctx; (void)buf; (void)len; return -1;
}
static inline int mbedtls_net_send(void* ctx, const unsigned char* buf, size_t len) {
  (void)ctx; (void)buf; (void)len; return -1;
}
static inline int mbedtls_net_recv_timeout(void* ctx, unsigned char* buf, size_t len,
                                           uint32_t timeout) {
  (void)ctx; (void)buf; (void)len; (void)timeout; return -1;
}
static inline void mbedtls_net_free(mbedtls_net_context* ctx) { (void)ctx; }
static inline int mbedtls_net_poll(mbedtls_net_context* ctx, uint32_t rw, uint32_t timeout) {
  (void)ctx; (void)rw; (void)timeout; return -1;
}

#ifdef __cplusplus
}
#endif

#endif
