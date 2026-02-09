# Papa Dash - Discord通知用 IoT ボタン

Amazon Dash Button の思想を再現した、ESP32ベースのDiscord通知デバイスプロジェクト。

## プロジェクト概要

物理ボタンを押すだけでDiscordに通知を送信する、シンプルで確実なIoTデバイス。

- **単機能**: ボタン押下 → Discord通知
- **確実性優先**: 通信はデバイス単体で完結
- **低消費電力**: DeepSleep実装済み。ボタン押下時のみ起床・通信
- **設定の簡素化**: キャプティブポータル（AP モード）でスマホから初期設定

詳細な仕様は [requirements.md](requirements.md) を参照。

## 動作フロー

```
[DeepSleep] ──(ボタン押下で起床)──→ setup()
  │
  ├─ EXT0ウェイクアップ（ボタン押下）
  │    ├─ detectPressType() でボタン保持時間を計測
  │    ├─ 短押し（<5秒）→ WiFi接続 → Discord送信 → LED通知 → DeepSleep
  │    └─ 長押し（≥5秒）→ AP起動（設定モード）→ WebServer → 5分タイムアウト→DeepSleep
  │
  └─ コールドブート（電源ON / 設定保存後の再起動）
       ├─ 設定済み → 即DeepSleep（ボタン待ち状態へ）
       └─ 未設定 → 設定モード起動
```

## LEDポリシー

| 状態 | LED |
|---|---|
| DeepSleep中 | 消灯 |
| 長押し検出中 | 超高速点滅（100ms間隔） |
| Discord送信成功 | 1秒点灯→消灯 |
| Discord送信失敗 | 3回短く点滅→消灯 |
| 設定モード | 1秒間隔点滅 |

## 開発環境

### macOS環境

- **OS**: macOS（Darwin 25.2.0）
- **開発ツール**: PlatformIO（6.1.19）
- **Arduino IDE**: 2.3.7（参考用、実際の開発はPlatformIO）

### ハードウェア

- **マイコンボード**: ESP32-WROVER開発ボード
  - チップ: ESP32-D0WDQ6 (revision v1.0)
  - Wi-Fi, BT, Dual Core, 240MHz
  - シリアル: `/dev/cu.usbserial-1120`
- **外部部品**:
  - LED（赤）+ 抵抗（220Ω）
  - ブレッドボード
  - ジャンパーワイヤー

## セットアップ

### 1. PlatformIOのインストール

```bash
brew install platformio
```

### 2. プロジェクトのクローン

```bash
git clone https://github.com/yousan/papa-dash.git
cd papa-dash/esp32_test
```

### 3. ハードウェア接続

#### LED接続（GPIO2）

```
LED の長い足（アノード） → 抵抗（220Ω） → GPIO2（IO2ピン）
LED の短い足（カソード） → GND
```

#### ボタン（GPIO4）

- 外部タクトスイッチを GPIO4 と GND の間に配線（内部プルアップ使用）
- ※ GPIO0（BOOTボタン）はフラッシュモード専用。DeepSleep wakeup には使用不可（CH340のDTR/RTSと干渉するため）

### 4. ビルド＆アップロード

```bash
cd esp32_test
pio run --target upload
```

### 5. 初期設定（キャプティブポータル）

1. 初回起動時、ESP32が自動的に設定モードに入る
2. スマホまたはPCで WiFi「**PapaDash-Setup**」に接続
3. キャプティブポータルが自動で開く（開かない場合は `http://192.168.4.1/` にアクセス）
4. 以下を入力して「保存して再起動」をタップ：
   - WiFi SSID
   - WiFi パスワード
   - Discord Webhook URL
   - 送信メッセージ
5. ESP32が再起動し、DeepSleepに入る（ボタン待ち状態）

### 6. 通常使用

- **ボタンを短く押す** → Discord にメッセージ送信
- **ボタンを5秒以上長押し** → 設定モードに入る（設定変更時）

## 動作テスト手順

シリアルモニタ（115200 baud）を接続して確認してください。

### テスト1: 未設定でコールドブート

1. NVSをクリアした状態で電源ON（または初回起動）
2. **期待**: 設定モード起動（AP表示、キャプティブポータル動作）
3. シリアル出力: `[Boot] Cold boot.` → `[Boot] No configuration found.` → `=== SETUP MODE ===`

### テスト2: 設定保存→再起動

1. 設定モードでWiFi/Webhook情報を入力して保存
2. **期待**: 再起動後、即DeepSleep
3. シリアル出力: `[Boot] Cold boot.` → `[Boot] Already configured. Entering DeepSleep immediately.`

### テスト3: 短押し（Discord送信）

