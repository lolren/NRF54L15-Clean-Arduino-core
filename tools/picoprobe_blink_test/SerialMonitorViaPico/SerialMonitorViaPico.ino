static unsigned long g_lastHeartbeatMs = 0;
static uint32_t g_heartbeatCount = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("XIAO nRF54L15 serial monitor test");
  Serial.println("route=header-uart baud=115200");
  Serial.println("type something and it will echo back");
}

void loop() {
  const unsigned long now = millis();
  if ((now - g_lastHeartbeatMs) >= 1000UL) {
    g_lastHeartbeatMs = now;
    digitalWrite(LED_BUILTIN, (g_heartbeatCount & 1U) ? HIGH : LOW);
    Serial.print("heartbeat=");
    Serial.println(g_heartbeatCount++);
  }

  while (Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch < 0) {
      break;
    }
    Serial.print("echo:");
    Serial.write(static_cast<uint8_t>(ch));
    if (ch == '\r') {
      Serial.println();
    }
  }
}
