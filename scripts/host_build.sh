#!/usr/bin/env bash
# Builds the portable core + UI for the desktop, runs the automated tests and
# (optionally) the screenshot harness. This does NOT need the OpenOrbis toolchain.
#
#   scripts/host_build.sh            # build + run tests (with ASan/UBSan)
#   scripts/host_build.sh --shots    # additionally render screenshots to build/host/shots
set -euo pipefail
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
OUT=build/host
mkdir -p "$OUT"
FLAGS=(-std=c++17 -O1 -g -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Isrc -I. -fsanitize=address,undefined -fno-omit-frame-pointer)

CORE=(src/core/*.cpp src/gfx/*.cpp src/ui/*.cpp src/platform/host/HostPlatform.cpp)

echo "== syntax check of the PS4 back-end (self-contained SDK declarations)"
"$CXX" -std=c++17 -fsyntax-only -Wall -Wextra -Isrc src/platform/ps4/Ps4Platform.cpp
"$CXX" -std=c++17 -fsyntax-only -Wall -Wextra -Isrc -DPS5UI_ENABLE_LAUNCH src/platform/ps4/Ps4Platform.cpp
"$CXX" -std=c++17 -fsyntax-only -Wall -Wextra -Isrc src/main.cpp

echo "== compile check with the PS4 flavour of flags (-fno-exceptions -fno-rtti, C++17 and C++14)"
for std in c++17 c++14; do
  for f in src/core/*.cpp src/gfx/Png.cpp src/gfx/Renderer.cpp src/gfx/Background.cpp src/ui/*.cpp src/main.cpp src/platform/ps4/Ps4Platform.cpp; do
    "$CXX" -std=$std -fno-exceptions -fno-rtti -fsyntax-only -Wall -Wextra -Isrc "$f"
  done
done

echo "== PNG fixtures"
python3 tests/make_png_fixtures.py "$OUT/png"

echo "== building tests"
"$CXX" "${FLAGS[@]}" "${CORE[@]}" tests/test_main.cpp -o "$OUT/tests"
echo "== running tests"
"$OUT/tests" "$OUT/png" pkg/sce_sys

if [[ "${1:-}" == "--shots" ]]; then
  "$CXX" "${FLAGS[@]}" "${CORE[@]}" tools/host/host_main.cpp -o "$OUT/host_ui"
  rm -rf "$OUT/data" "$OUT/shots"
  "$OUT/host_ui" --data "$OUT/data" --out "$OUT/shots" --demo --icon pkg/sce_sys/icon0.png
fi
echo "OK"
