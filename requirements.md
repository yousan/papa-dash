仕様書：Discord通知用 IoT ボタン（Dash思想・自作）

1. 目的・背景
	•	物理ボタンを押すだけで Discord に通知を送信したい
	•	Amazon Dash Button の思想（単機能・低コスト・確実性）を再現したい
	•	スマホは 設定時のみ使用、通常運用では不要
	•	自作前提だが、過剰な汎用性は不要

⸻

2. 全体コンセプト（設計思想）
	•	単機能：ボタン押下 → Discord通知
	•	確実性優先：通信はデバイス単体で完結
	•	低消費電力：普段は DeepSleep、押した時だけ通信
	•	設定の簡素化：キャプティブポータル（AP モード）でスマホから初期設定
	•	低コスト：量産は考えず、個人自作で現実的な範囲

⸻

3. システム構成

3.1 構成概要

[物理ボタン（GPIO4）]
     ↓ ext0 wakeup
[ESP32 DeepSleep → 起床]
  ├─ 短押し（<5秒）
  │    ├─ Wi-Fi STA接続
  │    ├─ Discord Webhook HTTPS POST
  │    └─ DeepSleep復帰
  ├─ 長押し（≥5秒）
  │    ├─ AP モード起動（設定用）
  │    ├─ キャプティブポータル（WebServer）
  │    └─ 5分タイムアウト → DeepSleep
  └─ LED（GPIO2）フィードバック

3.2 通信の役割分担

| 通信 | 用途 | 備考 |
|---|---|---|
| Wi-Fi STA | Discord通知送信 | ボタン短押し時のみ起動 |
| Wi-Fi AP | 設定用キャプティブポータル | 長押し時 or 未設定時のみ起動 |
| スマホ | 設定時のみ（APに接続） | 常駐不要 |

⸻

4. ハードウェア仕様

4.1 使用マイコン
	•	ESP32（ESP32-WROVER開発ボード）
	•	必須機能
	•	Wi-Fi（HTTPS対応）
	•	DeepSleep + ext0 wakeup
	•	GPIO4（外部ボタン、RTC_GPIO10）

4.2 入出力

| I/O | GPIO | 用途 |
|---|---|---|
| 入力 | GPIO4 | 外部ボタン（ext0 wakeup / 短押し・長押し検出） |
| 出力 | GPIO2 | LED（状態フィードバック） |

4.3 電源
	•	想定
	•	USB給電（開発・試作）
	•	電池運用（単三 / 単四 / LiPo）
	•	要件
	•	DeepSleep 時は極低消費電力（LED消灯、WiFi OFF）
	•	押下時のみ Wi-Fi 起動

⸻

5. ソフトウェア仕様（デバイス側）

5.1 動作モード

通常動作（短押し）
	1.	DeepSleep
	2.	ボタン押下で起床（ext0 wakeup, GPIO4 LOW）
	3.	detectPressType() で保持時間を計測
	4.	5秒未満で離す → 短押し判定
	5.	Wi-Fi STA 接続（20秒タイムアウト）
	6.	Discord Webhook に HTTPS POST
	7.	成功: LED 1秒点灯 / 失敗: LED 3回点滅
	8.	DeepSleep に戻る

設定モード（長押し or 未設定）
	1.	ボタン5秒以上長押し or 未設定でのコールドブート
	2.	Wi-Fi AP起動（SSID: PapaDash-Setup）
	3.	DNSServer + WebServer でキャプティブポータル提供
	4.	ブラウザから設定入力 → NVS保存 → ESP.restart()
	5.	5分タイムアウトで自動 DeepSleep

コールドブート時
	•	設定済み → 即DeepSleep（ボタン待ち状態へ）
	•	未設定 → 設定モード起動

⸻

5.2 設定項目（キャプティブポータル経由）
	•	Wi-Fi SSID
	•	Wi-Fi パスワード
	•	Discord Webhook URL
	•	送信メッセージ（固定文字列）

※ 設定値は NVS（Preferences ライブラリ）に永続化
※ `configured` フラグで設定済み判定

⸻

5.3 Discord送信仕様
	•	通信方式：HTTPS POST（WiFiClientSecure, setInsecure()）
	•	宛先：Discord Webhook URL（NVSから取得）
	•	ペイロード：

