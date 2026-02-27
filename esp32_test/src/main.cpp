#include <Arduino.h>
#include "esp_sleep.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// ESP32-C3 Papa Dash - DeepSleep + 長押し設定モード
// Board: Seeed XIAO ESP32C3

#define LED_PIN 3             // GPIO3 (D1) - 外部LED（GPIO0-5はDeepSleep中も状態維持可能）
#define BUTTON_PIN 4          // GPIO4 (D2) - 外部ボタン: GPIO4 → スイッチ → GND
#define AP_SSID "PapaDash-Setup"
#define DNS_PORT 53
#define WEB_PORT 80
#define WIFI_CONNECT_TIMEOUT 20000   // 20秒
#define LONG_PRESS_THRESHOLD 5000    // 5秒長押しで設定モード
#define SETUP_TIMEOUT 300000         // 設定モード5分タイムアウト
#define LED_FAST_BLINK 100           // 長押し検出中の超高速点滅
#define LED_SETUP_BLINK 1000         // 設定モードの点滅間隔

bool inSetupMode = false;
bool waitForButtonRelease = false;  // 長押し後、ボタンが離されるまでloop()での検出を無視
unsigned long setupStartTime = 0;

Preferences prefs;
WebServer server(WEB_PORT);
DNSServer dnsServer;

// NVSから読み込んだ設定
String nvsSSID;
String nvsPass;
String nvsWebhookURL;
String nvsMessage;

// WiFiスキャン結果
String scannedSSIDs = "";

// LED制御
unsigned long ledLastToggle = 0;
bool ledState = false;

// --- HTML生成 ---

String escapeHtml(const String& s) {
  String out = s;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  out.replace("'", "&#39;");
  return out;
}

