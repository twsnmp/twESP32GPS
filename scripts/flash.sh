#!/usr/bin/env bash
# =============================================================================
#  flash.sh — Flash firmware + upload LittleFS to XIAO ESP32-S3
#
#  Usage:
#    bash scripts/flash.sh             (auto-detect port)
#    bash scripts/flash.sh /dev/ttyACM0  (explicit port)
#
#  LittleFS upload uses arduino-littlefs-upload plugin.
#  Install it with:
#    pip install arduino-littlefs-upload
#  Or download from:
#    https://github.com/earlephilhower/arduino-littlefs-upload/releases
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
SKETCH_DIR="$ROOT_DIR/twESP32GPS"
DATA_DIR="$SKETCH_DIR/data"

FQBN="esp32:esp32:XIAO_ESP32S3"
BOARD_URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

# --- Port detection ---
if [ -n "${1:-}" ]; then
  PORT="$1"
else
  # Auto-detect common ESP32 serial ports
  PORT=$(ls /dev/tty.usbmodem* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1 || true)
  if [ -z "$PORT" ]; then
    echo "[ERROR] No serial port found. Plug in your XIAO ESP32-S3 and try again."
    echo "  Or specify manually: bash scripts/flash.sh /dev/ttyACM0"
    exit 1
  fi
fi

echo "=== twESP32GPS: Flashing to $PORT ==="

# --- Step 1: Compile (to ensure binary is up to date) ---
echo "[1/3] Compiling firmware..."
arduino-cli compile \
  --fqbn "$FQBN" \
  --additional-urls "$BOARD_URL" \
  --build-property "build.partitions=min_spiffs" \
  "$SKETCH_DIR"

# --- Step 2: Upload firmware ---
echo "[2/2] Uploading firmware to $PORT..."
arduino-cli upload \
  -p "$PORT" \
  --fqbn "$FQBN" \
  --additional-urls "$BOARD_URL" \
  "$SKETCH_DIR"

echo ""
echo "=== Flash complete! ==="
echo ""
echo "  1. On first boot, connect to WiFi AP: 'twESP32GPS-Setup'"
echo "  2. Open browser → http://192.168.4.1 to configure WiFi"
echo "  3. After WiFi setup, access the dashboard at http://<ESP32-IP>/"
echo "  4. NTP server is available at <ESP32-IP>:123 (UDP)"
