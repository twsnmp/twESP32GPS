# twESP32GPS

**Stratum 1 NTP Server & Satellite Dashboard**
for Seeed Studio XIAO ESP32-S3 + VK2828U7G5 GPS Module

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

---

## Features

- 🛰️ **Stratum 1 NTP Server** — GPS-disciplined, 1PPS hardware interrupt for sub-millisecond accuracy
- 🌐 **Web Dashboard** — Svelte 5 single-page app with real-time satellite Sky Plot visualization
- 📡 **Multi-constellation** — GPS, GLONASS, Galileo, BeiDou, QZSS, SBAS satellite tracking
- 🔧 **WiFiManager** — Captive Portal for easy WiFi configuration on first boot
- 🌍 **EN/JA Bilingual** — English and Japanese dashboard UI

---

## Hardware

### Wiring (VK2828U7G5 → XIAO ESP32-S3)

| GPS Pin | Label | ESP32-S3 Pin |
|---------|-------|--------------|
| G       | GND   | GND          |
| V       | VCC   | 3V3          |
| T       | TX    | D7 (GPIO 44) |
| R       | RX    | D6 (GPIO 43) |
| B       | PPS   | D1 (GPIO  2) |
| E       | EN    | 3V3          |

---

## Project Structure

```
twESP32GPS/
├── mise.toml                    # Development environment (arduino-cli + node)
├── twESP32GPS/                  # Arduino sketch
│   ├── twESP32GPS.ino           # Main entry point (setup/loop)
│   ├── config.h                 # Pin definitions & constants
│   ├── gps.h / gps.cpp          # GPS parsing (TinyGPS++ + custom GSV)
│   ├── ntp.h / ntp.cpp          # NTP UDP server (RFC 5905, Stratum 1)
│   ├── web.h / web.cpp          # HTTP server + /api/gps JSON endpoint
│   └── data/                    # LittleFS filesystem root
│       └── index.html.gz        # Svelte dashboard (built by scripts/)
├── frontend/                    # Svelte 5 + Vite frontend source
│   ├── src/
│   │   ├── App.svelte           # Main UI (3-pane layout)
│   │   ├── components/
│   │   │   └── SkyPlot.svelte   # Canvas 2D satellite sky plot
│   │   ├── i18n.svelte.js       # EN/JA translations
│   │   └── style.css            # Glassmorphic dark/light theme
│   └── vite.config.js           # Vite + gzip compression config
└── scripts/
    ├── install_libs.sh          # Install Arduino libs & board core
    ├── build_frontend.sh        # Build Svelte → gzip → data/
    ├── build_fw.sh              # Compile Arduino firmware
    └── flash.sh                 # Flash firmware + upload LittleFS
```

---

## Quick Start

### 1. Install Dependencies

```bash
# Install mise (if needed)
curl https://mise.run | sh

# Install arduino-cli (macOS)
brew install arduino-cli

# Install all dependencies (board core + Arduino libs + npm)
mise run install
```

### 2. Build Frontend

```bash
mise run build-frontend
```

### 3. Compile Firmware

```bash
mise run build
```

### 4. Flash to ESP32

```bash
# Plug in XIAO ESP32-S3 via USB-C, then:
mise run flash

# Or specify port explicitly:
bash scripts/flash.sh /dev/ttyACM0
```

### 5. First Boot — WiFi Setup

1. Connect your phone/laptop to the WiFi AP: **`twESP32GPS-Setup`**
2. Open browser → navigate to **http://192.168.4.1**
3. Select your WiFi network and enter password
4. ESP32 restarts and connects to your network

### 6. Access Dashboard

After connecting, find the IP from your router or serial monitor:

```
[WiFi] Connected! IP: 192.168.1.xxx
[Ready] NTP: 192.168.1.xxx:123 | Web: http://192.168.1.xxx/
```

- **Dashboard**: `http://<ESP32-IP>/`
- **JSON API**: `http://<ESP32-IP>/api/gps`
- **NTP Server**: UDP `<ESP32-IP>:123`

---

## API Reference

### `GET /api/gps`

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
    { "prn": 1, "system": "GPS", "elevation": 45, "azimuth": 120, "snr": 38 },
    { "prn": 65, "system": "GLONASS", "elevation": 32, "azimuth": 210, "snr": 30 }
  ]
}
```

---

## Testing NTP

```bash
# Check NTP response (macOS/Linux)
ntpdate -q <ESP32-IP>

# Detailed NTP packet inspection
ntpq -p <ESP32-IP>

# Or using sntp
sntp -d <ESP32-IP>
```

Expected result when GPS is locked:
```
stratum 1, ref GPS
```

---

## Dependencies

| Type | Library | Version |
|------|---------|---------|
| Arduino | TinyGPS++ | latest |
| Arduino | ArduinoJson | v7.x |
| Arduino | WiFiManager | latest |
| ESP32 Core | esp32:esp32 | ≥3.x |
| Frontend | Svelte | v5.x |
| Frontend | Vite | v6.x |

---

## Frontend Development

To develop the UI with live reload (proxy to a real ESP32):

```bash
# Edit frontend/vite.config.js → update proxy target to your ESP32 IP
cd frontend
npm run dev
# → http://localhost:5173
```

---

## Credits

- UI/UX design inspired by [twgps](../twgps) by the same author
- NTP implementation based on RFC 4330 / RFC 5905
