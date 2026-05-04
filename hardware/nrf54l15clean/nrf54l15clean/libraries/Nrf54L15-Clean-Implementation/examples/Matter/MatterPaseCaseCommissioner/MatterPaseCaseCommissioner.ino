// Matter PASE→CASE Full Commissioning — 2-Board Demo
// Runs PASE first (verified working), then CASE Sigma over same UDP.


#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Thread Core"
#endif

using xiao_nrf54l15::Nrf54ThreadExperimental;
using xiao_nrf54l15::Secp256r1;
using xiao_nrf54l15::Secp256r1Point;
using xiao_nrf54l15::Secp256r1Scalar;
using xiao_nrf54l15::MatterPbkdf2;

enum class DemoRole : uint8_t { COMMISSIONER = 0, COMMISSIONEE = 1 };
constexpr DemoRole ROLE = DemoRole::COMMISSIONER;

namespace {
constexpr uint16_t kPort = 5570U;
constexpr uint32_t kPin = 20202021UL;
Nrf54ThreadExperimental g_thread;

// PASE state
bool g_paseDone=false;
uint8_t g_secret[32]={0},g_salt[32]={0},g_w0[32]={0},g_w1[32]={0},g_xpt[65]={0};
uint32_t g_iters=2000; uint16_t g_sid=0;

// CASE state
bool g_idRdy=false,g_ephRdy=false,g_caseDone=false;
Secp256r1Scalar g_idPr,g_ephPr; Secp256r1Point g_idPu,g_ephPu;
uint8_t g_opk[16]={0};

otIp6Address g_peer={};
enum Msg:uint8_t{kAnn=0,kPbReq=1,kPbRsp=2,kSp1=3,kSp2=4,kSp3=5,kCS1=6,kCS2=7,kCS3=8};
static const otIp6Address kMA={.mFields={.m8={0xff,0x03,0,0,0,0,0,0,0,0,0,0,0,0,0,1}}};

// Crypto helpers (same as PASE demo)
void deriveWS(uint32_t pin,const uint8_t*s,uint32_t it,uint8_t*w0,uint8_t*w1){
  char ps[16];snprintf(ps,16,"%lu",(unsigned long)pin);size_t pl=strlen(ps);
  uint8_t raw[32],buf[128];const char*c="SPAKE2P Key Salt";size_t cl=strlen(c);
  memcpy(buf,s,32);memcpy(buf+32,c,cl);
  MatterPbkdf2::deriveKey((const uint8_t*)ps,pl,buf,32+cl,it,32,raw);
  Secp256r1::BigNum256 w,n=Secp256r1::orderN();
  Secp256r1::bnFromBytes(raw,&w);
  if(Secp256r1::bnCompare(w,n)>=0)Secp256r1::bnSub(w,n,&w);
  if(Secp256r1::bnIsZero(w))Secp256r1::bnSetOne(&w);
  Secp256r1::bnToBytes(w,w0);
  memcpy(buf,s,32);memcpy(buf+32,w0,32);memcpy(buf+64,c,cl);
  MatterPbkdf2::deriveKey((const uint8_t*)ps,pl,buf,64+cl,it,32,raw);
  Secp256r1::bnFromBytes(raw,&w);
  if(Secp256r1::bnCompare(w,n)>=0)Secp256r1::bnSub(w,n,&w);
  if(Secp256r1::bnIsZero(w))Secp256r1::bnSetOne(&w);
  Secp256r1::bnToBytes(w,w1);
}
bool spCmt(bool pr,const uint8_t*w0,uint8_t out[65]){
  Secp256r1Scalar x;Secp256r1::generateRandomScalar(&x);
  Secp256r1::BigNum256 xb,wb,sb,n=Secp256r1::orderN();
  Secp256r1::bnFromBytes(x.bytes,&xb);Secp256r1::bnFromBytes(w0,&wb);
  Secp256r1::bnAdd(xb,wb,&sb);
  if(Secp256r1::bnCompare(sb,n)>=0)Secp256r1::bnSub(sb,n,&sb);
  Secp256r1Scalar sc;Secp256r1::bnToBytes(sb,sc.bytes);
  Secp256r1Point P;if(!Secp256r1::scalarMultiplyBase(sc,&P))return false;
  Secp256r1::encodeUncompressed(P,out);return true;
}
void aCtr(const uint8_t k[16],const uint8_t*iv,const uint8_t*in,uint8_t*out,size_t len){
  uint8_t c[16];memcpy(c,iv,16);
  for(size_t i=0;i<len;i+=16){uint8_t ks[16],t[32];memcpy(t,k,16);memcpy(t+16,c,16);MatterPbkdf2::sha256(t,32,ks);for(size_t j=0;j<16&&(i+j)<len;j++)out[i+j]=in[i+j]^ks[j];for(int j=15;j>=0;j--){c[j]++;if(c[j]!=0)break;}}
}
bool ecdh(const Secp256r1Scalar&pr,const Secp256r1Point&pu,uint8_t sh[32]){
  Secp256r1Point P;if(!Secp256r1::scalarMultiply(pr,pu,&P))return false;memcpy(sh,P.x,32);return true;
}

// Fragmentation (for CASE Sigma messages >74 bytes)
constexpr size_t kMF=55;uint8_t g_rb[512];uint16_t g_rl=0;uint8_t g_rt=0,g_rm=0;
void sF(uint8_t t,const uint8_t*d,uint16_t len,const otIp6Address&dst){
  uint8_t tot=(len+kMF-1)/kMF;
  for(uint8_t s=0;s<tot;s++){uint8_t f[64];f[0]=t;f[1]=s;f[2]=tot;uint16_t off=s*kMF,ch=len-off;if(ch>kMF)ch=kMF;memcpy(f+3,d+off,ch);g_thread.sendUdp(dst,kPort,f,3+ch);delay(30);}
}
bool rF(const uint8_t*f,uint16_t fl,uint8_t*ot,uint8_t**od,uint16_t*ol){
  if(fl<3)return false;uint8_t s=f[1],t=f[2];if(!t||t>16)return false;
  if(g_rt!=t){g_rt=t;g_rm=0;g_rl=0;}if(s>=t)return false;
  uint16_t off=s*kMF,ch=fl-3;memcpy(g_rb+off,f+3,ch);g_rm|=(1U<<s);if(off+ch>g_rl)g_rl=off+ch;
  uint8_t am=(t<8)?((1U<<t)-1):0xFF;if(t==8)am=0xFF;
  if((g_rm&am)==am){*ot=f[0];*od=g_rb;*ol=g_rl;g_rt=0;return true;}return false;
}

// CASE handler: process Sigma1 (responder side)
void caseSig1(const uint8_t*pl,uint16_t len,const otMessageInfo&info){
  if(len<194)return;
  uint8_t ee[65],ie[65],sr[32],ss[32];
  memcpy(ee,pl,65);memcpy(ie,pl+65,65);memcpy(sr,pl+130,32);memcpy(ss,pl+162,32);
  uint8_t hi[131];hi[0]=0;memcpy(hi+1,ee,65);memcpy(hi+66,ie,65);
  uint8_t hh[32];MatterPbkdf2::sha256(hi,131,hh);
  Secp256r1Point pP,pE;
  if(!Secp256r1::decodeUncompressed(ie,&pP)||!Secp256r1::decodeUncompressed(ee,&pE))return;
  Serial.print("CASE sig1 ");unsigned long t0=millis();
  if(!Secp256r1::ecdsaVerify(pP,hh,sr,ss)){Serial.println("FAIL");return;}
  Serial.print("OK ");Serial.print(millis()-t0);Serial.println("ms");
  Secp256r1::generateKeyPair(&g_ephPr,&g_ephPu);g_ephRdy=true;
  uint8_t sh[32];if(!ecdh(g_ephPr,pE,sh))return;
  MatterPbkdf2::hmacSha256(sh,32,(const uint8_t*)"CaseSession",11,g_opk);
  uint8_t p2[210],re[65],ri[65];
  Secp256r1::encodeUncompressed(g_ephPu,re);Secp256r1::encodeUncompressed(g_idPu,ri);
  memcpy(p2,re,65);memcpy(p2+65,ri,65);
  uint8_t si[196];si[0]=1;memcpy(si+1,re,65);memcpy(si+66,ri,65);memcpy(si+131,ee,65);
  MatterPbkdf2::sha256(si,196,hh);
  uint8_t r2[32]={0},s2[32]={0};
  t0=millis();if(!Secp256r1::ecdsaSign(g_idPr,hh,r2,s2))return;
  Serial.print("CASE sig2 sign ");Serial.print(millis()-t0);Serial.println("ms");
  memcpy(p2+130,r2,32);memcpy(p2+162,s2,32);
  uint8_t iv[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  aCtr(g_opk,iv,(uint8_t*)"CASERESP_OK",p2+194,12);
  sF(kCS2,p2,206,info.mPeerAddr);g_caseDone=true;
  Serial.println("CASE DONE (responder)");
}

// CASE handler: process Sigma2 (initiator side)
void caseSig2(const uint8_t*pl,uint16_t len,const otMessageInfo&info){
  if(len<206)return;
  uint8_t ee[65],ie[65],sr[32],ss[32],enc[12];
  memcpy(ee,pl,65);memcpy(ie,pl+65,65);memcpy(sr,pl+130,32);memcpy(ss,pl+162,32);memcpy(enc,pl+194,12);
  uint8_t si[196];si[0]=1;memcpy(si+1,ee,65);memcpy(si+66,ie,65);
  uint8_t oe[65];Secp256r1::encodeUncompressed(g_ephPu,oe);memcpy(si+131,oe,65);
  uint8_t hh[32];MatterPbkdf2::sha256(si,196,hh);
  Secp256r1Point pP,pE;
  if(!Secp256r1::decodeUncompressed(ie,&pP)||!Secp256r1::decodeUncompressed(ee,&pE))return;
  Serial.print("CASE sig2 ");unsigned long t0=millis();
  if(!Secp256r1::ecdsaVerify(pP,hh,sr,ss)){Serial.println("FAIL");return;}
  Serial.print("OK ");Serial.print(millis()-t0);Serial.println("ms");
  uint8_t sh[32];if(!ecdh(g_ephPr,pE,sh))return;
  MatterPbkdf2::hmacSha256(sh,32,(const uint8_t*)"CaseSession",11,g_opk);
  uint8_t iv[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},dec[12];
  aCtr(g_opk,iv,enc,dec,12);Serial.print("CASE decrypt: ");Serial.write(dec,12);Serial.println();
  g_caseDone=true;
  uint8_t c3[12]="CASEINIT_OK",p3[12];
  aCtr(g_opk,iv,c3,p3,12);sF(kCS3,p3,12,info.mPeerAddr);
  Serial.println("CASE DONE (initiator)");
}

void onUdp(void*,const uint8_t*d,uint16_t len,const otMessageInfo&info){
  if(!d||len<1)return;uint8_t t=d[0];
  if(t==kAnn){memcpy(&g_peer,&info.mPeerAddr,sizeof(otIp6Address));return;}
  
  // Fragment reassembly for CASE
  if(t==kCS1||t==kCS2||t==kCS3){uint8_t*full,ft;uint16_t fl;if(rF(d,len,&ft,&full,&fl)){if(ROLE==DemoRole::COMMISSIONEE&&ft==kCS1)caseSig1(full,fl,info);else if(ROLE==DemoRole::COMMISSIONER&&ft==kCS2)caseSig2(full,fl,info);}return;}
  
  // PASE handling (small messages)
  Serial.print("pase rx t=");Serial.println(t);
  if(ROLE==DemoRole::COMMISSIONER&&t==kPbReq){if(len<33)return;memcpy(g_salt,d+1,32);deriveWS(kPin,g_salt,g_iters,g_w0,g_w1);g_sid=(uint16_t)(micros()&0xFFFF)|1U;uint8_t rs[39];rs[0]=kPbRsp;memcpy(rs+1,g_salt,32);memcpy(rs+33,&g_iters,4);memcpy(rs+37,&g_sid,2);g_thread.sendUdp(info.mPeerAddr,info.mPeerPort,rs,39);Serial.println("pase pbkdf-resp");return;}
  if(ROLE==DemoRole::COMMISSIONEE&&t==kPbRsp){if(len<39)return;memcpy(g_salt,d+1,32);memcpy(&g_iters,d+33,4);memcpy(&g_sid,d+35,2);unsigned long ta=millis();deriveWS(kPin,g_salt,g_iters,g_w0,g_w1);Serial.print("pase ws ");Serial.print(millis()-ta);Serial.println("ms");uint8_t X[65];unsigned long tb=millis();if(spCmt(true,g_w0,X)){memcpy(g_xpt,X,65);Serial.print("pase cmt ");Serial.print(millis()-tb);Serial.println("ms");uint8_t m[68];m[0]=kSp1;memcpy(m+1,X,65);memcpy(m+66,&g_sid,2);g_thread.sendUdp(info.mPeerAddr,info.mPeerPort,m,68);Serial.println("pase sp1");}return;}
  if(ROLE==DemoRole::COMMISSIONER&&t==kSp1){if(len<68)return;uint8_t pX[65];memcpy(pX,d+1,65);uint8_t Y[65];if(!spCmt(false,g_w0,Y))return;uint8_t cn[162];memcpy(cn,pX,65);memcpy(cn+65,Y,65);memcpy(cn+130,g_w0,32);MatterPbkdf2::sha256(cn,162,g_secret);g_paseDone=true;uint8_t m[74];m[0]=kSp2;memcpy(m+1,Y,65);uint8_t cf[32];MatterPbkdf2::hmacSha256(g_secret,32,(const uint8_t*)"confirm",7,cf);memcpy(m+66,cf,8);g_thread.sendUdp(info.mPeerAddr,info.mPeerPort,m,74);Serial.println("pase DONE");return;}
  if(ROLE==DemoRole::COMMISSIONEE&&t==kSp2){if(len<74)return;uint8_t pY[65];memcpy(pY,d+1,65);uint8_t cn[162];memcpy(cn,g_xpt,65);memcpy(cn+65,pY,65);memcpy(cn+130,g_w0,32);MatterPbkdf2::sha256(cn,162,g_secret);uint8_t ec[32];MatterPbkdf2::hmacSha256(g_secret,32,(const uint8_t*)"confirm",7,ec);if(memcmp(d+66,ec,8)==0){g_paseDone=true;uint8_t m[9];m[0]=kSp3;memcpy(m+1,ec+8,8);g_thread.sendUdp(info.mPeerAddr,info.mPeerPort,m,9);Serial.println("pase VERIFIED");}return;}
}

} // namespace

void setup(){
  Serial.begin(115200);while(!Serial&&(millis()-1)<3000){}
  Serial.println();Serial.print("=== PC ");Serial.println(ROLE==DemoRole::COMMISSIONER?"COMM":"COMME");
  
  otOperationalDataset ds={};Nrf54ThreadExperimental::buildDemoDataset(&ds);
  g_thread.setActiveDataset(ds);g_thread.begin();
  g_thread.openUdp(kPort,onUdp,nullptr);
  
  if(ROLE==DemoRole::COMMISSIONER){for(int i=0;i<32;i++)g_salt[i]=(uint8_t)(i*7+13);unsigned long t0=millis();deriveWS(kPin,g_salt,g_iters,g_w0,g_w1);Serial.print("verifier ");Serial.print(millis()-t0);Serial.println("ms");}
  
  Serial.print("thread=");Serial.print(g_thread.roleName());Serial.print(" udp=");Serial.println(g_thread.udpOpened()?1:0);
}

void loop(){
  g_thread.process();
  
  // Announce every 3s
  {static uint32_t la=0;if(g_thread.udpOpened()&&(millis()-la)>=3000){la=millis();uint8_t a[1]={kAnn};g_thread.sendUdp(kMA,kPort,a,1);}}
  
  // Commissionee starts PASE - send to leader, retry if no response
  if(ROLE==DemoRole::COMMISSIONEE&&!g_paseDone&&g_thread.attached()){static uint32_t lp=0;static bool sent=false;
    if((millis()-lp)>=10000||!sent){lp=millis();sent=true;otIp6Address leader;if(g_thread.getLeaderRloc(&leader)){uint8_t r[33];r[0]=kPbReq;for(int i=0;i<32;i++)g_salt[i]=(uint8_t)(micros()^(millis()>>(i%8)));memcpy(r+1,g_salt,32);g_thread.sendUdp(leader,kPort,r,33);Serial.println("pase pbkdf-req");}}}
  
  // After PASE complete, generate identity key then run CASE
  static uint8_t phase=0;
  if(g_paseDone&&phase==0){phase=1;Serial.println("PASE done, generating id key...");}
  if(phase==1){unsigned long t0=millis();Secp256r1::generateKeyPair(&g_idPr,&g_idPu);Serial.print("id key ");Serial.print(millis()-t0);Serial.println("ms");g_idRdy=true;phase=2;}
  if(phase==2&&ROLE==DemoRole::COMMISSIONER){unsigned long t0=millis();Secp256r1::generateKeyPair(&g_ephPr,&g_ephPu);Serial.print("eph key ");Serial.print(millis()-t0);Serial.println("ms");g_ephRdy=true;phase=3;}
  if(phase==2&&ROLE==DemoRole::COMMISSIONEE)phase=3;
  
  // Initiator sends CASE Sigma1
  if(phase==3&&ROLE==DemoRole::COMMISSIONER&&g_ephRdy&&!g_caseDone){static uint32_t ls=0;if((millis()-ls)>=5000){ls=millis();uint8_t ee[65],ie[65];Secp256r1::encodeUncompressed(g_ephPu,ee);Secp256r1::encodeUncompressed(g_idPu,ie);uint8_t hi[131];hi[0]=0;memcpy(hi+1,ee,65);memcpy(hi+66,ie,65);uint8_t hh[32];MatterPbkdf2::sha256(hi,131,hh);uint8_t r[32]={0},s[32]={0};Serial.print("CASE sig1 sign ");unsigned long t0=millis();if(Secp256r1::ecdsaSign(g_idPr,hh,r,s)){Serial.print(millis()-t0);Serial.println("ms");uint8_t p[194];memcpy(p,ee,65);memcpy(p+65,ie,65);memcpy(p+130,r,32);memcpy(p+162,s,32);sF(kCS1,p,194,g_peer);Serial.println("CASE sigma1 sent");}}}
  
  if(g_caseDone&&phase==3){phase=4;Serial.println("CASE complete!");}
  
  {static uint32_t ls=0;if((millis()-ls)>=5000){ls=millis();Serial.print(ROLE==DemoRole::COMMISSIONER?"C":"E");Serial.print(" P=");Serial.print(g_paseDone?1:0);Serial.print(" C=");Serial.print(g_caseDone?1:0);Serial.print(" ");Serial.println(g_thread.roleName());}}
}
