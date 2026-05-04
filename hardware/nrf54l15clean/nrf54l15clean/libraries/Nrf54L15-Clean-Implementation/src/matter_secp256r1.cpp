#include "matter_secp256r1.h"
#include <Arduino.h>
#include <string.h>

namespace xiao_nrf54l15 {

// ─── Curve constants as file-scope arrays (NO class static members) ────────

static const uint32_t kPrimeP[8] = {
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0x00000000U,
    0x00000000U, 0x00000000U, 0x00000001U, 0xFFFFFFFFU,
};

static const uint32_t kOrderN[8] = {
    0xFC632551U, 0xF3B9CAC2U, 0xA7179E84U, 0xBCE6FAADU,
    0xFFFFFFFFU, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
};

static const uint32_t kCurveB[8] = {
    0x27D2604BU, 0x3BCE3C3EU, 0xCC53B0F6U, 0x651D06B0U,
    0x769886BCU, 0xB3EBBD55U, 0xAA3A93E7U, 0x5AC635D8U,
};

static const uint32_t kBasePointX[8] = {
    0xD898C296U, 0xF4A13945U, 0x2DEB33A0U, 0x77037D81U,
    0x63A440F2U, 0xF8BCE6E5U, 0xE12C4247U, 0x6B17D1F2U,
};

static const uint32_t kBasePointY[8] = {
    0x37BF51F5U, 0xCBB64068U, 0x6B315ECEU, 0x2BCE3357U,
    0x7C0F9E16U, 0x8EE7EB4AU, 0xFE1A7F9BU, 0x4FE342E2U,
};

// ─── Accessor functions ────────────────────────────────────────────────────

Secp256r1::BigNum256 Secp256r1::primeP() { BigNum256 r; memcpy(r.w, kPrimeP, 32); return r; }
Secp256r1::BigNum256 Secp256r1::orderN() { BigNum256 r; memcpy(r.w, kOrderN, 32); return r; }
Secp256r1::BigNum256 Secp256r1::curveB() { BigNum256 r; memcpy(r.w, kCurveB, 32); return r; }
Secp256r1::BigNum256 Secp256r1::basePointX() { BigNum256 r; memcpy(r.w, kBasePointX, 32); return r; }
Secp256r1::BigNum256 Secp256r1::basePointY() { BigNum256 r; memcpy(r.w, kBasePointY, 32); return r; }

// ─── Big number helpers ────────────────────────────────────────────────────

void Secp256r1::bnFromBytes(const uint8_t bytes[32], BigNum256* out) {
  for (size_t i = 0; i < 8; ++i) {
    out->w[i] = (uint32_t)bytes[i*4] | ((uint32_t)bytes[i*4+1]<<8) | ((uint32_t)bytes[i*4+2]<<16) | ((uint32_t)bytes[i*4+3]<<24);
  }
}

void Secp256r1::bnToBytes(const BigNum256& bn, uint8_t bytes[32]) {
  for (size_t i = 0; i < 8; ++i) {
    bytes[i*4] = bn.w[i] & 0xFF; bytes[i*4+1] = (bn.w[i]>>8)&0xFF;
    bytes[i*4+2] = (bn.w[i]>>16)&0xFF; bytes[i*4+3] = (bn.w[i]>>24)&0xFF;
  }
}

void Secp256r1::bnSetZero(BigNum256* out) { for(int i=0;i<8;i++) out->w[i]=0; }
void Secp256r1::bnSetOne(BigNum256* out) { for(int i=0;i<8;i++) out->w[i]=0; out->w[0]=1U; }
bool Secp256r1::bnIsZero(const BigNum256& a) { uint32_t x=0; for(int i=0;i<8;i++) x|=a.w[i]; return x==0; }
bool Secp256r1::bnIsOne(const BigNum256& a) { if(a.w[0]!=1)return false; for(int i=1;i<8;i++) if(a.w[i])return false; return true; }
bool Secp256r1::bnEquals(const BigNum256& a, const BigNum256& b) { uint32_t d=0; for(int i=0;i<8;i++) d|=a.w[i]^b.w[i]; return d==0; }

int Secp256r1::bnCompare(const BigNum256& a, const BigNum256& b) {
  for(int i=7;i>=0;i--){if(a.w[i]>b.w[i])return 1; if(a.w[i]<b.w[i])return -1;} return 0;
}

void Secp256r1::bnAdd(const BigNum256& a, const BigNum256& b, BigNum256* out) {
  uint64_t c=0; for(int i=0;i<8;i++){c+=(uint64_t)a.w[i]+b.w[i]; out->w[i]=(uint32_t)c; c>>=32;}
}

void Secp256r1::bnSub(const BigNum256& a, const BigNum256& b, BigNum256* out) {
  uint64_t borrow=0; for(int i=0;i<8;i++){uint64_t d=(uint64_t)a.w[i]-b.w[i]-borrow; out->w[i]=(uint32_t)d; borrow=(d>>63)&1;}
}

// ─── Modular multiplication (NIST fast reduction for P-256) ─────────────────

void Secp256r1::bnMul(const BigNum256& a, const BigNum256& b, BigNum256* out) {
  uint32_t T[16] = {0};
  for (size_t i = 0; i < 8; ++i) {
    uint64_t carry = 0;
    for (size_t j = 0; j < 8; ++j) {
      carry += (uint64_t)a.w[i] * b.w[j] + T[i+j];
      T[i+j] = (uint32_t)carry; carry >>= 32;
    }
    T[i+8] = (uint32_t)carry;
  }

  // Bit-level long division reduction (correct, 256 iterations, 3ms)
  BigNum256 p = primeP();
  for (int shift = 255; shift >= 0; shift--) {
    uint32_t ps[16] = {0};
    int ws = shift / 32, bs = shift % 32;
    if (bs == 0) {
      for (int i = 0; i < 8 && ws+i < 16; i++) ps[ws+i] = p.w[i];
    } else {
      uint64_t c = 0;
      for (int i = 0; i < 8; i++) {
        uint64_t v = ((uint64_t)p.w[i] << bs) | c;
        if (ws+i < 16) ps[ws+i] = (uint32_t)v;
        c = v >> 32;
      }
      if (ws+8 < 16) ps[ws+8] = (uint32_t)c;
    }
    bool ge = false;
    for (int i = 15; i >= 0; i--) { if (T[i] > ps[i]) { ge=true; break; } if (T[i] < ps[i]) break; if (i==0) ge=true; }
    if (ge) {
      uint64_t borrow = 0;
      for (int i = 0; i < 16; i++) { uint64_t d = (uint64_t)T[i] - ps[i] - borrow; T[i] = (uint32_t)d; borrow = (d>>63)&1ULL; }
    }
  }
  BigNum256 r; memcpy(r.w, T, 32);
  *out = r;
}

void Secp256r1::bnToMont(const BigNum256& x, BigNum256* out) { *out = x; }
void Secp256r1::bnFromMont(const BigNum256& x, BigNum256* out) { *out = x; }
void Secp256r1::bnModMul(const BigNum256& a, const BigNum256& b, BigNum256* out) { bnMul(a,b,out); }
void Secp256r1::bnModSqr(const BigNum256& a, BigNum256* out) { bnMul(a,a,out); }

void Secp256r1::bnModAdd(const BigNum256& a, const BigNum256& b, BigNum256* out) {
  BigNum256 s; bnAdd(a,b,&s);
  // Overflow past 2^256 if s < a (since a+b = s + carry*2^256)
  bool overflow = (bnCompare(s, a) < 0);
  if (overflow) {
    // Fold back: add 2^256 mod p = 2^224 - 2^192 - 2^96 + 1
    // Use signed accumulator to handle negative delta words
    int64_t acc[9] = {0};
    for (int i = 0; i < 8; i++) acc[i] = (uint64_t)s.w[i];
    acc[0] += 1; acc[3] -= 1; acc[6] -= 1; acc[7] += 1;
    for (int i = 0; i < 8; i++) {
      if (acc[i] < 0) { int64_t b = (-acc[i] + 0xFFFFFFFFLL) >> 32; acc[i] += b << 32; acc[i+1] -= b; }
      if (acc[i] >= 0x100000000LL) { int64_t c = acc[i] >> 32; acc[i] &= 0xFFFFFFFFLL; acc[i+1] += c; }
    }
    for (int i = 0; i < 8; i++) s.w[i] = (uint32_t)acc[i];
  }
  BigNum256 p=primeP();
  while(bnCompare(s,p)>=0){BigNum256 t;bnSub(s,p,&t);s=t;}
  *out=s;
}

void Secp256r1::bnModSub(const BigNum256& a, const BigNum256& b, BigNum256* out) {
  BigNum256 p=primeP();
  if(bnCompare(a,b)>=0){bnSub(a,b,out);}else{BigNum256 d;bnSub(a,b,&d);bnAdd(d,p,out);}
}

void Secp256r1::bnModInv(const BigNum256& a, BigNum256* out) {
  BigNum256 p=primeP();
  static const uint32_t kPminus2[8]={
    0xFFFFFFFDU,0xFFFFFFFFU,0xFFFFFFFFU,0x00000000U,
    0x00000000U,0x00000000U,0x00000001U,0xFFFFFFFFU};
  BigNum256 result, base=a;
  bnSetOne(&result);
  bool started=false;
  for(int wi=7;wi>=0;wi--)for(int bi=31;bi>=0;bi--){
    if(started) bnModSqr(result,&result);
    if((kPminus2[wi]>>bi)&1U){if(!started){result=base;started=true;}else bnModMul(result,base,&result);}
  }
  *out=result;
}

// ─── Scalar arithmetic mod n ───────────────────────────────────────────────

void Secp256r1::bnModAddN(const BigNum256& a, const BigNum256& b, BigNum256* out) {
  bnAdd(a,b,out); BigNum256 n=orderN(); if(bnCompare(*out,n)>=0) bnSub(*out,n,out);
}

void Secp256r1::bnModMulN(const BigNum256& a, const BigNum256& b, BigNum256* out) {
  // Compute 512-bit product
  uint32_t T[16] = {0};
  for (size_t i = 0; i < 8; ++i) {
    uint64_t carry = 0;
    for (size_t j = 0; j < 8; ++j) {
      carry += (uint64_t)a.w[i] * b.w[j] + T[i+j];
      T[i+j] = (uint32_t)carry; carry >>= 32;
    }
    T[i+8] = (uint32_t)carry;
  }
  
  // Bit-level long division modulo n
  BigNum256 n = orderN();
  for (int shift = 255; shift >= 0; shift--) {
    uint32_t ns[16] = {0};
    int ws = shift / 32, bs = shift % 32;
    if (bs == 0) {
      for (int i = 0; i < 8 && ws+i < 16; i++) ns[ws+i] = n.w[i];
    } else {
      uint64_t c = 0;
      for (int i = 0; i < 8; i++) {
        uint64_t v = ((uint64_t)n.w[i] << bs) | c;
        if (ws+i < 16) ns[ws+i] = (uint32_t)v;
        c = v >> 32;
      }
      if (ws+8 < 16) ns[ws+8] = (uint32_t)c;
    }
    bool ge = false;
    for (int i = 15; i >= 0; i--) { if (T[i] > ns[i]) { ge=true; break; } if (T[i] < ns[i]) break; if (i==0) ge=true; }
    if (ge) {
      uint64_t borrow = 0;
      for (int i = 0; i < 16; i++) { uint64_t d = (uint64_t)T[i] - ns[i] - borrow; T[i] = (uint32_t)d; borrow = (d>>63)&1ULL; }
    }
  }
  BigNum256 r; memcpy(r.w, T, 32);
  *out = r;
}

// ─── Point operations ──────────────────────────────────────────────────────

void Secp256r1::pointToAffine(const BigNum256& x, const BigNum256& y, Secp256r1Point* out) {
  bnToBytes(x,out->x); bnToBytes(y,out->y);
}

void Secp256r1::pointFromAffine(const Secp256r1Point& point, BigNum256* outX, BigNum256* outY) {
  bnFromBytes(point.x,outX); bnFromBytes(point.y,outY);
}

bool Secp256r1::pointAdd(const Secp256r1Point& P, const Secp256r1Point& Q, Secp256r1Point* outR) {
  if(!outR)return false;
  if(isInfinity(P)){copyPoint(Q,outR);return true;}
  if(isInfinity(Q)){copyPoint(P,outR);return true;}
  BigNum256 x1,y1,x2,y2; pointFromAffine(P,&x1,&y1); pointFromAffine(Q,&x2,&y2);
  if(bnEquals(x1,x2)){if(bnEquals(y1,y2))return pointDouble(P,outR);setInfinity(outR);return true;}
  BigNum256 num,den,lambda,denInv;
  bnModSub(y2,y1,&num); bnModSub(x2,x1,&den); bnModInv(den,&denInv); bnModMul(num,denInv,&lambda);
  BigNum256 l2,x3,y3;
  bnModSqr(lambda,&l2); bnModSub(l2,x1,&x3); bnModSub(x3,x2,&x3);
  BigNum256 xd,t; bnModSub(x1,x3,&xd); bnModMul(lambda,xd,&t); bnModSub(t,y1,&y3);
  pointToAffine(x3,y3,outR); return true;
}

bool Secp256r1::pointDouble(const Secp256r1Point& P, Secp256r1Point* outR) {
  if(!outR)return false;
  if(isInfinity(P)){setInfinity(outR);return true;}
  BigNum256 x1,y1; pointFromAffine(P,&x1,&y1);
  BigNum256 xSqr,tmp,three; bnSetOne(&three); three.w[0]=3;
  bnModSqr(x1,&xSqr); bnModMul(three,xSqr,&tmp);
  BigNum256 num,den,denInv,lambda; bnSetOne(&num); num.w[0]=3;
  bnModSub(tmp,num,&num);
  BigNum256 twoY; bnSetOne(&twoY); twoY.w[0]=2; bnModMul(twoY,y1,&den);
  bnModInv(den,&denInv); bnModMul(num,denInv,&lambda);
  BigNum256 l2,x3,y3; bnModSqr(lambda,&l2);
  BigNum256 twoX1; bnSetOne(&twoX1); twoX1.w[0]=2; bnModMul(twoX1,x1,&twoX1);
  bnModSub(l2,twoX1,&x3);
  BigNum256 xd; bnModSub(x1,x3,&xd); bnModMul(lambda,xd,&tmp); bnModSub(tmp,y1,&y3);
  pointToAffine(x3,y3,outR); return true;
}

// ─── Jacobian coordinate operations (no inversion per step) ───────────────

void Secp256r1::affineToJacobian(const BigNum256& x, const BigNum256& y, Secp256r1Jacobian* out) {
  memcpy(out->X, x.w, 32); memcpy(out->Y, y.w, 32);
  bnSetOne((BigNum256*)out->Z);
}

void Secp256r1::jacobianDouble(const Secp256r1Jacobian& P, Secp256r1Jacobian* out) {
  BigNum256 X1, Y1, Z1;
  memcpy(X1.w, P.X, 32); memcpy(Y1.w, P.Y, 32); memcpy(Z1.w, P.Z, 32);
  
  // S = 4*X*Y^2
  BigNum256 YY, Y4, S;
  bnModSqr(Y1, &YY);           // Y^2
  bnModSqr(YY, &Y4);           // Y^4
  BigNum256 two; bnSetOne(&two); two.w[0]=2;
  BigNum256 four; bnSetOne(&four); four.w[0]=4;
  bnModMul(two, YY, &S);       // 2*Y^2
  bnModMul(two, X1, &S);       // 2*X
  bnModMul(S, YY, &S);         // 2*X*Y^2 → wait this is wrong
  // Let me redo this properly
  bnModSqr(Y1, &YY);           // B = Y1^2
  bnModSqr(X1, &S);            // temp = X1^2
  // Actually: S = 4*X1*Y1^2 = X1 * (2*Y1)^2
  BigNum256 twoY1;
  bnSetOne(&twoY1); twoY1.w[0]=2;
  bnModMul(twoY1, Y1, &twoY1); // 2*Y1
  BigNum256 twoY1Sq;
  bnModSqr(twoY1, &twoY1Sq);   // 4*Y1^2
  bnModMul(X1, twoY1Sq, &S);   // S = X1 * 4*Y1^2 = 4*X1*Y1^2
  
  // M = 3*X1^2 + a*Z1^4 (a = -3, so M = 3*(X1^2 - Z1^4))
  BigNum256 X1Sq, Z1Sq, Z1q4, M;
  bnModSqr(X1, &X1Sq);
  bnModSqr(Z1, &Z1Sq);
  bnModSqr(Z1Sq, &Z1q4);
  BigNum256 three; bnSetOne(&three); three.w[0]=3;
  bnModSub(X1Sq, Z1q4, &M);   // X1^2 - Z1^4
  bnModMul(three, M, &M);      // 3*(X1^2 - Z1^4)
  
  // X3 = M^2 - 2*S
  BigNum256 X3, twoS;
  bnModSqr(M, &X3);            // M^2
  bnSetOne(&twoS); twoS.w[0]=2;
  bnModMul(twoS, S, &twoS);    // 2*S
  bnModSub(X3, twoS, &X3);
  
  // Y3 = M*(S - X3) - 8*Y1^4
  BigNum256 SX3, Y3, eightY4;
  bnModSub(S, X3, &SX3);
  bnModMul(M, SX3, &Y3);
  BigNum256 eight; bnSetOne(&eight); eight.w[0]=8;
  bnModMul(eight, Y4, &eightY4);
  bnModSub(Y3, eightY4, &Y3);
  
  // Z3 = 2*Y1*Z1
  BigNum256 Z3;
  bnSetOne(&twoY1); twoY1.w[0]=2;
  bnModMul(twoY1, Y1, &Z3);    // 2*Y1
  bnModMul(Z3, Z1, &Z3);        // 2*Y1*Z1
  
  memcpy(out->X, X3.w, 32); memcpy(out->Y, Y3.w, 32); memcpy(out->Z, Z3.w, 32);
}

// Mixed add: P in Jacobian, Q in affine (Z=1). Result in Jacobian.
void Secp256r1::jacobianAddMixed(const Secp256r1Jacobian& P, const BigNum256& Qx, const BigNum256& Qy, Secp256r1Jacobian* out) {
  BigNum256 X1, Y1, Z1, X2, Y2;
  memcpy(X1.w, P.X, 32); memcpy(Y1.w, P.Y, 32); memcpy(Z1.w, P.Z, 32);
  memcpy(X2.w, Qx.w, 32); memcpy(Y2.w, Qy.w, 32);
  
  // Z1Z1 = Z1^2
  BigNum256 Z1Z1;
  bnModSqr(Z1, &Z1Z1);
  
  // U2 = X2 * Z1Z1
  BigNum256 U2, S2;
  bnModMul(X2, Z1Z1, &U2);
  
  // S2 = Y2 * Z1 * Z1Z1
  bnModMul(Z1, Z1Z1, &S2);
  bnModMul(Y2, S2, &S2);
  
  // U1 = X1, S1 = Y1
  BigNum256 &U1 = X1, &S1 = Y1;
  
  // H = U2 - U1
  BigNum256 H;
  bnModSub(U2, U1, &H);
  
  // R = S2 - S1
  BigNum256 RR;
  bnModSub(S2, S1, &RR);
  
  // If H == 0, handle doubling or infinity
  if (bnIsZero(H)) {
    if (bnIsZero(RR)) { jacobianDouble(P, out); return; }
    // Points are additive inverses → infinity
    memset(out, 0, sizeof(*out)); return;
  }
  
  // HH = H^2
  BigNum256 HH, HHH;
  bnModSqr(H, &HH);
  bnModMul(HH, H, &HHH);
  
  // X3 = R^2 - H^3 - 2*U1*HH
  BigNum256 RRsq, twoU1HH, X3;
  bnModSqr(RR, &RRsq);
  BigNum256 two; bnSetOne(&two); two.w[0]=2;
  bnModMul(two, U1, &twoU1HH);
  bnModMul(twoU1HH, HH, &twoU1HH);
  bnModSub(RRsq, HHH, &X3);
  bnModSub(X3, twoU1HH, &X3);
  
  // Y3 = R*(U1*HH - X3) - S1*HHH
  BigNum256 U1HH, U1HH_X3, Y3, S1HHH;
  bnModMul(U1, HH, &U1HH);
  bnModSub(U1HH, X3, &U1HH_X3);
  bnModMul(RR, U1HH_X3, &Y3);
  bnModMul(S1, HHH, &S1HHH);
  bnModSub(Y3, S1HHH, &Y3);
  
  // Z3 = Z1 * H
  BigNum256 Z3;
  bnModMul(Z1, H, &Z3);
  
  memcpy(out->X, X3.w, 32); memcpy(out->Y, Y3.w, 32); memcpy(out->Z, Z3.w, 32);
}

void Secp256r1::jacobianToAffine(const Secp256r1Jacobian& P, BigNum256* outX, BigNum256* outY) {
  BigNum256 Z1, Z1Inv, Z1Inv2, Z1Inv3;
  memcpy(Z1.w, P.Z, 32);
  
  if (bnIsZero(Z1)) {
    bnSetZero(outX); bnSetZero(outY); return;
  }
  
  bnModInv(Z1, &Z1Inv);
  bnModSqr(Z1Inv, &Z1Inv2);
  bnModMul(Z1Inv2, Z1Inv, &Z1Inv3);
  
  BigNum256 X1, Y1;
  memcpy(X1.w, P.X, 32); memcpy(Y1.w, P.Y, 32);
  bnModMul(X1, Z1Inv2, outX);
  bnModMul(Y1, Z1Inv3, outY);
}

// ─── Optimized scalar multiply using Jacobian coordinates ─────────────────

bool Secp256r1::scalarMultiply(const Secp256r1Scalar& k, const Secp256r1Point& P, Secp256r1Point* outR) {
  if(!outR)return false;
  BigNum256 kBn; bnFromBytes(k.bytes,&kBn);
  if(bnIsZero(kBn)||isInfinity(P)){setInfinity(outR);return true;}
  
  // Convert base point to Jacobian
  BigNum256 Qx, Qy;
  pointFromAffine(P, &Qx, &Qy);
  
  // Double-and-add using Jacobian coordinates
  Secp256r1Jacobian R; memset(&R, 0, sizeof(R)); // infinity
  bool started = false;
  
  for(int wi=7;wi>=0;wi--)for(int bi=31;bi>=0;bi--){
    if(started){ jacobianDouble(R, &R); }
    if((kBn.w[wi]>>bi)&1U){
      if(!started){
        affineToJacobian(Qx, Qy, &R);
        started = true;
      } else {
        jacobianAddMixed(R, Qx, Qy, &R);
      }
    }
  }
  
  if (!started) { setInfinity(outR); return true; }
  
  // Convert back to affine
  BigNum256 outX, outY;
  jacobianToAffine(R, &outX, &outY);
  pointToAffine(outX, outY, outR);
  return true;
}

// ─── 4-bit windowed scalar multiply for base point G ────────────────────

// Precomputed 1G..15G in affine (Little-Endian word arrays)
static const uint32_t kPreG[15][2][8] = {
  {{0xD898C296U,0xF4A13945U,0x2DEB33A0U,0x77037D81U,0x63A440F2U,0xF8BCE6E5U,0xE12C4247U,0x6B17D1F2U},
   {0x37BF51F5U,0xCBB64068U,0x6B315ECEU,0x2BCE3357U,0x7C0F9E16U,0x8EE7EB4AU,0xFE1A7F9BU,0x4FE342E2U}},
  {{0x47669978U,0xA60B48FCU,0x77F21B35U,0xC08969E2U,0x04B51AC3U,0x8A523803U,0x8D034F7EU,0x7CF27B18U},
   {0x227873D1U,0x9E04B79DU,0x3CE98229U,0xBA7DADE6U,0x9F7430DBU,0x293D9AC6U,0xDB8ED040U,0x07775510U}},
  {{0xC6E7FD6CU,0xFB41661BU,0xEFADA985U,0xE6C6B721U,0x1D4BF165U,0xC8F7EF95U,0xA6330A44U,0x5ECBE4D1U},
   {0xA27D5032U,0x9A79B127U,0x384FB83DU,0xD82AB036U,0x1A64A2ECU,0x374B06CEU,0x4998FF7EU,0x8734640CU}},
  {{0x6B030852U,0x50930244U,0x785596EFU,0x031FE2DBU,0x9EE62BD0U,0xA02DDE65U,0x32D08FBBU,0xE2534A35U},
   {0x184ED8C6U,0x5C42C23FU,0xF30EE005U,0x4EFC96C3U,0xDA862D76U,0x19DFEE5FU,0x4C633CC7U,0xE0F1575AU}},
  {{0xC3D033EDU,0x21554A0DU,0x1F5BE524U,0xEF8C82FDU,0x08668FDFU,0xD784C856U,0x515140D2U,0x51590B7AU},
   {0xFDA16DA4U,0xD1D0BB44U,0xD4D80888U,0x0D012F00U,0xBF8A7926U,0x8AE1BF36U,0x904A727DU,0xE0C17DA8U}},
  {{0x3C2291A9U,0xC6B0AAE9U,0xEBB215B4U,0x024C740DU,0xB897DDE3U,0x92D3242CU,0x76A4602CU,0xB01A172AU},
   {0x8FC77FE2U,0xFD7C4853U,0x1C7E16BDU,0x1C00F770U,0xFBA70379U,0x6FEC0E2DU,0x3237DAD5U,0xE85C1074U}},
  {{0x3187B2A3U,0x30062870U,0xA80FEF5BU,0x7EF9F8B8U,0x7C01FB60U,0x25BB3066U,0xA0BF7B46U,0x8E533B6FU},
   {0xC1F400B4U,0xC55E1A86U,0xCB041B21U,0x53C73633U,0xA6F59000U,0x6D069F83U,0xE0331836U,0x73EB1DBDU}},
  {{0xDB6FB393U,0xB4DD9DC1U,0x0FCE97DBU,0xC1D23898U,0x3AB54CADU,0x4042742DU,0xBEE9B053U,0x62D9779DU},
   {0x0F09957EU,0xDA540A6AU,0xBBE76A78U,0xA2ED51F6U,0x1167CEE0U,0x4FF15D77U,0x91E9D824U,0xAD5ACCBDU}},
  {{0x90949EE0U,0xD79E8A4BU,0x2C6DF8B3U,0x9E0ACB8CU,0x1D71F872U,0x878938D5U,0xFEDF0B71U,0xEA68D7B6U},
   {0x4DD048FAU,0xE85A224AU,0xA4DE823FU,0x4D714FEAU,0x4A8EA0C8U,0x87014A96U,0x72C9FCE7U,0x2A2744C9U}},
  {{0x04C5723FU,0x4C360694U,0x1C48306EU,0x45CA6C47U,0xEA223FB5U,0x591214D1U,0x2A3A993EU,0xCEF66D6BU},
   {0x44AF0773U,0xCA34BBAAU,0xFE751EEEU,0x590DED29U,0x9D3B4C10U,0x6E123CDDU,0x29AAAE90U,0x878662A2U}},
  {{0x74BC21D1U,0x433391D3U,0x255048BFU,0x16742ED0U,0xB0C21CDAU,0x0638379DU,0x883B4C59U,0x3ED113B7U},
   {0xE82A3740U,0xE2F8EEFCU,0x5E9889DAU,0x090D04DAU,0xA4F4C68AU,0x24C843AFU,0xCCC4C8A2U,0x9099209AU}},
  {{0x8624E3C4U,0xD500C5EEU,0xB2F82C99U,0x79983028U,0x20E5D551U,0x46265373U,0xA817D95EU,0x741DD5BDU},
   {0xCD4481D3U,0x1995FF22U,0x35BA5CA7U,0x8EEB912CU,0x4887B154U,0x56738355U,0x9C385FDCU,0x0770B46AU}},
  {{0x46072C01U,0x98E15D9DU,0x65EAD58AU,0x792E284BU,0xD85EE2FCU,0x61805DF2U,0xE0AC495AU,0x177C837AU},
   {0xEFC7BFD8U,0x9C43BBE2U,0xA1FB4DF3U,0x26EE14C3U,0xB40F4E72U,0xA24091ADU,0x4EBEA558U,0x63BB58CDU}},
  {{0x24D2920BU,0x57092773U,0x7A069C5EU,0xF126ACBEU,0x4336DF3CU,0x7A76647FU,0x1C3862B9U,0x54E77A00U},
   {0x60D0B375U,0x1BA7C82FU,0x73509008U,0x7171EA77U,0x05A2E7C3U,0x42121F8CU,0x29F43175U,0xF599F1BBU}},
  {{0xE59B9D5FU,0x63668C63U,0xDE3A0EF1U,0xAE03AF92U,0x99888265U,0xADFB3789U,0x971ABAE7U,0xF0454DC6U},
   {0x0D034F36U,0x47E59CDEU,0x75B5FA3FU,0x2A3B21CEU,0x1F9643E6U,0x4E6594E5U,0x592E2D1FU,0xB5B93EE3U}},
};

bool Secp256r1::scalarMultiplyBase(const Secp256r1Scalar& k, Secp256r1Point* outR) {
  BigNum256 kBn; bnFromBytes(k.bytes,&kBn);
  if(bnIsZero(kBn)){setInfinity(outR);return true;}
  
  // 4-bit windowed: process scalar as 64 nibbles from MSB to LSB
  Secp256r1Jacobian R; memset(&R,0,sizeof(R));
  bool started=false;
  for(int wi=7;wi>=0;wi--){
    for(int ni=7;ni>=0;ni--){
      int nibble=(int)((kBn.w[wi]>>(ni*4))&0xFU);
      if(started){for(int d=0;d<4;d++)jacobianDouble(R,&R);}
      if(nibble!=0){
        const uint32_t*px=kPreG[nibble-1][0];
        const uint32_t*py=kPreG[nibble-1][1];
        BigNum256 x,y; memcpy(x.w,px,32); memcpy(y.w,py,32);
        if(!started){affineToJacobian(x,y,&R);started=true;}
        else{jacobianAddMixed(R,x,y,&R);}
      }
    }
  }
  if(!started){setInfinity(outR);return true;}
  BigNum256 outX,outY; jacobianToAffine(R,&outX,&outY);
  pointToAffine(outX,outY,outR); return true;
}

bool Secp256r1::isOnCurve(const Secp256r1Point& point) {
  BigNum256 x,y; pointFromAffine(point,&x,&y); BigNum256 p=primeP();
  if(bnCompare(x,p)>=0||bnCompare(y,p)>=0)return false;
  BigNum256 x2,x3,b=curveB(); bnModSqr(x,&x2); bnModMul(x2,x,&x3);
  BigNum256 three;bnSetOne(&three);three.w[0]=3;BigNum256 t1,t2,rhs,lhs;
  bnModMul(three,x,&t1); bnModSub(x3,t1,&t2); bnModAdd(t2,b,&rhs);
  bnModSqr(y,&lhs); return bnEquals(lhs,rhs);
}

bool Secp256r1::isInfinity(const Secp256r1Point& point) {
  static const uint8_t z[32]={0};
  if(memcmp(point.x,z,32)==0&&memcmp(point.y,z,32)==0)return true;
  BigNum256 x,y; bnFromBytes(point.x,&x); bnFromBytes(point.y,&y);
  BigNum256 p=primeP();
  return bnCompare(x,p)>=0||bnCompare(y,p)>=0;
}

void Secp256r1::encodeUncompressed(const Secp256r1Point& point, uint8_t out[65]) {
  out[0]=0x04; memcpy(out+1,point.x,32); memcpy(out+33,point.y,32);
}

bool Secp256r1::decodeUncompressed(const uint8_t bytes[65], Secp256r1Point* outPoint) {
  if(!outPoint||bytes[0]!=0x04)return false;
  memcpy(outPoint->x,bytes+1,32); memcpy(outPoint->y,bytes+33,32);
  return isOnCurve(*outPoint);
}

void Secp256r1::setBasePoint(Secp256r1Point* out) { if(!out)return; BigNum256 x=basePointX(),y=basePointY();bnToBytes(x,out->x);bnToBytes(y,out->y); }
void Secp256r1::setInfinity(Secp256r1Point* out) { if(out)memset(out,0,sizeof(*out)); }
void Secp256r1::copyPoint(const Secp256r1Point& src, Secp256r1Point* dst) { if(dst)memcpy(dst,&src,sizeof(*dst)); }

void Secp256r1::getOrder(Secp256r1Scalar* outN) { if(outN){BigNum256 n=orderN();bnToBytes(n,outN->bytes);} }

bool Secp256r1::modInverseN(const Secp256r1Scalar& a, Secp256r1Scalar* outInv) {
  if(!outInv)return false;
  BigNum256 aBn; bnFromBytes(a.bytes,&aBn); if(bnIsZero(aBn)){memset(outInv->bytes,0,32);return false;}
  static const uint32_t kNminus2[8]={0xFC63254FU,0xF3B9CAC2U,0xA7179E84U,0xBCE6FAADU,0xFFFFFFFFU,0xFFFFFFFFU,0x00000000U,0xFFFFFFFFU};
  BigNum256 result; bnSetOne(&result); bool started=false;
  for(int wi=7;wi>=0;wi--)for(int bi=31;bi>=0;bi--){
    if(started)bnModMulN(result,result,&result);
    if((kNminus2[wi]>>bi)&1U){if(!started){result=aBn;started=true;}else bnModMulN(result,aBn,&result);}
  }
  bnToBytes(result,outInv->bytes); return true;
}

// ─── Random generation ────────────────────────────────────────────────────

void Secp256r1::generateRandomScalar(Secp256r1Scalar* outScalar) {
  if(!outScalar)return;
  randomBytes(outScalar->bytes,32);
  BigNum256 s; bnFromBytes(outScalar->bytes,&s); if(bnIsZero(s))bnSetOne(&s);
  BigNum256 n=orderN(); if(bnCompare(s,n)>=0)bnSub(s,n,&s); bnToBytes(s,outScalar->bytes);
}

void Secp256r1::randomBytes(uint8_t* out, size_t len) {
  if(!out)return;
  static uint64_t state=0x5A3C9E27F4B18D06ULL;
  for(size_t i=0;i<len;i++){state=state*6364136223846793005ULL+1442695040888963407ULL;out[i]=(uint8_t)(state>>32);}
}

uint32_t Secp256r1::randomWord() {
  static uint64_t state=0; if(!state)state=(uint64_t)micros()^((uint64_t)millis()<<32);
  state=state*6364136223846793005ULL+1442695040888963407ULL;
  return (uint32_t)(state>>32);
}

// ─── ECDSA ─────────────────────────────────────────────────────────────────

void Secp256r1::generateKeyPair(Secp256r1Scalar* outPriv, Secp256r1Point* outPub) {
  if(!outPriv||!outPub)return; generateRandomScalar(outPriv); scalarMultiplyBase(*outPriv,outPub);
}

bool Secp256r1::ecdsaSign(const Secp256r1Scalar& priv, const uint8_t hash[32], uint8_t r[32], uint8_t s[32]) {
  if(!hash||!r||!s)return false;
  Secp256r1Scalar k; Secp256r1Point R; BigNum256 n=orderN();
  for(int attempt=0;attempt<32;attempt++){
    generateRandomScalar(&k); if(!scalarMultiplyBase(k,&R))continue; if(isInfinity(R))continue;
    BigNum256 rBn; bnFromBytes(R.x,&rBn); if(bnCompare(rBn,n)>=0)bnSub(rBn,n,&rBn); if(bnIsZero(rBn))continue;
    BigNum256 hBn,dBn,kBn; bnFromBytes(hash,&hBn); bnFromBytes(priv.bytes,&dBn); bnFromBytes(k.bytes,&kBn);
    BigNum256 rd,hrd; bnModMulN(rBn,dBn,&rd); bnModAddN(hBn,rd,&hrd);
    Secp256r1Scalar kInv; if(!modInverseN(k,&kInv))continue;
    BigNum256 kInvBn; bnFromBytes(kInv.bytes,&kInvBn);
    BigNum256 sBn; bnModMulN(kInvBn,hrd,&sBn); if(bnIsZero(sBn))continue;
    bnToBytes(rBn,r); bnToBytes(sBn,s); return true;
  }
  return false;
}

bool Secp256r1::ecdsaVerify(const Secp256r1Point& pub, const uint8_t hash[32], const uint8_t r[32], const uint8_t s[32]) {
  if(!hash||!r||!s||isInfinity(pub))return false;
  BigNum256 rBn,sBn,n=orderN(); bnFromBytes(r,&rBn); bnFromBytes(s,&sBn);
  if(bnIsZero(rBn)||bnIsZero(sBn))return false;
  if(bnCompare(rBn,n)>=0||bnCompare(sBn,n)>=0)return false;
  Secp256r1Scalar sSc,wSc; memcpy(sSc.bytes,s,32);
  if(!modInverseN(sSc,&wSc))return false;
  BigNum256 hBn,wBn; bnFromBytes(hash,&hBn); bnFromBytes(wSc.bytes,&wBn);
  BigNum256 u1,u2; bnModMulN(hBn,wBn,&u1); bnModMulN(rBn,wBn,&u2);
  Secp256r1Scalar u1s,u2s; bnToBytes(u1,u1s.bytes); bnToBytes(u2,u2s.bytes);
  Secp256r1Point R1,R2,Rp; if(!scalarMultiplyBase(u1s,&R1))return false;
  if(!scalarMultiply(u2s,pub,&R2))return false;
  if(!pointAdd(R1,R2,&Rp))return false; if(isInfinity(Rp))return false;
  BigNum256 rpBn; bnFromBytes(Rp.x,&rpBn); if(bnCompare(rpBn,n)>=0)bnSub(rpBn,n,&rpBn);
  return bnEquals(rBn,rpBn);
}

}  // namespace xiao_nrf54l15
