#!/usr/bin/env bash
# =============================================================================
#  build_fw.sh — Compile Arduino firmware with arduino-cli
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
SKETCH_DIR="$ROOT_DIR/twESP32GPS"

# XIAO ESP32-S3 Fully Qualified Board Name
FQBN="esp32:esp32:XIAO_ESP32S3"

# Additional board index URL
BOARD_URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

echo "=== twESP32GPS: Compiling firmware ==="
echo "  Board: $FQBN"
echo "  Sketch: $SKETCH_DIR"
echo ""

arduino-cli compile \
  --fqbn "$FQBN" \
  --additional-urls "$BOARD_URL" \
  --build-property "build.partitions=min_spiffs" \
  --warnings all \
  "$SKETCH_DIR"

echo ""
echo "=== Firmware compilation successful! ==="
echo "Run: bash scripts/flash.sh [PORT] to upload"
