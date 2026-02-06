#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ESP32 Discord Webhook テスト

#include "config.h"
// config.h に以下を定義:
//   WIFI_SSID, WIFI_PASS, DISCORD_WEBHOOK_URL

#define LED_PIN 2
#define BUTTON_PIN 0

void sendDiscordMessage(const char* message) {
  Serial.printf("[Discord] Free heap before: %d\n", ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);  // 10秒タイムアウト

  HTTPClient http;
  http.setTimeout(10000);
  http.begin(client, DISCORD_WEBHOOK_URL);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"content\":\"" + String(message) + "\"}";
  Serial.println("[Discord] Sending...");

  int httpCode = http.POST(payload);
  Serial.printf("[Discord] HTTP %d\n", httpCode);

  http.end();
  client.stop();

  Serial.printf("[Discord] Free heap after: %d\n", ESP.getFreeHeap());
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== ESP32 Discord Bot ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi FAILED!");
    return;
  }

  Serial.print("WiFi OK! IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_PIN, HIGH);

  delay(2000);
  sendDiscordMessage("パパ、トイレットペーパーを買ってきて");
  Serial.println("Setup done. Press BOOT button to send message.");
}

void loop() {
  // 動作確認用：5秒ごとにシリアル出力
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    Serial.printf("[loop] heap=%d wifi=%d\n", ESP.getFreeHeap(), WiFi.status() == WL_CONNECTED);
    lastPrint = millis();
  }

  // BOOTボタン押下でDiscord送信
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Button pressed!");
      if (WiFi.status() == WL_CONNECTED) {
        sendDiscordMessage("パパ、トイレットペーパーを買ってきて");
      } else {
        Serial.println("WiFi not connected!");
      }
      // ボタンリリース待ち
      while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
      Serial.println("Button released.");
    }
  }

  delay(10);
}
