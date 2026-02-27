# Papa Dash - Discord通知用 IoT ボタン

Amazon Dash Button の思想を再現した、ESP32-C3ベースのDiscord通知デバイスプロジェクト。

## プロジェクト概要

物理ボタンを押すだけでDiscordに通知を送信する、シンプルで確実なIoTデバイス。

- **単機能**: ボタン押下 → Discord通知
- **確実性優先**: 通信はデバイス単体で完結
- **低消費電力**: DeepSleep実装済み。ボタン押下時のみ起床・通信
- **設定の簡素化**: config.hで事前設定 or キャプティブポータルでスマホから設定
- **WiFiスキャン**: 設定画面でSSIDを一覧表示（電波強度10段階表示）

詳細な仕様は [requirements.md](requirements.md) を参照。

## 動作フロー

```
[DeepSleep] ──(ボタン押下で起床)──→ setup()
  │
  ├─ GPIOウェイクアップ（ボタン押下）
  │    ├─ 短押し（<5秒）→ WiFi接続 → Discord送信 → LED 1.4秒点灯 → DeepSleep
  │    └─ 長押し（≥5秒）→ LED点滅開始 → WiFiスキャン → AP起動（設定モード）
  │         ├─ ボタン短押し → DeepSleep（Discord送信なし）
  │         └─ 5分タイムアウト → DeepSleep
  │
  └─ コールドブート（電源ON / 設定保存後の再起動）
       ├─ 設定済み → 即DeepSleep（ボタン待ち状態へ）
       ├─ config.hに設定あり → NVSに自動書き込み → DeepSleep
       └─ 未設定 → 設定モード起動
```

## LEDポリシー

| 状態 | LED |
|---|---|
| DeepSleep中 | 消灯 |
| 長押し検出中（5秒間） | 消灯（静かに待機） |
| 長押し検出完了 | 1秒間隔点滅（設定モード移行） |
| Discord送信成功 | 1.4秒点灯→消灯 |
| Discord送信失敗 | 3回短く点滅→消灯 |
| 設定モード | 1秒間隔点滅 |

## 開発環境

### macOS環境

- **OS**: macOS
- **開発ツール**: PlatformIO

### ハードウェア

- **マイコンボード**: Seeed XIAO ESP32C3
  - チップ: ESP32-C3 (RISC-V, 160MHz)
  - Wi-Fi, BLE
  - USB-Serial/JTAG内蔵
- **外部部品**:
  - LED + 抵抗（220Ω）
  - タクトスイッチ
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

XIAO ESP32C3 のピン配置と配線:

```
        USB
    ┌───────────┐
D0  │●         ●│ 5V
D1  │●──LED     ●│ GND ←── ボタン片足 & LED(-)
D2  │●──ボタン  ●│ 3V3
D3  │●         ●│ D10
D4  │●         ●│ D9
D5  │●         ●│ D8
D6  │●         ●│ D7
    └───────────┘
```

#### LED接続（D1 / GPIO3）

```
D1 → 抵抗（220Ω） → LEDの長い足（+） → LEDの短い足（-） → GND
```

#### ボタン接続（D2 / GPIO4）

```
D2 → タクトスイッチ → GND（内部プルアップ使用、外部抵抗不要）
```

**注意**: XIAO ESP32C3 のピン表記（D0, D1...）とGPIO番号は異なります:

| ボード表記 | GPIO番号 | 用途 |
|---|---|---|
| D0 | GPIO2 | - |
| D1 | GPIO3 | LED |
| D2 | GPIO4 | ボタン |
| D3 | GPIO5 | - |
| D4 | GPIO6 | - |
| D5 | GPIO7 | - |

### 4. 設定ファイルの準備

```bash
cp src/config.h.example src/config.h
```

`config.h` を編集して、WiFiとDiscordの設定を入力:

```cpp
#define CONFIG_WIFI_SSID "あなたのSSID"
#define CONFIG_WIFI_PASS "あなたのパスワード"
#define CONFIG_WEBHOOK_URL "https://discord.com/api/webhooks/..."
#define CONFIG_MESSAGE "パパ、トイレットペーパーを買ってきて"
```

`config.h` はgitignoreされているため、認証情報がリポジトリに含まれることはありません。

### 5. ビルド＆アップロード

```bash
pio run --target upload
```

**DeepSleep後にアップロードできない場合**:

XIAO ESP32C3 はDeepSleepに入るとUSBポートが消えます。以下の手順でブートローダーモードに入ってください:

1. **BOOTボタンを押しながら** RESETボタンを押して離す
2. BOOTボタンを離す
3. `pio run --target upload` を実行

### 6. 初回起動

