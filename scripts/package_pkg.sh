#!/usr/bin/env bash
# ELF -> eboot.bin -> GP4 -> PKG using the OpenOrbis tools.
#
#   OO_PS4_TOOLCHAIN=/path/to/toolchain scripts/package_pkg.sh <app.elf> <out-dir>
#
# Environment:
#   OO_PS4_TOOLCHAIN   toolchain root (required)
#   PS5UI_CONTENT_ID   36-char content id, must match param.sfo
#                      (default IV0000-PSUI00001_00-0000000000000000)
#
# The tool command lines below follow the official OpenOrbis samples. If your toolchain
# version differs, the script prints the tool's own --help text on failure.
set -euo pipefail

ELF="${1:?usage: package_pkg.sh <app.elf> <out-dir>}"
OUT="${2:?usage: package_pkg.sh <app.elf> <out-dir>}"
TC="${OO_PS4_TOOLCHAIN:?OO_PS4_TOOLCHAIN is not set}"
CONTENT_ID="${PS5UI_CONTENT_ID:-IV0000-PSUI00001_00-0000000000000000}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

case "$(uname -s)" in
  Linux)  BIN="$TC/bin/linux" ;;
  Darwin) BIN="$TC/bin/macos" ;;
  *)      BIN="$TC/bin/windows" ;;
esac
[[ -d "$BIN" ]] || { echo "error: toolchain tool folder not found: $BIN" >&2; exit 1; }
chmod +x "$BIN"/* 2>/dev/null || true

# Self-contained .NET tool: do not require ICU on the build machine.
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1

FSELF="$BIN/create-fself"
GP4TOOL="$BIN/create-gp4"
PKGTOOL="$BIN/PkgTool.Core"
for t in "$FSELF" "$PKGTOOL"; do
  [[ -x "$t" ]] || { echo "error: missing tool $t" >&2; ls -la "$BIN" >&2; exit 1; }
done

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
ELF="$(cd "$(dirname "$ELF")" && pwd)/$(basename "$ELF")"
STAGE="$OUT/pkg_stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/sce_sys"

cp "$ROOT/pkg/sce_sys/param.sfo" "$ROOT/pkg/sce_sys/icon0.png" "$STAGE/sce_sys/"
FILES=("eboot.bin" "sce_sys/param.sfo" "sce_sys/icon0.png")

# Some toolchain releases ship the runtime modules next to their samples; copy them if present.
for cand in "$TC/samples/hello_world/sce_module" "$TC/sce_module"; do
  if [[ -d "$cand" ]] && compgen -G "$cand/*" >/dev/null; then
    mkdir -p "$STAGE/sce_module"
    cp "$cand"/* "$STAGE/sce_module/"
    for f in "$STAGE"/sce_module/*; do FILES+=("sce_module/$(basename "$f")"); done
    echo "package: included sce_module/ from $cand"
    break
  fi
done

echo "package: creating eboot.bin"
OELF="$OUT/app.oelf"
rm -f "$STAGE/eboot.bin" "$OELF"
make_eboot() {
  (cd "$STAGE" && "$FSELF" "$@")
}
ok=0
for variant in 1 2; do
  rm -f "$STAGE/eboot.bin"
      make_eboot -in="$ELF" -out="$OELF" --eboot "eboot.bin" && [[ -s "$STAGE/eboot.bin" ]] && ok=1 && break
    
  
    
  
  echo "package: create-fself variant $variant did not produce eboot.bin, trying next" >&2
done
if [[ $ok != 1 ]]; then
  echo "error: create-fself failed. Tool help follows:" >&2
  "$FSELF" --help >&2 || true
  exit 1
fi

echo "package: creating pkg.gp4"
if [[ -x "$GP4TOOL" ]]; then
  (cd "$STAGE" && "$GP4TOOL" -out pkg.gp4 --content-id="$CONTENT_ID" --files "${FILES[*]}") || {
    echo "package: create-gp4 failed, using fallback tools/make_gp4.py" >&2
    python3 "$ROOT/tools/make_gp4.py" "$STAGE/pkg.gp4" "$CONTENT_ID" "${FILES[@]}"
  }
else
  python3 "$ROOT/tools/make_gp4.py" "$STAGE/pkg.gp4" "$CONTENT_ID" "${FILES[@]}"
fi

echo "package: building PKG"
(cd "$STAGE" && "$PKGTOOL" pkg_build pkg.gp4 .) || {
  echo "error: PkgTool.Core failed. Tool help follows:" >&2
  "$PKGTOOL" --help >&2 || true
  exit 1
}

PKG="$(find "$STAGE" -maxdepth 1 -name '*.pkg' -print -quit)"
[[ -n "$PKG" ]] || { echo "error: no .pkg produced" >&2; ls -la "$STAGE" >&2; exit 1; }
mv "$PKG" "$OUT/"
cp "$STAGE/eboot.bin" "$OUT/eboot.bin"
echo "package: done -> $OUT/$(basename "$PKG")"
