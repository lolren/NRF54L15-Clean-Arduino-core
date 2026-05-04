#pragma once
#include <stddef.h>
#include <stdint.h>

namespace xiao_nrf54l15 {

struct Secp256r1Point { uint8_t x[32]; uint8_t y[32]; };
struct Secp256r1Scalar { uint8_t bytes[32]; };
struct Secp256r1Jacobian { uint32_t X[8]; uint32_t Y[8]; uint32_t Z[8]; };

class Secp256r1 {
 public:
  static constexpr size_t kCoordinateSize = 32U;
  static constexpr size_t kPointUncompressedSize = 65U;
  static constexpr size_t kScalarSize = 32U;

  struct BigNum256 { uint32_t w[8] = {0}; };

  // Curve constants
  static BigNum256 primeP();
  static BigNum256 orderN();
  static BigNum256 curveB();

  // Big number helpers
  static void bnFromBytes(const uint8_t bytes[32], BigNum256* out);
  static void bnToBytes(const BigNum256& bn, uint8_t bytes[32]);
  static void bnSetZero(BigNum256* out);
  static void bnSetOne(BigNum256* out);
  static bool bnIsZero(const BigNum256& a);
  static bool bnIsOne(const BigNum256& a);
  static bool bnEquals(const BigNum256& a, const BigNum256& b);
  static int bnCompare(const BigNum256& a, const BigNum256& b);
  static void bnAdd(const BigNum256& a, const BigNum256& b, BigNum256* out);
  static void bnSub(const BigNum256& a, const BigNum256& b, BigNum256* out);
  static void bnMul(const BigNum256& a, const BigNum256& b, BigNum256* out);
  static void bnModAdd(const BigNum256& a, const BigNum256& b, BigNum256* out);
  static void bnModSub(const BigNum256& a, const BigNum256& b, BigNum256* out);
  static void bnModMul(const BigNum256& a, const BigNum256& b, BigNum256* out);
  static void bnModSqr(const BigNum256& a, BigNum256* out);
  static void bnModInv(const BigNum256& a, BigNum256* out);
  static void bnModMulN(const BigNum256& a, const BigNum256& b, BigNum256* out);
  static void bnModAddN(const BigNum256& a, const BigNum256& b, BigNum256* out);

  // Point operations
  static void generateRandomScalar(Secp256r1Scalar* outScalar);
  static bool scalarMultiply(const Secp256r1Scalar& k, const Secp256r1Point& P, Secp256r1Point* outR);
  static bool scalarMultiplyBase(const Secp256r1Scalar& k, Secp256r1Point* outR);
  static bool pointAdd(const Secp256r1Point& P, const Secp256r1Point& Q, Secp256r1Point* outR);
  static bool pointDouble(const Secp256r1Point& P, Secp256r1Point* outR);
  static bool isOnCurve(const Secp256r1Point& point);
  static bool isInfinity(const Secp256r1Point& point);
  static void encodeUncompressed(const Secp256r1Point& point, uint8_t outBytes[65]);
  static bool decodeUncompressed(const uint8_t bytes[65], Secp256r1Point* outPoint);
  static void setBasePoint(Secp256r1Point* outPoint);
  static void setInfinity(Secp256r1Point* outPoint);
  static void copyPoint(const Secp256r1Point& src, Secp256r1Point* dst);
  static void getOrder(Secp256r1Scalar* outN);
  static bool modInverseN(const Secp256r1Scalar& a, Secp256r1Scalar* outInv);

  // ECDSA
  static void generateKeyPair(Secp256r1Scalar* outPriv, Secp256r1Point* outPub);
  static bool ecdsaSign(const Secp256r1Scalar& priv, const uint8_t hash[32], uint8_t r[32], uint8_t s[32]);
  static bool ecdsaVerify(const Secp256r1Point& pub, const uint8_t hash[32], const uint8_t r[32], const uint8_t s[32]);

 private:
  static BigNum256 basePointX();
  static BigNum256 basePointY();
  static void bnToMont(const BigNum256& x, BigNum256* out);
  static void bnFromMont(const BigNum256& x, BigNum256* out);
  static void randomBytes(uint8_t* out, size_t length);
  static uint32_t randomWord();
  static void pointToAffine(const BigNum256& x, const BigNum256& y, Secp256r1Point* out);
  static void pointFromAffine(const Secp256r1Point& point, BigNum256* outX, BigNum256* outY);
  
  // Jacobian coordinate operations (no inversion until final affine conversion)
  static void jacobianDouble(const Secp256r1Jacobian& P, Secp256r1Jacobian* out);
  static void jacobianAddMixed(const Secp256r1Jacobian& P, const BigNum256& Qx, const BigNum256& Qy, Secp256r1Jacobian* out);
  static void jacobianToAffine(const Secp256r1Jacobian& P, BigNum256* outX, BigNum256* outY);
  static void affineToJacobian(const BigNum256& x, const BigNum256& y, Secp256r1Jacobian* out);
};

}  // namespace xiao_nrf54l15
