# Papa Dash - Discord通知用 IoT ボタン

Amazon Dash Button の思想を再現した、ESP32ベースのDiscord通知デバイスプロジェクト。

## プロジェクト概要

物理ボタンを押すだけでDiscordに通知を送信する、シンプルで確実なIoTデバイス。

- **単機能**: ボタン押下 → Discord通知
- **確実性優先**: 通信はデバイス単体で完結
- **低消費電力**: 普段はDeep Sleep、押した時だけ通信（予定）
- **設定の簡素化**: BLEを使いスマホから初期設定（予定）

詳細な仕様は [requirements.md](requirements.md) を参照。

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

#### ボタン

- ボード上の **BOOT/USERボタン**（GPIO0）を使用

### 4. ビルド＆アップロード

```bash
cd esp32_test
pio run --target upload
```

### 5. シリアルモニタ（Arduino IDE）

1. Arduino IDEを開く
2. 右上の虫眼鏡アイコンをクリック
3. ボーレートを **115200** に設定
4. メッセージを確認

## 動作確認済みの機能

### ✅ 基本I/O

- **GPIO出力**: LED点滅制御（GPIO2）
- **GPIO入力**: ボタン検出（GPIO0）
- **シリアル通信**: 115200 baud

### ✅ ボタン入力テスト

現在の実装：

```
BOOTボタンを押す → LEDが高速で3秒間点滅（0.1秒間隔）
```

**テスト方法**:
1. プログラムをアップロード
2. ボード上のBOOT/USERボタンを押す
3. 外部LED（GPIO2）が高速で3秒間点滅することを確認

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

1. **環境構築**
   - ESP32ボードパッケージのインストール
   - PlatformIOのセットアップ
   - タイムアウト問題の解決

2. **最小限のテスト**
   - LED点滅（GPIO2）
   - シリアル通信確認

3. **ボタン入力テスト**
   - BOOTボタン（GPIO0）の読み取り
   - ボタン押下でLED高速点滅（3秒間）

### Phase 2: ネットワーク機能（予定）

- [ ] Wi-Fi接続テスト
- [ ] Discord Webhook送信テスト
- [ ] ボタン → Discord通知の統合

### Phase 3: 最終実装（予定）

- [ ] BLE設定機能
- [ ] Deep Sleep実装
- [ ] 電池駆動対応

## トラブルシューティング

### Arduino IDEでアップロードエラー

**症状**: `Failed uploading: uploading error: exit status 2`

**原因**: 自動リセットの問題、またはボード選択の誤り

**解決方法**:
- PlatformIOを使用（`pio run --target upload`）
- ボード選択を確認（ESP32 Dev Module）
- アップロード速度を下げる（platformio.iniで`upload_speed = 115200`）

### ボードパッケージのインストールタイムアウト

**症状**: `Error: 4 DEADLINE_EXCEEDED`

**解決方法**:
- Arduino IDEの設定でタイムアウト値を延長
- 参考: https://esp32.com/viewtopic.php?t=47384

### LEDが点滅しない

**確認事項**:
1. シリアルモニタで「LED ON」「LED OFF」が表示されているか
2. LED接続が正しいか（極性、抵抗値）
3. GPIOピン番号が正しいか
4. ボード上の内蔵LEDではなく、外部LEDを使用しているか

### ボード検出の問題

**確認コマンド**:
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
pio run --target clean
pio run

# シリアルモニタ（ターミナルから使う場合）
# ※Arduino IDEのシリアルモニタ推奨
```

### Git操作

```bash
# ステータス確認
git status

# 変更をコミット
git add .
git commit -m "コミットメッセージ"

# プッシュ
git push origin master
```

## 次のステップ

1. **Wi-Fi接続テスト**
   - ESP32をWi-Fiに接続
   - 接続状態の確認

2. **Discord Webhook送信**
   - HTTPS POSTリクエスト
   - Discord通知の送信確認

3. **統合テスト**
   - ボタン押下 → Wi-Fi接続 → Discord通知
   - エラーハンドリング

4. **Deep Sleep実装**
   - 低消費電力モード
   - ボタン割り込みで起動

5. **BLE設定機能**
   - スマホアプリから設定
   - Wi-Fi認証情報の保存

## ライセンス

MIT License

## 開発者

- [@yousan](https://github.com/yousan)
- Co-developed with Claude Opus 4.5

---

**最終更新**: 2026-02-05
