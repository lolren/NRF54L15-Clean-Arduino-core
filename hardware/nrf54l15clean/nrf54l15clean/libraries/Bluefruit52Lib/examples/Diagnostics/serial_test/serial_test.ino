void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  Serial1.begin(115200);
  delay(1000);
  Serial.println("TEST: Serial OK");
  Serial1.println("TEST: Serial1 OK");
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(200);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(800);
  Serial.println("BLINK");
  Serial1.println("BLINK");
}
