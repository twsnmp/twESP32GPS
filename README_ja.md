# twESP32GPS

**ストラタム 1 NTP サーバー & 衛星ダッシュボード**  
Seeed Studio XIAO ESP32-S3 + VK2828U7G5 GPS モジュール 対応

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Arduino](https://img.shields.io/badge/Arduino-CLI-blue.svg)](https://arduino.github.io/arduino-cli/)

[English README is here](README.md)

---

## 概要

**twESP32GPS** は、[Seeed Studio XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) マイコンボードと VK2828U7G5 マルチ測位対応 GPS モジュールを組み合わせた Arduino ベースのファームウェアです。

小型・低コストの ESP32-S3 ボードを、GPS モジュールからの 1PPS（1 秒パルス）ハードウェア割り込みで同期された **ストラタム 1 NTP タイムサーバー** に変換します。ミリ秒以下の精度で正確な時刻を LAN 内に配信できます。  
さらに、ブラウザベースのダッシュボードで、6 種類の GNSS 測位システムの衛星を リアルタイムのスカイプロットで可視化できます。

### 特長

- **専用の GPS サーバーハードウェアが不要** — 小型の XIAO ESP32-S3 ボード 1 枚で動作
- **1PPS 同期 NTP** — LAN への高精度時刻配信を実現
- **ブラウザでリアルタイム衛星表示** — アプリのインストール不要
- **初回起動時のキャプティブポータルによるゼロ設定 WiFi セットアップ**
- **英語/日本語バイリンガル UI** — ダッシュボードから切り替え可能

---

## 機能一覧

| 機能 | 説明 |
|------|------|
| 🛰️ **ストラタム 1 NTP サーバー** | GPS 同期 + 1PPS ハードウェア割り込みによるミリ秒以下の精度。RFC 4330 / RFC 5905 準拠。 |
| 🌐 **Web ダッシュボード** | Svelte 5 製シングルページアプリ。Canvas 2D によるリアルタイム衛星スカイプロットを表示。LittleFS から gzip 圧縮 HTML を配信。 |
| 📡 **マルチ測位対応** | GPS、GLONASS、Galileo、BeiDou、QZSS、SBAS の衛星を同時追跡。 |
| 🔧 **WiFiManager** | 初回起動時にキャプティブポータルで簡単 WiFi 設定。認証情報のハードコード不要。 |
| 🌍 **英語/日本語バイリンガル UI** | ダッシュボードから実行時に切り替え可能。 |
| 📊 **REST JSON API** | `GET /api/gps` エンドポイントで GPS 状態全体（測位・位置・衛星情報・NTP 統計）を JSON で返す。 |
| ⏱️ **稼働時間・クライアント数追跡** | デバイスの稼働時間と NTP クライアントのリクエスト総数を追跡。 |
| 💾 **LittleFS ストレージ** | フロントエンドアセットを ESP32 フラッシュファイルシステム（LittleFS）に保存し、HTTP で配信。 |

---

## 必要な機材

| 機材 | 数量 | 備考 |
|------|------|------|
| [Seeed Studio XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) | 1 | メインマイコンボード |
| VK2828U7G5 GPS モジュール | 1 | 1PPS 出力対応のマルチ GNSS モジュール |
| USB-C ケーブル | 1 | プログラミングおよび電源供給用 |
| ジャンパーワイヤー | 6 本 | GND、VCC、TX、RX、PPS、EN の 6 接続 |
| ブレッドボード（任意） | 1 | プロトタイピング用 |

> **電源について**: XIAO ESP32-S3 と GPS モジュールはどちらも 3.3V で動作します。GPS の VCC ピンと EN ピンはボードの 3V3 ピンに直接接続します。

---

## 回路図

### 配線表

| GPS ピン | ラベル | XIAO ESP32-S3 ピン | GPIO |
|----------|--------|--------------------|------|
| G | GND | GND | — |
| V | VCC | 3V3 | — |
| T | TX（GPS→ESP） | D7 | GPIO 44 |
| R | RX（ESP→GPS） | D6 | GPIO 43 |
| B | PPS | D1 | GPIO 2 |
| E | EN（イネーブル） | 3V3 | — |

### 配線図（ASCII アート）

```
VK2828U7G5 GPS モジュール       XIAO ESP32-S3
┌─────────────────┐            ┌──────────────────┐
│  G  (GND)  ─────┼────────────┼─ GND             │
│  V  (VCC)  ─────┼────────────┼─ 3V3             │
│  T  (TX)   ─────┼────────────┼─ D7  (GPIO 44)   │
│  R  (RX)   ─────┼────────────┼─ D6  (GPIO 43)   │
│  B  (1PPS) ─────┼────────────┼─ D1  (GPIO 2)    │
│  E  (EN)   ─────┼────────────┼─ 3V3             │
└─────────────────┘            └──────────────────┘
                                      │
                                   USB-C
                                 （PCまたは電源へ）
```

> **1PPS 信号について**: PPS ピンは GPS の正秒タイミングで毎秒 1 回正確なパルスを生成します。ファームウェアはこのパルスを GPIO 2 の `RISING` エッジによるハードウェア割り込みで捕捉し、NTP タイムスタンプを GPS 時刻に同期します。

---

## プロジェクト構成

```
twESP32GPS/
├── mise.toml                    # 開発環境設定（arduino-cli + node）
├── twESP32GPS/                  # Arduino スケッチ
│   ├── twESP32GPS.ino           # メインエントリポイント（setup/loop）
│   ├── config.h                 # ピン定義・定数
│   ├── gps.h / gps.cpp          # GPS 解析（TinyGPS++ + カスタム NMEA GSV）
│   ├── ntp.h / ntp.cpp          # NTP UDP サーバー（RFC 5905、ストラタム 1）
│   ├── web.h / web.cpp          # HTTP サーバー + /api/gps JSON エンドポイント
│   ├── dashboard.h              # gzip 圧縮 Svelte ダッシュボード（自動生成）
│   └── data/                    # LittleFS ファイルシステムルート
│       └── index.html.gz        # Svelte ダッシュボード（scripts/ でビルド）
├── frontend/                    # Svelte 5 + Vite フロントエンドソース
│   ├── src/
│   │   ├── App.svelte           # メイン UI（3 ペインレイアウト）
│   │   ├── components/
│   │   │   └── SkyPlot.svelte   # Canvas 2D 衛星スカイプロット
│   │   ├── i18n.svelte.js       # 英語/日本語翻訳
│   │   └── style.css            # グラスモーフィック ダーク/ライトテーマ
│   └── vite.config.js           # Vite + gzip 圧縮設定
└── scripts/
    ├── install_libs.sh          # Arduino ライブラリ・ボードコアのインストール
    ├── build_frontend.sh        # Svelte → gzip → dashboard.h のビルド
    ├── build_fw.sh              # Arduino ファームウェアのコンパイル
    ├── flash.sh                 # ファームウェア書き込み + LittleFS アップロード
    └── inline_and_convert.js    # アセットインライン化・C++ ヘッダ変換
```

---

## 事前準備（必要なソフトウェア）

### ソフトウェア

| ツール | バージョン | インストール方法 |
|--------|-----------|----------------|
| [arduino-cli](https://arduino.github.io/arduino-cli/) | 最新版 | `brew install arduino-cli`（macOS） |
| [Node.js](https://nodejs.org/) | ≥ 20 | [mise](https://mise.jdx.dev/) または [nvm](https://github.com/nvm-sh/nvm) 経由 |
| [mise](https://mise.jdx.dev/) | 最新版 | `curl https://mise.run \| sh`（任意だが推奨） |

### Arduino ライブラリ（`install_libs.sh` で自動インストール）

| ライブラリ | バージョン |
|-----------|-----------|
| [TinyGPS++](https://github.com/mikalhart/TinyGPSPlus) | 最新版 |
| [ArduinoJson](https://arduinojson.org/) | v7.x |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | 最新版 |
| esp32:esp32 ボードコア | ≥ 3.x |

---

## ビルドと書き込み手順

### 1. リポジトリをクローン

```bash
git clone https://github.com/twsnmp/twESP32GPS.git
cd twESP32GPS
```

### 2. 依存関係のインストール

```bash
# mise のインストール（未インストールの場合）
curl https://mise.run | sh

# 全依存関係インストール: ESP32 ボードコア + Arduino ライブラリ + npm パッケージ
mise run install
```

mise を使わない場合:

```bash
# Arduino ライブラリとボードコアのインストール
bash scripts/install_libs.sh

# フロントエンド npm 依存関係のインストール
cd frontend && npm install && cd ..
```

### 3. フロントエンドのビルド

Svelte ダッシュボードをコンパイルし、アセットをインライン化して `dashboard.h` を生成します。

```bash
mise run build-frontend
# または:
bash scripts/build_frontend.sh
```

### 4. ファームウェアのコンパイル

```bash
mise run build
# または:
bash scripts/build_fw.sh
```

### 5. XIAO ESP32-S3 への書き込み

XIAO ESP32-S3 を USB-C で接続してから実行します。

```bash
mise run flash
# または（シリアルポートを自動検出）:
bash scripts/flash.sh

# シリアルポートを明示的に指定する場合:
bash scripts/flash.sh /dev/tty.usbmodem1101   # macOS
bash scripts/flash.sh /dev/ttyACM0            # Linux
```

flash スクリプトは次の手順を実行します。
1. ファームウェアのコンパイル
2. コンパイル済みバイナリをフラッシュに書き込み
3. LittleFS ファイルシステムイメージ（ダッシュボードを含む）のアップロード

---

## 設定

### WiFi セットアップ（初回起動時）

初回起動時（または WiFi 認証情報が保存されていない場合）、デバイスは **WiFi アクセスポイント** を起動します。

| 設定項目 | 値 |
|----------|---|
| SSID | `twESP32GPS-Setup` |
| パスワード | *(なし — オープン AP)* |
| ポータル URL | `http://192.168.4.1` |
| ポータルタイムアウト | 3 分 |

**手順:**
1. スマートフォンまたはパソコンを `twESP32GPS-Setup` WiFi に接続する
2. キャプティブポータルが自動的に開く（開かない場合は `http://192.168.4.1` を手動で開く）
3. 接続する WiFi ネットワークを選択し、パスワードを入力する
4. デバイスが再起動し、指定した WiFi ネットワークに接続する

接続後、デバイスの IP アドレスがシリアルコンソールに表示されます。

```
[WiFi] Connected! IP: 192.168.1.xxx
[Ready] NTP: 192.168.1.xxx:123 | Web: http://192.168.1.xxx/
```

### ファームウェア定数（`config.h`）

コンパイル前に [`twESP32GPS/config.h`](twESP32GPS/config.h) を編集することで、以下の定数を変更できます。

| 定数 | デフォルト値 | 説明 |
|------|-------------|------|
| `GPS_BAUD` | `9600` | GPS モジュールのボーレート |
| `GPS_RX_PIN` | `44` | GPS TX → ESP RX の GPIO（D7） |
| `GPS_TX_PIN` | `43` | GPS RX → ESP TX の GPIO（D6） |
| `PPS_PIN` | `2` | 1PPS 信号の GPIO（D1） |
| `NTP_PORT` | `123` | NTP サーバーの UDP ポート |
| `HTTP_PORT` | `80` | HTTP Web サーバーのポート |
| `WIFI_AP_NAME` | `"twESP32GPS-Setup"` | WiFi セットアップ用アクセスポイントの SSID |
| `WIFI_AP_PASS` | `""` | AP パスワード（空文字 = オープン） |
| `MAX_SATELLITES` | `48` | 保存する衛星情報の最大数 |

### フロントエンド開発（ライブリロード）

実機の ESP32 に API をプロキシしてローカルでライブリロード開発する場合:

```bash
# frontend/vite.config.js のプロキシターゲットを実機の ESP32 の IP に変更してから実行
cd frontend
npm run dev
# → http://localhost:5173 で確認
```

---

## デバイスへのアクセス

デバイスがネットワークに接続されたら、シリアルコンソールに表示された IP アドレスを使ってアクセスします。

| サービス | URL / アドレス |
|----------|--------------|
| Web ダッシュボード | `http://<ESP32-IP>/` |
| GPS JSON API | `http://<ESP32-IP>/api/gps` |
| NTP サーバー | UDP `<ESP32-IP>:123` |

---

## API リファレンス

### `GET /api/gps`

現在の GPS 状態と衛星データを JSON で返します。

**レスポンス例:**

```json
{
  "hasFix": true,
  "time": "03:34:56",
  "date": "2025-07-19",
  "latitude": 35.681236,
  "longitude": 139.767125,
  "altitude": 40.5,
  "speedKmh": 0.2,
  "numSatellites": 8,
  "numSatsInView": 14,
  "stratum": 1,
  "uptime": 3600,
  "ntpClients": 42,
  "satellites": [
    { "prn": 1,  "system": "GPS",     "elevation": 45, "azimuth": 120, "snr": 38 },
    { "prn": 65, "system": "GLONASS", "elevation": 32, "azimuth": 210, "snr": 30 },
    { "prn": 11, "system": "Galileo", "elevation": 20, "azimuth": 310, "snr": 25 }
  ]
}
```

**フィールド説明:**

| フィールド | 型 | 説明 |
|------------|-----|------|
| `hasFix` | bool | GPS が有効な測位を取得できている場合 `true` |
| `time` | string | UTC 時刻（HH:MM:SS） |
| `date` | string | UTC 日付（YYYY-MM-DD） |
| `latitude` | float | 緯度（10 進数表記） |
| `longitude` | float | 経度（10 進数表記） |
| `altitude` | float | 高度（メートル） |
| `speedKmh` | float | 対地速度（km/h） |
| `numSatellites` | int | 測位に使用している衛星数（GGA から取得） |
| `numSatsInView` | int | 視野内の衛星総数（GSV から取得） |
| `stratum` | int | NTP ストラタム: `1` = GPS 測位あり、`16` = 測位なし |
| `uptime` | int | デバイスの稼働時間（秒） |
| `ntpClients` | int | 起動後に処理した NTP リクエストの総数 |
| `satellites[]` | array | 衛星ごとの詳細情報（以下参照） |
| `satellites[].prn` | int | 衛星 PRN 番号 |
| `satellites[].system` | string | 測位システム: GPS、GLONASS、Galileo、BeiDou、QZSS、SBAS |
| `satellites[].elevation` | int | 仰角（度、0〜90） |
| `satellites[].azimuth` | int | 方位角（度、0〜359） |
| `satellites[].snr` | int | 信号対雑音比（dBHz、0 = 追跡していない） |

---

## テスト方法

### シリアルモニター

XIAO ESP32-S3 を USB で接続し、**115200 baud** でシリアルモニターを開くと、起動メッセージと GPS 解析の出力を確認できます。

### NTP サーバーのテスト

```bash
# NTP 問い合わせでオフセットとストラタムを確認（macOS/Linux）
ntpdate -q <ESP32-IP>

# 詳細な NTP パケット検査
ntpq -p <ESP32-IP>

# sntp を使用する場合
sntp -d <ESP32-IP>
```

**GPS 測位取得後の期待される出力:**

```
server <ESP32-IP>, stratum 1, offset 0.000xxx, delay 0.00xxx
```

**GPS 測位前（衛星を待っている状態）の期待される出力:**

```
server <ESP32-IP>, stratum 16, ...
```

> **注意**: 電源投入後、GPS モジュールが測位を取得するまでに 30 秒〜数分かかる場合があります（TTFF: Time-To-First-Fix）。アルマナック/エフェメリスデータのキャッシュ状況や空の見晴らしによって変わります。測位が取得されるまで、NTP ストラタムは `16` になります。

### Web ダッシュボードのテスト

1. ブラウザで `http://<ESP32-IP>/` を開く
2. ダッシュボードに以下が表示されることを確認する:
   - GPS 測位状態、UTC 時刻・日付、座標、高度、速度
   - NTP ストラタムとクライアントリクエスト数
   - 衛星の位置と信号強度を示すスカイプロット
3. データは数秒ごとに自動更新されます

### JSON API のテスト

```bash
curl http://<ESP32-IP>/api/gps | python3 -m json.tool
```

---

## トラブルシューティング

| 症状 | 考えられる原因 | 解決策 |
|------|--------------|--------|
| 初回起動時に WiFi AP が現れない | ファームウェアが正しく書き込まれていない | `flash.sh` を再実行し、シリアル出力を確認する |
| GPS が測位できない | 空の見晴らしが悪い・屋内 | 空がよく見える屋外の場所に移動する |
| NTP ストラタムが 16 のまま | GPS 測位未取得 | GPS が測位を取得するまで待つ（TTFF） |
| ダッシュボードが表示されない | LittleFS が書き込まれていない・ビルド失敗 | `build_frontend.sh` と `flash.sh` の両方が正常に完了しているか確認する |
| シリアルポートが見つからない | USB ドライバが未インストール | 使用 OS 用の CH340 または CP210x USB シリアルドライバをインストールする |
| WiFi セットアップポータルがタイムアウトする | 3 分のタイムアウトが経過した | デバイスの電源を入れ直してから再試行する |

---

## 依存ライブラリ・ツール

| 種別 | ライブラリ/ツール | バージョン | ライセンス |
|------|-----------------|-----------|-----------|
| Arduino | [TinyGPS++](https://github.com/mikalhart/TinyGPSPlus) | 最新版 | LGPL-2.1 |
| Arduino | [ArduinoJson](https://arduinojson.org/) | v7.x | MIT |
| Arduino | [WiFiManager](https://github.com/tzapu/WiFiManager) | 最新版 | MIT |
| ESP32 コア | [esp32:esp32](https://github.com/espressif/arduino-esp32) | ≥ 3.x | Apache-2.0 |
| フロントエンド | [Svelte](https://svelte.dev/) | v5.x | MIT |
| フロントエンド | [Vite](https://vitejs.dev/) | v6.x | MIT |
| ビルドツール | [arduino-cli](https://arduino.github.io/arduino-cli/) | 最新版 | Apache-2.0 |
| 開発環境 | [mise](https://mise.jdx.dev/) | 最新版 | MIT |

---

## ライセンス

このプロジェクトは **Apache License 2.0** の下でライセンスされています。  
全文は [LICENSE](LICENSE) ファイルをご参照ください。

```
Copyright 2025 twsnmp contributors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
```

---

## クレジット・関連プロジェクト

- **[twgps](../twgps)** — Go 言語ベースの先行プロジェクト。UI/UX デザインと NTP ロジックは twgps から着想を得ています
- NTP 実装は [RFC 4330](https://datatracker.ietf.org/doc/html/rfc4330) / [RFC 5905](https://datatracker.ietf.org/doc/html/rfc5905) に準拠
- GPS 解析には [TinyGPS++](https://github.com/mikalhart/TinyGPSPlus) を使用し、マルチ測位対応のカスタム NMEA GSV パーサーを追加実装

---

## コントリビューション

バグ報告、機能要望、プルリクエストを歓迎します。  
プロジェクトリポジトリで Issue または Pull Request を作成してください。
