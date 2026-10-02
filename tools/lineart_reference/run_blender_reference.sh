#!/usr/bin/env bash
# Generates Blender 3.6.23's own Line Art output for scenes.txt into $1 (default build/lineart-reference).
# Downloads the official Linux build into build/blender-3.6.23 when it is not there (CI caches it).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${1:-$ROOT/build/lineart-reference}"
VERSION=3.6.23
DIR="$ROOT/build/blender-$VERSION"
if [[ ! -x "$DIR/blender" ]]; then
  mkdir -p "$ROOT/build"
  URL="https://download.blender.org/release/Blender3.6/blender-$VERSION-linux-x64.tar.xz"
  for attempt in 1 2 3; do
    if curl -fsSL --retry 3 "$URL" -o "$ROOT/build/blender.tar.xz"; then break; fi
    sleep $((attempt * 5))
  done
  mkdir -p "$DIR"
  tar -xJf "$ROOT/build/blender.tar.xz" -C "$DIR" --strip-components=1
  rm -f "$ROOT/build/blender.tar.xz"
fi
"$DIR/blender" --version | head -1
"$DIR/blender" --background --factory-startup --python "$HERE/blender_reference.py" -- "$HERE/scenes.txt" "$OUT"
ls "$OUT"
