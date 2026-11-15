#!/bin/bash
# Copies the built firmware image to dist/ under a dated name, ready to AirDrop
# or put in iCloud Drive and upload from a phone on the gauge's Wi-Fi page
# ("Update firmware"). Build first: pio run -e outback_gauge
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ENV="${1:-outback_gauge}"
SRC="$ROOT/.pio/build/$ENV/firmware.bin"
[ -f "$SRC" ] || { echo "no $SRC; run: pio run -e $ENV"; exit 1; }
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/outback-gauge-$(date +%Y%m%d-%H%M).bin"
cp "$SRC" "$OUT"
echo "$OUT"