1. 設定済み状態でボタン（GPIO4）を短く（5秒未満）押して離す
2. **期待**: WiFi接続 → Discord送信 → LED 1秒点灯 → DeepSleep
3. シリアル出力: `[Boot] Woke up by button press.` → `[Press] Short press detected` → `=== SHORT PRESS ===` → `[Discord] HTTP 204` → `[Sleep] Goodnight.`

### テスト4: 長押し（設定モード）

1. ボタン（GPIO4）を5秒以上押し続ける（押している間LEDが超高速点滅する）
2. **期待**: 設定モード起動
3. シリアル出力: `[Press] Long press detected (>=5s).` → `[Boot] Long press → Setup mode.` → `=== SETUP MODE ===`

### テスト5: 設定モードタイムアウト

1. 設定モードに入った後、5分間何もしない
2. **期待**: 自動でDeepSleepに入る
3. シリアル出力: `[Setup] Timeout! Entering DeepSleep.`

### テスト6: WiFi接続失敗

1. 存在しないSSIDを設定した状態で短押し
2. **期待**: LED 3回短く点滅 → DeepSleep
3. シリアル出力: `[WiFi] Connection failed!` → `[ShortPress] WiFi failed.`

### テスト7: Discord送信失敗

1. 無効なWebhook URLを設定した状態で短押し
2. **期待**: LED 3回短く点滅 → DeepSleep
3. シリアル出力: `[Discord] HTTP -1` (またはエラーコード) → `[ShortPress] Message send failed.`

### NVSクリア方法

テスト1を再実施する場合など、NVS設定をクリアしたい場合：

```cpp
// setup()の先頭に一時的に追加してアップロード
prefs.begin("papa-dash", false);
prefs.clear();
prefs.end();
```

## プロジェクト構成

```
papa-dash/
├── README.md                    # このファイル
├── requirements.md              # 詳細仕様書
├── .gitignore                   # Git除外設定
├── esp32_test/                  # PlatformIOプロジェクト
│   ├── platformio.ini          # PlatformIO設定
│   └── src/
│       └── main.cpp            # メインプログラム
└── sketch_feb5b/               # Arduinoスケッチ（参考）
    └── sketch_feb5b.ino
```

## 開発履歴

### Phase 1: 基本動作確認（完了）

1. **環境構築** - ESP32ボードパッケージ、PlatformIOセットアップ
2. **最小限のテスト** - LED点滅（GPIO2）、シリアル通信確認
3. **ボタン入力テスト** - BOOTボタン（GPIO0）の読み取り

### Phase 2: ネットワーク機能（完了）

1. **Wi-Fi接続** - STA モードでの接続・タイムアウト処理
2. **Discord Webhook送信** - HTTPS POST、WiFiClientSecure使用
3. **キャプティブポータル** - AP モード + DNSServer + WebServer による設定UI
4. **NVS永続化** - Preferences ライブラリで設定保存

### Phase 3: 省電力・最終実装（完了）

1. **DeepSleep実装** - ext0 ウェイクアップ（GPIO4 LOW トリガー）
2. **ボタン長押し検出** - 5秒閾値で短押し/長押し判定
3. **LED フィードバック** - 成功/失敗/検出中の視覚通知
4. **設定モードタイムアウト** - 5分で自動DeepSleep

## トラブルシューティング

### ボタンを押してもDeepSleepから起きない

- 外部ボタンがGPIO4とGNDに正しく配線されているか確認
- GPIO4以外のものが接続されていないか確認（LED・抵抗等があるとプルアップが負けて誤動作する）
- `esp_sleep_enable_ext0_wakeup` でGPIO4、LOWトリガーを設定済み

### 設定モードに入れない

- ボタンを**5秒以上**押し続ける必要がある
- 押している間、LEDが超高速点滅（100ms間隔）するのを確認
- 5秒経過前に離すと短押し（Discord送信）として処理される

### WiFiに接続できない

- SSIDとパスワードが正しいか確認（設定モードで再設定）
- ESP32とルーターの距離を確認
- 20秒のタイムアウト後、LED3回点滅→DeepSleepに戻る

### Arduino IDEでアップロードエラー

**症状**: `Failed uploading: uploading error: exit status 2`

**解決方法**:
- PlatformIOを使用（`pio run --target upload`）
- ボード選択を確認（ESP32 Dev Module）
- アップロード速度を下げる（platformio.iniで`upload_speed = 115200`）

### ボード検出の問題

```bash
ls /dev/cu.*
# /dev/cu.usbserial-1120 が表示されることを確認
```

## コマンドリファレンス

### PlatformIO

```bash
# ビルド
pio run

# アップロード
pio run --target upload

# クリーンビルド
pio run --target clean && pio run

# シリアルモニタ
pio device monitor --baud 115200
```

## ライセンス

MIT License

## 開発者

- [@yousan](https://github.com/yousan)
- Co-developed with Claude Opus 4.5 / 4.6

---

**最終更新**: 2026-02-09
