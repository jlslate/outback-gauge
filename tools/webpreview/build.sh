#!/bin/bash
# Renders the Wi-Fi settings form with the real src/webpage.cpp and writes
# docs/settings-page.html, so the markup can be checked in a browser without
# flashing anything.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$ROOT/.pio/webpreview"
INC="-I$ROOT/tools/preview/shim -I$ROOT/src"

mkdir -p "$OUT"
clang++ -std=c++17 -O1 $INC -c "$ROOT/src/webpage.cpp" -o "$OUT/webpage.o"
clang++ -std=c++17 -O1 $INC -c "$ROOT/src/settings.cpp" -o "$OUT/settings.o"
clang++ -std=c++17 -O1 $INC -c "$HERE/render.cpp" -o "$OUT/render.o"
clang++ "$OUT"/webpage.o "$OUT/settings.o" "$OUT/render.o" -o "$OUT/render"

(cd "$OUT" && ./render)
cp "$OUT/settings-page.html" "$ROOT/docs/settings-page.html"
echo "wrote docs/settings-page.html"
