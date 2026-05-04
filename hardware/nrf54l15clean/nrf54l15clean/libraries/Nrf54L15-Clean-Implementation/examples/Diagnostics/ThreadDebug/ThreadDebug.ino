#include <Arduino.h>
#include <nrf54_thread_experimental.h>
using xiao_nrf54l15::Nrf54ThreadExperimental;
Nrf54ThreadExperimental g_thread;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("DEBUG: boot");
  
  otOperationalDataset ds = {};
  Nrf54ThreadExperimental::buildDemoDataset(&ds);
  g_thread.setActiveDataset(ds);
  g_thread.begin();
  
  Serial.print("DEBUG: thread_started=");
  Serial.println(g_thread.started() ? 1 : 0);
  
  g_thread.openUdp(5540, nullptr, nullptr);
  Serial.print("DEBUG: udp=");
  Serial.println(g_thread.udpOpened() ? 1 : 0);
  Serial.println("DEBUG: setup_done");
}

void loop() {
  g_thread.process();
  static uint32_t last=0;
  if(millis()-last>=2000) {
    last=millis();
    Serial.print("DEBUG: role=");
    Serial.println(g_thread.roleName());
  }
}
