#!/usr/bin/env bash
# =============================================================================
#  install_libs.sh — Install Arduino CLI board core and libraries
# =============================================================================
set -euo pipefail

echo "=== twESP32GPS: Installing Arduino dependencies ==="

# --- Arduino CLI check ---
if ! command -v arduino-cli &> /dev/null; then
  echo "[ERROR] arduino-cli not found."
  echo "  Install via: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
  echo "  Or: brew install arduino-cli  (macOS with Homebrew)"
  exit 1
fi

echo "[1/4] Updating board index..."
arduino-cli core update-index --additional-urls \
  "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

echo "[2/4] Installing ESP32 board core..."
arduino-cli core install esp32:esp32 --additional-urls \
  "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

echo "[3/4] Installing Arduino libraries..."
arduino-cli lib install "TinyGPS++"
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "WiFiManager"

echo "[4/4] Verifying installed libraries..."
arduino-cli lib list | grep -E "TinyGPSPlus|ArduinoJson|WiFiManager"

echo ""
echo "=== Done! All dependencies installed. ==="
echo ""
echo "Next steps:"
echo "  1. Run: bash scripts/build_frontend.sh   (build the Svelte dashboard)"
echo "  2. Run: bash scripts/build_fw.sh          (compile firmware)"
echo "  3. Run: bash scripts/flash.sh             (upload to ESP32)"