config.h に設定を書いた場合:
1. アップロード後、自動でconfig.hの設定がNVSに保存される
2. DeepSleepに入る（ボタン待ち状態）
3. **ボタン（D2）を短く押す** → Discord にメッセージ送信

config.h を使わない場合（キャプティブポータル）:
1. 初回起動時、ESP32が自動的に設定モードに入る
2. スマホまたはPCで WiFi「**PapaDash-Setup**」に接続
3. キャプティブポータルが自動で開く（開かない場合は `http://192.168.4.1/` にアクセス）
4. WiFi SSIDをドロップダウンから選択（電波強度表示付き）
5. WiFiパスワード、Discord Webhook URL、送信メッセージを入力
6. 「保存して再起動」をタップ → DeepSleep（ボタン待ち状態）

### 7. 通常使用

- **ボタンを短く押す** → Discord にメッセージ送信（LED 1.4秒点灯で成功確認）
- **ボタンを6秒以上長押し** → 設定モードに入る（LED点滅開始で確認、設定変更時）
- **設定モード中にボタンを短く押す** → 設定モードを終了しDeepSleepに戻る

## プロジェクト構成

```
papa-dash/
├── README.md                    # このファイル
├── requirements.md              # 詳細仕様書
├── .gitignore                   # Git除外設定
├── esp32_test/                  # PlatformIOプロジェクト
│   ├── platformio.ini          # PlatformIO設定
│   └── src/
│       ├── main.cpp            # メインプログラム
│       ├── config.h            # WiFi/Webhook設定（gitignore）
│       └── config.h.example    # 設定ファイルのテンプレート
└── sketch_feb5b/               # Arduinoスケッチ（参考・旧）
    └── sketch_feb5b.ino
```

## トラブルシューティング

### ボタンを押してもDeepSleepから起きない

- ボタンがD2（GPIO4）とGNDに正しく配線されているか確認
- DeepSleep wakeupはGPIO0-5のみ対応。D4（GPIO6）以降は使用不可
- ESP32-C3は起動が遅いため、短押しだとボタンが既に離されていることがある（正常動作）

### アップロードできない（ポートが見つからない）

- DeepSleep中はUSBポートが消える
- **BOOTボタンを押しながらRESET** でブートローダーモードに入る
- Bluetoothポートを掴む場合は `--upload-port /dev/cu.usbmodem1101` を明示指定

### LEDが反応しない

- LEDがD1（GPIO3）に接続されているか確認
- GPIO6以降はDeepSleep中にHIGHになるため、LEDはGPIO0-5に接続すること

### 設定モードに入れない

- ボタンを**6秒以上**押し続ける必要がある（起動時間+5秒判定）
- 5秒経過でLEDが1秒間隔点滅を開始すれば設定モード移行成功
- 途中で離すと短押し（Discord送信）として処理される

### WiFiに接続できない

- SSIDとパスワードが正しいか確認（設定モードまたはconfig.hで再設定）
- ESP32とルーターの距離を確認
- 20秒のタイムアウト後、LED3回点滅→DeepSleepに戻る

### NVSクリア方法

設定をリセットしたい場合、`setup()` の先頭に一時的に追加してアップロード:

```cpp
prefs.begin("papa-dash", false);
prefs.clear();
prefs.end();
```

## コマンドリファレンス

```bash
# ビルド
pio run

# アップロード
pio run --target upload

# ポート指定アップロード
pio run --target upload --upload-port /dev/cu.usbmodem1101

# クリーンビルド
pio run --target clean && pio run

# シリアルモニタ
pio device monitor --baud 115200
```

## 開発履歴

### v1.0: ESP32-WROVER版（完了）

- 基本動作確認（LED点滅、ボタン入力）
- Wi-Fi接続、Discord Webhook送信
- キャプティブポータル設定UI
- DeepSleep + ext0 ウェイクアップ

### v2.0: XIAO ESP32C3版（現在）

- Seeed XIAO ESP32C3 に移植
- DeepSleep API変更（ext0 → GPIO wakeup）
- WiFiスキャン機能追加（SSID一覧、電波強度10段階表示）
- config.hによる設定プリセット機能
- LED/ボタンのGPIO最適化（GPIO0-5範囲に配置）
- 長押し検出のチャタリング耐性改善（200msデバウンス）
- 起動時1秒間のGPIO安定化監視（DeepSleep復帰対応）
- 設定モードからのボタン終了機能（Discord送信なし）

## ライセンス

MIT License

## 開発者

- [@yousan](https://github.com/yousan)
- Co-developed with Claude Opus 4.5 / 4.6

---

**最終更新**: 2026-02-27
