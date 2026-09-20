#!/bin/bash
# Renders every gauge page on the Mac using the real ui.cpp and LVGL, so the
# layout can be checked without flashing. Writes docs/preview.png.
#
# Needs the LVGL copy PlatformIO downloads: run `pio run` once first.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
LVGL="$ROOT/.pio/libdeps/outback_gauge/lvgl"
OUT="$ROOT/.pio/preview"
INC="-I$HERE/shim -I$ROOT/include -I$ROOT/src -I$LVGL -I$LVGL/src -DLV_CONF_INCLUDE_SIMPLE"

[ -d "$LVGL" ] || { echo "LVGL not found; run 'pio run -e outback_gauge' first"; exit 1; }
mkdir -p "$OUT/obj"

# LVGL objects are cached; only ui.cpp and the harness rebuild each time.
for f in $(find "$LVGL/src" -name '*.c'); do
  o="$OUT/obj/$(echo "${f#$LVGL/}" | tr '/' '_').o"
  [ -f "$o" ] || clang -c -O1 -w $INC "$f" -o "$o"
done
clang++ -std=c++17 -O1 $INC -c "$ROOT/src/ui.cpp" -o "$OUT/ui.o"
clang++ -std=c++17 -O1 $INC -c "$ROOT/src/settings.cpp" -o "$OUT/settings.o"
clang++ -std=c++17 -O1 $INC -c "$HERE/harness.cpp" -o "$OUT/harness.o"
clang++ "$OUT"/obj/*.o "$OUT/ui.o" "$OUT/settings.o" "$OUT/harness.o" -o "$OUT/render"

(cd "$OUT" && ./render)
sips -s format png "$OUT/gauges.ppm" --out "$ROOT/docs/preview.png" >/dev/null
echo "wrote docs/preview.png"