```json
{
  "content": "（設定されたメッセージ）"
}
```

	•	成功判定：HTTP レスポンスコード 200〜299
	•	タイムアウト：10秒

⸻

5.4 LEDフィードバック仕様

| 状態 | LED動作 | 間隔 |
|---|---|---|
| DeepSleep中 | 消灯 | - |
| 長押し検出中 | 超高速点滅 | 100ms |
| Discord送信成功 | 1秒点灯→消灯 | - |
| Discord送信失敗 / WiFi失敗 | 3回短く点滅→消灯 | 150ms on/off × 3 |
| 設定モード | ゆっくり点滅 | 1000ms |

⸻

5.5 定数一覧

| 定数名 | 値 | 説明 |
|---|---|---|
| LED_PIN | 2 | LED出力ピン |
| BUTTON_PIN | 4 | ボタン入力ピン（外部スイッチ、RTC_GPIO10） |
| AP_SSID | "PapaDash-Setup" | 設定モードのAP名 |
| WIFI_CONNECT_TIMEOUT | 20000ms | WiFi接続タイムアウト |
| LONG_PRESS_THRESHOLD | 5000ms | 長押し判定閾値 |
| SETUP_TIMEOUT | 300000ms | 設定モード自動終了（5分） |
| LED_FAST_BLINK | 100ms | 長押し検出中の点滅間隔 |
| LED_SETUP_BLINK | 1000ms | 設定モードの点滅間隔 |

⸻

5.6 関数一覧

| 関数 | 戻り値 | 説明 |
|---|---|---|
| loadSettings() | void | NVSから設定読み込み |
| isConfigured() | bool | NVSのconfiguredフラグ確認 |
| saveSettings() | void | NVSに設定書き込み |
| enterDeepSleep() | void | LED消灯→WiFi OFF→ext0設定→DeepSleep |
| detectPressType() | PressType | ボタン保持時間を計測、SHORT/LONG判定 |
| connectWiFi() | bool | WiFi STA接続（タイムアウト付き） |
| sendDiscordMessage() | bool | Webhook送信、HTTP 2xxで成功 |
| handleShortPress() | void | WiFi接続→Discord送信→LED→DeepSleep |
| startSetupMode() | void | AP起動→WebServer開始 |
| updateLED() | void | 設定モード用LED点滅制御 |
| ledFeedbackSuccess() | void | LED 1秒点灯 |
| ledFeedbackFailure() | void | LED 3回点滅 |

⸻

6. セキュリティ考慮
	•	AP（PapaDash-Setup）は長押し時 or 未設定時のみ起動
	•	通常運用時にAPが立ち上がることはない
	•	Discord Webhook URLはNVSに保存（平文だがデバイス内蔵）
	•	HTTPS通信はsetInsecure()使用（証明書検証なし、ESP32メモリ制約のため）

⸻

7. 開発環境

7.1 開発OS
	•	macOS

7.2 デバイス開発
	•	PlatformIO（Arduino Framework）
	•	言語：C++（Arduinoスタイル）

7.3 スマホ側
	•	ネイティブアプリ不要
	•	標準ブラウザでキャプティブポータルにアクセス

⸻

8. コスト目安（1台）

| 構成 | 目安コスト |
|---|---|
| 最小構成（USB給電） | 700〜1,300円 |
| 電池運用・実用 | 1,200〜2,500円 |
| ケース込み | 〜3,000円程度 |

※ Amazon Dash（実質0円）には及ばないが、思想的には近い

⸻

9. 非対応・割り切り事項
	•	BLE設定は不採用（キャプティブポータルに変更済み）
	•	汎用IoTプラットフォーム化はしない
	•	複数ボタン / 複雑なUIは対象外
	•	技適・製品販売は現時点では考慮しない
	•	HTTPS証明書検証は省略（ESP32メモリ制約）

⸻

10. まとめ（設計の芯）
	•	通信は Wi-Fi
	•	設定は キャプティブポータル（AP モード）
	•	普段は DeepSleep で眠らせる
	•	ボタン1個で完結
	•	長押しでいつでも再設定可能

これは

Amazon Dash Button を、個人が2026年に現実的に再設計した形

となる設計である。
