#!/usr/bin/env bash
# Downloads the OpenOrbis PS4 Toolchain and prints its root directory (the folder holding link.x).
#
#   OO_TOOLCHAIN_URL=<direct zip/tar.gz url>   optional; otherwise the latest GitHub release is used
#   GH_TOKEN                                   needed for `gh release download` in CI
#   scripts/get_toolchain.sh <dest-dir>
set -euo pipefail
DEST="${1:?usage: get_toolchain.sh <dest-dir>}"
REPO="OpenOrbis/OpenOrbis-PS4-Toolchain"
mkdir -p "$DEST"
DEST="$(cd "$DEST" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ -n "${OO_TOOLCHAIN_URL:-}" ]]; then
  echo "get_toolchain: downloading $OO_TOOLCHAIN_URL" >&2
  curl -fL --retry 3 -o "$TMP/toolchain.bin" "$OO_TOOLCHAIN_URL"
else
  echo "get_toolchain: downloading latest release of $REPO" >&2
  (cd "$TMP" && gh release download --repo "$REPO" --pattern '*.zip' --pattern '*.tar.gz' --pattern '*.tgz') >&2 || true
  mapfile -t assets < <(find "$TMP" -maxdepth 1 -type f \( -name '*.zip' -o -name '*.tar.gz' -o -name '*.tgz' \) | sort)
  if [[ ${#assets[@]} -eq 0 ]]; then
    echo "get_toolchain: no release asset found, falling back to a git clone" >&2
    git clone --depth 1 "https://github.com/$REPO" "$TMP/src" >&2
  else
    pick="${assets[0]}"
    for a in "${assets[@]}"; do
      case "$(basename "$a" | tr '[:upper:]' '[:lower:]')" in *linux*) pick="$a"; break;; esac
    done
    echo "get_toolchain: using asset $(basename "$pick")" >&2
    cp "$pick" "$TMP/toolchain.bin"
    case "$pick" in *.zip) mv "$TMP/toolchain.bin" "$TMP/toolchain.zip" ;; esac
  fi
fi

mkdir -p "$TMP/x"
if [[ -f "$TMP/toolchain.zip" ]]; then
  unzip -q "$TMP/toolchain.zip" -d "$TMP/x"
elif [[ -f "$TMP/toolchain.bin" ]]; then
  if unzip -tq "$TMP/toolchain.bin" >/dev/null 2>&1; then unzip -q "$TMP/toolchain.bin" -d "$TMP/x"
  else tar -xf "$TMP/toolchain.bin" -C "$TMP/x"; fi
elif [[ -d "$TMP/src" ]]; then
  cp -a "$TMP/src/." "$TMP/x/"
fi

LINKX="$(find "$TMP/x" -name link.x -print -quit)"
[[ -n "$LINKX" ]] || { echo "get_toolchain: link.x not found in the downloaded toolchain" >&2; exit 1; }
SRC_ROOT="$(dirname "$LINKX")"
rm -rf "$DEST/toolchain"
mv "$SRC_ROOT" "$DEST/toolchain"
chmod -R u+rwX "$DEST/toolchain"
find "$DEST/toolchain/bin" -type f -exec chmod +x {} + 2>/dev/null || true
echo "$DEST/toolchain"
