// ESP32 最小限テスト
// LED点滅 + シリアル通信確認

#define LED_PIN 2  // ESP32の内蔵LED（多くのボードで GPIO2）

void setup() {
  // シリアル通信開始
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 Test Started");
  Serial.println("Board: ESP32");

  // LED設定
  pinMode(LED_PIN, OUTPUT);

  Serial.println("Setup completed");
}

void loop() {
  // LED点滅
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(1000);

  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(1000);
}
