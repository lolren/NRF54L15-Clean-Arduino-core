// Matter Operational Encrypted Over Thread
#include <nrf54_all.h>

#include "nrf54l15_hal.h"
#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core"
#endif
using namespace xiao_nrf54l15;
// Set ROLE: 0=LightNode 1=Controller
#ifndef MATTER_DEMO_ROLE
#define MATTER_DEMO_ROLE 0
#endif
namespace {
constexpr uint16_t kPort=5540U;
constexpr uint32_t kStatusMs=3000U,kCmdMs=5000U;
constexpr uint8_t kKey[16]={0xA1,0xB2,0xC3,0xD4,0xE5,0xF6,0x07,0x18,0x29,0x3A,0x4B,0x5C,0x6D,0x7E,0x8F,0x90};
Nrf54ThreadExperimental g_thread;
bool g_on=false;
uint8_t g_ec=0,g_dc=0;
uint32_t g_ls=0,g_lc=0;
void aesCtr(const uint8_t k[16],uint8_t ctr,const uint8_t*in,size_t len,uint8_t*out){
  Ecb ecb;
  for(size_t o=0;o<len;o+=16){
    uint8_t blk[16]={ctr,0,0,0,0,0,0,0,(uint8_t)(o&0xFF),(uint8_t)((o>>8)&0xFF),(uint8_t)((o>>16)&0xFF),(uint8_t)((o>>24)&0xFF),0,0,0,0};
    uint8_t ks[16]={0};ecb.encryptBlock(k,blk,ks);
    size_t n=len-o;if(n>16)n=16;
    for(size_t i=0;i<n;i++)out[o+i]=in[o+i]^ks[i];
  }
}
uint8_t mac(const uint8_t*d,size_t len){uint8_t m=0;for(size_t i=0;i<len;i++)m^=d[i];return m;}
void onRx(void*,const uint8_t*d,uint16_t len,const otMessageInfo&info){
#if MATTER_DEMO_ROLE==0
  if(!d||len<2)return;
  uint8_t ctr=d[0],ma=d[len-1];
  uint8_t p[32]={0};aesCtr(kKey,ctr,d+1,len-2,p);
  if(mac(p,len-2)!=ma){Serial.println("MAC FAIL");return;}
  if(len>=3){uint8_t cmd=p[0];
    if(cmd==1)g_on=true;else if(cmd==2)g_on=false;else if(cmd==3)g_on=!g_on;
    Serial.print("op cmd=");Serial.print(cmd==1?"ON":cmd==2?"OFF":"TOGGLE");
    Serial.print(" light=");Serial.println(g_on?"ON":"OFF");
  }
#endif
}
void send(uint8_t cmd){
  otIp6Address ld;if(!g_thread.getLeaderRloc(&ld))return;
  g_ec++;uint8_t p[1]={cmd},c[16]={0};
  aesCtr(kKey,g_ec,p,1,c);
  uint8_t b[32]={g_ec};memcpy(b+1,c,1);b[2]=mac(p,1);
  g_thread.sendUdp(ld,kPort,b,3);
}
void ps(){
  Serial.print("op role=");
#if MATTER_DEMO_ROLE==0
  Serial.print("light");
#else
  Serial.print("ctrl");
#endif
  Serial.print(" thread=");Serial.print(g_thread.roleName());
  Serial.print(" on=");Serial.print(g_on?1:0);
  Serial.print(" ec=");Serial.print(g_ec);
  Serial.print(" dc=");Serial.println(g_dc);
}
}
void setup(){
  Serial.begin(115200);{uint32_t t=millis();while(!Serial&&(millis()-t)<1500){}}
  pinMode(LED_BUILTIN,OUTPUT);digitalWrite(LED_BUILTIN,LOW);
  Serial.print("op === Encrypted Matter (");
#if MATTER_DEMO_ROLE==0
  Serial.println("LIGHT) ===");
#else
  Serial.println("CTRL) ===");
#endif
  otOperationalDataset ds={};Nrf54ThreadExperimental::buildDemoDataset(&ds);
  g_thread.setActiveDataset(ds);g_thread.begin();
  g_thread.openUdp(kPort,onRx,nullptr);
  ps();
}
void loop(){
  g_thread.process();
  digitalWrite(LED_BUILTIN,g_on?HIGH:LOW);
#if MATTER_DEMO_ROLE==1
  if(g_thread.attached()&&(millis()-g_lc)>=kCmdMs){g_lc=millis();
    static uint8_t s=0;uint8_t cmds[]={1,2,3,1};send(cmds[s%4]);s++;}
#endif
  if((millis()-g_ls)>=kStatusMs){g_ls=millis();ps();}
}
