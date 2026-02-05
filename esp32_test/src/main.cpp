#include <Arduino.h>

// ESP32 ボタン入力テスト
// BOOTボタンを押すと、LEDが高速で3秒間点滅

#define LED_PIN 2     // 外部LED（GPIO2）
#define BUTTON_PIN 0  // BOOTボタン（GPIO0）

void setup() {
  // シリアル通信開始
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 Button Test Started");
  Serial.println("Press BOOT button to blink LED for 3 seconds");

  // LED設定
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // 初期状態：消灯

  // ボタン設定（内部プルアップ使用）
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Setup completed");
}

void loop() {
  // ボタンが押されたかチェック（LOWで押下）
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed! Blinking for 3 seconds...");

    // チャタリング防止
    delay(50);

    // 3秒間高速点滅（0.1秒間隔 = 100ms）
    unsigned long startTime = millis();
    while (millis() - startTime < 3000) {
      digitalWrite(LED_PIN, LOW);   // 点灯
      delay(100);
      digitalWrite(LED_PIN, HIGH);  // 消灯
      delay(100);
    }

    Serial.println("Blinking finished");

    // ボタンが離されるまで待つ
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(10);
    }
  }

  delay(10);  // CPU負荷軽減
}