String buildConfigPage() {
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>PapaDash 設定</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#f0f2f5;padding:16px;font-size:16px}
.card{background:#fff;border-radius:12px;padding:24px;max-width:400px;margin:0 auto;box-shadow:0 2px 8px rgba(0,0,0,0.1)}
h1{font-size:20px;margin-bottom:8px;color:#333}
p.desc{font-size:14px;color:#666;margin-bottom:20px}
label{display:block;font-size:14px;font-weight:600;color:#555;margin-bottom:4px;margin-top:16px}
input[type=text],input[type=password],select{width:100%;padding:12px;border:1px solid #ddd;border-radius:8px;font-size:16px;-webkit-appearance:none;background:#fff}
select{-webkit-appearance:menulist;appearance:menulist}
input:focus,select:focus{outline:none;border-color:#5865F2;box-shadow:0 0 0 3px rgba(88,101,242,0.15)}
button{width:100%;padding:14px;background:#5865F2;color:#fff;border:none;border-radius:8px;font-size:16px;font-weight:600;margin-top:24px;cursor:pointer}
button:active{background:#4752C4}
.ssid-manual{margin-top:8px;display:none}
.ssid-manual.show{display:block}
.ok{background:#57F287;color:#333;padding:16px;border-radius:8px;text-align:center;margin-top:16px;font-weight:600}
</style>
</head>
<body>
<div class="card">
<h1>PapaDash 設定</h1>
<p class="desc">WiFiとDiscordの設定を入力してください</p>
<form method="POST" action="/save">
<label>WiFi SSID</label>
<select id="ssid_select" onchange="var m=document.getElementById('ssid_manual');var s=document.getElementById('ssid');if(this.value==='__manual__'){m.classList.add('show');s.value='';s.focus();}else{m.classList.remove('show');s.value=this.value;}">
)rawliteral";

  // スキャン結果をoptionとして追加
  if (scannedSSIDs.length() > 0) {
    html += scannedSSIDs;
  } else {
    html += "<option value=\"\">（スキャン結果なし）</option>\n";
  }
  html += "<option value=\"__manual__\">手動で入力...</option>\n";
  html += R"rawliteral(</select>
<div id="ssid_manual" class="ssid-manual">
<input type="text" id="ssid_manual_input" placeholder="SSIDを入力" onchange="document.getElementById('ssid').value=this.value;" oninput="document.getElementById('ssid').value=this.value;">
</div>
<input type="hidden" name="ssid" id="ssid" value=")rawliteral";

  html += escapeHtml(nvsSSID);
  html += R"rawliteral(">
<label>WiFi パスワード</label>
<input type="password" name="pass" value=")rawliteral";

  html += escapeHtml(nvsPass);
  html += R"rawliteral(">
<label>Discord Webhook URL</label>
<input type="text" name="webhook" value=")rawliteral";

  html += escapeHtml(nvsWebhookURL);
  html += R"rawliteral(" required>
<label>送信メッセージ</label>
<input type="text" name="message" value=")rawliteral";

  html += escapeHtml(nvsMessage.isEmpty() ? "パパ、トイレットペーパーを買ってきて" : nvsMessage);
  html += R"rawliteral(" required>
<button type="submit">保存して再起動</button>
</form>
</div>
<script>
(function(){
  var sel=document.getElementById('ssid_select');
  var hid=document.getElementById('ssid');
  if(sel.value && sel.value!=='__manual__'){hid.value=sel.value;}
})();
</script>
</body>
</html>)rawliteral";

  return html;
}

String buildSuccessPage() {
  return R"rawliteral(<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>PapaDash 設定完了</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#f0f2f5;padding:16px;font-size:16px}
.card{background:#fff;border-radius:12px;padding:24px;max-width:400px;margin:0 auto;box-shadow:0 2px 8px rgba(0,0,0,0.1);text-align:center}
h1{font-size:20px;color:#333;margin-bottom:12px}
p{color:#666;font-size:14px}
.ok{background:#57F287;color:#333;padding:16px;border-radius:8px;font-weight:600;margin-bottom:16px}
</style>
</head>
<body>
<div class="card">
<div class="ok">設定を保存しました</div>
<h1>再起動します...</h1>
<p>3秒後にESP32が再起動します。<br>WiFiに自動接続されます。</p>
</div>
</body>
</html>)rawliteral";
}

// --- NVS操作 ---

void loadSettings() {
  prefs.begin("papa-dash", true);  // read-only
  nvsSSID = prefs.getString("wifi_ssid", "");
  nvsPass = prefs.getString("wifi_pass", "");
  nvsWebhookURL = prefs.getString("webhook_url", "");
  nvsMessage = prefs.getString("message", "");
  prefs.end();

  // NVSが空ならconfig.hの値をデフォルトとして使用
  if (nvsSSID.isEmpty()) nvsSSID = CONFIG_WIFI_SSID;
  if (nvsPass.isEmpty()) nvsPass = CONFIG_WIFI_PASS;
  if (nvsWebhookURL.isEmpty()) nvsWebhookURL = CONFIG_WEBHOOK_URL;
  if (nvsMessage.isEmpty()) nvsMessage = CONFIG_MESSAGE;
}

bool isConfigured() {
  prefs.begin("papa-dash", true);
  bool cfg = prefs.getBool("configured", false);
  prefs.end();
  return cfg;
}

void saveSettings(const String& ssid, const String& pass, const String& webhook, const String& message) {
  prefs.begin("papa-dash", false);  // read-write
  prefs.putString("wifi_ssid", ssid);
  prefs.putString("wifi_pass", pass);
  prefs.putString("webhook_url", webhook);
  prefs.putString("message", message);
  prefs.putBool("configured", true);
  prefs.end();
  Serial.println("[NVS] Settings saved.");
}

// --- LED制御 ---

void updateLED() {
  if (!inSetupMode) return;
  unsigned long now = millis();
  if (now - ledLastToggle >= LED_SETUP_BLINK) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    ledLastToggle = now;
  }
}

void ledFeedbackSuccess() {
  digitalWrite(LED_PIN, HIGH);
  delay(1400);
  digitalWrite(LED_PIN, LOW);
}

void ledFeedbackFailure() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
}

// --- DeepSleep ---

void enterDeepSleep() {
  Serial.println("[Sleep] Entering DeepSleep...");
  digitalWrite(LED_PIN, LOW);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  // ESP32-C3: GPIO wakeup（GPIO0-5のみ対応）
  gpio_pullup_en((gpio_num_t)BUTTON_PIN);
  gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);
  esp_deep_sleep_enable_gpio_wakeup(BIT(BUTTON_PIN), ESP_GPIO_WAKEUP_GPIO_LOW);

  Serial.println("[Sleep] Goodnight.");
  Serial.flush();
  esp_deep_sleep_start();
}

// --- ボタン押下タイプ検出 ---

enum PressType { PRESS_SHORT, PRESS_LONG };

PressType detectPressType() {
  Serial.println("[Press] Detecting press type...");
  unsigned long start = millis();
  unsigned long lastLow = millis();  // 最後にLOWを検出した時刻

  // 5秒間ボタン状態を監視（チャタリング耐性あり）
  while ((millis() - start) < LONG_PRESS_THRESHOLD) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      lastLow = millis();
    } else if ((millis() - lastLow) > 200) {
      // 200ms以上連続でHIGH → ボタンが離された
      break;
    }
    delay(10);
  }

  if ((millis() - start) >= LONG_PRESS_THRESHOLD) {
    Serial.println("[Press] Long press detected (>=5s).");
    return PRESS_LONG;
  }

  unsigned long held = millis() - start;
  Serial.printf("[Press] Short press detected (%lums).\n", held);
  return PRESS_SHORT;
}

// --- Discord送信 ---

bool sendDiscordMessage(const char* webhookUrl, const char* message) {
  Serial.printf("[Discord] Free heap before: %d\n", ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  HTTPClient http;
  http.setTimeout(10000);
  http.begin(client, webhookUrl);
  http.addHeader("Content-Type", "application/json");

  // JSONエスケープ（簡易版）
  String msg = String(message);
  msg.replace("\\", "\\\\");
  msg.replace("\"", "\\\"");
  String payload = "{\"content\":\"" + msg + "\"}";
  Serial.println("[Discord] Sending...");

  int httpCode = http.POST(payload);
  Serial.printf("[Discord] HTTP %d\n", httpCode);

  http.end();
  client.stop();

  Serial.printf("[Discord] Free heap after: %d\n", ESP.getFreeHeap());

  return (httpCode >= 200 && httpCode < 300);
}

// --- WiFi接続 ---

bool connectWiFi() {
  Serial.printf("[WiFi] Connecting to: %s\n", nvsSSID.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(nvsSSID.c_str(), nvsPass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("[WiFi] Connection failed!");
  return false;
}

// --- 短押し処理 ---

void handleShortPress() {
  Serial.println("=== SHORT PRESS: Send Discord message ===");

  if (!connectWiFi()) {
    Serial.println("[ShortPress] WiFi failed.");
    ledFeedbackFailure();
    enterDeepSleep();
    return;
  }

  delay(500);
  bool success = sendDiscordMessage(nvsWebhookURL.c_str(), nvsMessage.c_str());

  if (success) {
    Serial.println("[ShortPress] Message sent successfully.");
    ledFeedbackSuccess();
  } else {
    Serial.println("[ShortPress] Message send failed.");
    ledFeedbackFailure();
  }

  enterDeepSleep();
}

// --- Webサーバーハンドラ ---

void handleRoot() {
  server.send(200, "text/html", buildConfigPage());
}

void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  String webhook = server.arg("webhook");
  String message = server.arg("message");

  Serial.printf("[Save] SSID=%s, Webhook=%s, Message=%s\n",
                ssid.c_str(), webhook.c_str(), message.c_str());

  saveSettings(ssid, pass, webhook, message);

  server.send(200, "text/html", buildSuccessPage());

  delay(3000);
  ESP.restart();
}

// キャプティブポータル検出URLへの応答
void handleCaptivePortal() {
  Serial.printf("[Captive] Redirecting: %s\n", server.uri().c_str());
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "");
}

void handleNotFound() {
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "");
}

// --- 設定モード開始 ---

void scanWiFiNetworks() {
  Serial.println("[Scan] Scanning WiFi networks...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  Serial.printf("[Scan] Found %d networks.\n", n);

  scannedSSIDs = "";
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;  // 非表示SSID除外

    // 重複排除
    if (scannedSSIDs.indexOf("value=\"" + escapeHtml(ssid) + "\"") >= 0) continue;

    int rssi = WiFi.RSSI(i);
    // RSSI(-30〜-90dBm)を10段階に変換
    int level = constrain(map(rssi, -90, -30, 1, 10), 1, 10);

    bool selected = (ssid == nvsSSID);
    scannedSSIDs += "<option value=\"" + escapeHtml(ssid) + "\"";
    if (selected) scannedSSIDs += " selected";
    scannedSSIDs += ">" + escapeHtml(ssid) + " (" + String(level) + "/10)</option>\n";
  }

  WiFi.scanDelete();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
}

void startSetupMode() {
  inSetupMode = true;
  setupStartTime = millis();
  Serial.println("=== SETUP MODE ===");

  // AP起動前にWiFiスキャン
  scanWiFiNetworks();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  delay(100);

  Serial.print("[AP] IP: ");
  Serial.println(WiFi.softAPIP());

  // DNSサーバー：全ドメインを自分に向ける
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // キャプティブポータル検出URL
  server.on("/hotspot-detect.html", handleCaptivePortal);
  server.on("/generate_204", handleCaptivePortal);
  server.on("/gen_204", handleCaptivePortal);
  server.on("/connecttest.txt", handleCaptivePortal);
  server.on("/ncsi.txt", handleCaptivePortal);
  server.on("/fwlink", handleCaptivePortal);
  server.on("/redirect", handleCaptivePortal);
  server.on("/canonical.html", handleCaptivePortal);
  server.on("/success.txt", handleCaptivePortal);

  // メインページ
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[Web] Server started on port 80");
  Serial.println("[Setup] Connect to WiFi: " AP_SSID);
  Serial.printf("[Setup] Timeout in %d seconds.\n", SETUP_TIMEOUT / 1000);
}

// --- Arduino setup/loop ---

void setup() {
  // ボタンピンを設定し、1秒間連続監視（DeepSleep復帰後のGPIO安定化対応）
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  bool buttonPressed = false;
  for (int i = 0; i < 100; i++) {  // 10ms × 100 = 1000ms
    if (digitalRead(BUTTON_PIN) == LOW) {
      buttonPressed = true;
      break;
    }
    delay(10);
  }

  Serial.begin(115200);
  delay(100);

  Serial.println("=== Papa Dash (XIAO ESP32C3) ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // NVS設定読み込み
  loadSettings();

  esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  Serial.printf("[Boot] Wakeup cause: %d, button: %s\n", wakeupCause, buttonPressed ? "pressed" : "released");

  if (wakeupCause == ESP_SLEEP_WAKEUP_GPIO) {
    // ボタン押下による起床
    Serial.println("[Boot] Woke up by button press.");
    PressType pt = buttonPressed ? detectPressType() : PRESS_SHORT;
    if (pt == PRESS_SHORT) {
      if (isConfigured() && !nvsSSID.isEmpty()) {
        handleShortPress();  // Discord送信 → DeepSleep（この関数から戻らない）
      } else {
        Serial.println("[Boot] Not configured, entering setup mode.");
        startSetupMode();
      }
    } else {
      // 長押し → 設定モード（即開始、ボタンリリースはloop()で待つ）
      Serial.println("[Boot] Long press → Setup mode.");
      waitForButtonRelease = true;
      startSetupMode();
    }
  } else {
    // コールドブート（電源ON / 設定保存後の再起動）
    Serial.println("[Boot] Cold boot.");
    if (isConfigured() && !nvsSSID.isEmpty()) {
      Serial.println("[Boot] Already configured. Entering DeepSleep immediately.");
      enterDeepSleep();
    } else {
      // config.hに有効な設定があればNVSに自動書き込み
      if (String(CONFIG_WIFI_SSID) != "YOUR_SSID" && String(CONFIG_WEBHOOK_URL).indexOf("YOUR_WEBHOOK_URL") < 0) {
        Serial.println("[Boot] Loading settings from config.h...");
        saveSettings(CONFIG_WIFI_SSID, CONFIG_WIFI_PASS, CONFIG_WEBHOOK_URL, CONFIG_MESSAGE);
        loadSettings();
        Serial.println("[Boot] Config.h applied. Entering DeepSleep.");
        enterDeepSleep();
      } else {
        Serial.println("[Boot] No configuration found. Starting setup mode.");
        startSetupMode();
      }
    }
  }
}

void loop() {
  if (!inSetupMode) {
    // 通常ここには来ないが、安全のためDeepSleepへ
    enterDeepSleep();
    return;
  }

  // 設定モード
  dnsServer.processNextRequest();
  server.handleClient();
  updateLED();

  // 長押しからの設定モード開始時、ボタンが離されるまで待つ
  if (waitForButtonRelease) {
    if (digitalRead(BUTTON_PIN) == HIGH) {
      delay(200);  // チャタリング防止
      waitForButtonRelease = false;
      Serial.println("[Setup] Button released. Ready for input.");
    }
  } else {
    // ボタン短押しでDeepSleepに戻る（ボタンが離されてからDeepSleep）
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);  // チャタリング防止
      if (digitalRead(BUTTON_PIN) == LOW) {
        Serial.println("[Setup] Button pressed. Waiting for release...");
        while (digitalRead(BUTTON_PIN) == LOW) {
          delay(50);
        }
        delay(200);  // チャタリング防止
        Serial.println("[Setup] Entering DeepSleep.");
        enterDeepSleep();
      }
    }
  }

  // タイムアウト判定
  if ((millis() - setupStartTime) >= SETUP_TIMEOUT) {
    Serial.println("[Setup] Timeout! Entering DeepSleep.");
    enterDeepSleep();
  }

  delay(10);
}
