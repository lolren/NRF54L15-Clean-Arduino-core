#include <Arduino.h>
#include <matter_secp256r1.h>
#include <matter_pbkdf2.h>
using namespace xiao_nrf54l15;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ecdh: boot");
  
  Serial.println("ecdh: generating keypair...");
  Secp256r1Scalar priv;
  Secp256r1Point pub;
  Secp256r1::generateKeyPair(&priv, &pub);
  
  Serial.println("ecdh: keypair done!");
  
  uint8_t pubBytes[65]={0};
  Secp256r1::encodeUncompressed(pub, pubBytes);
  Serial.print("ecdh: pubkey=");
  for(int i=0;i<16;i++){if(pubBytes[i+1]<16)Serial.print('0');Serial.print(pubBytes[i+1],HEX);}
  Serial.println();
  Serial.println("ecdh: SUCCESS");
}

void loop() { delay(1000); Serial.println("ecdh: alive"); }
