#ifndef MBEDTLS_ECP_H
#define MBEDTLS_ECP_H
#include <stddef.h>
#include <stdint.h>
typedef struct mbedtls_ecp_group { uint16_t id; size_t pbits; } mbedtls_ecp_group;
typedef struct mbedtls_ecp_point { uint8_t X[65]; uint8_t Y[65]; uint8_t Z[65]; } mbedtls_ecp_point;
typedef struct mbedtls_ecp_keypair { mbedtls_ecp_group grp; mbedtls_ecp_point Q; uint8_t d[32]; } mbedtls_ecp_keypair;
#define MBEDTLS_ECP_DP_SECP256R1 23
static inline void mbedtls_ecp_keypair_init(mbedtls_ecp_keypair* kp) { (void)kp; }
static inline void mbedtls_ecp_keypair_free(mbedtls_ecp_keypair* kp) { (void)kp; }
#endif
