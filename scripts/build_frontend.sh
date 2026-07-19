#!/usr/bin/env bash
# =============================================================================
#  build_frontend.sh — Build Svelte dashboard and generate dashboard.h
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
FRONTEND_DIR="$ROOT_DIR/frontend"
SKETCH_DIR="$ROOT_DIR/twESP32GPS"
DATA_DIR="$SKETCH_DIR/data"

echo "=== twESP32GPS: Building Svelte frontend ==="

# --- Node.js check ---
if ! command -v node &> /dev/null; then
  echo "[ERROR] Node.js not found. Install via: mise install node"
  exit 1
fi

echo "[1/3] Installing npm dependencies..."
cd "$FRONTEND_DIR"
npm install

echo "[2/3] Building with Vite..."
npm run build

echo "[3/3] Inlining assets and generating dashboard.h..."
node "$SCRIPT_DIR/inline_and_convert.js"

echo ""
echo "=== Frontend build and C++ conversion complete! ==="
if [ -f "$SKETCH_DIR/dashboard.h" ]; then
  HEADER_SIZE=$(du -h "$SKETCH_DIR/dashboard.h" | awk '{print $1}')
  echo "  dashboard.h generated successfully (size: $HEADER_SIZE)"
else
  echo "[ERROR] dashboard.h was not generated."
  exit 1
fi

