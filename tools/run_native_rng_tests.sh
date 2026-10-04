#!/usr/bin/env bash
# Host test for the Blender RNG/hash closure (rand.cc, noise.c, BLI_hash.h) used by the Offset and
# Noise modifiers. Compiles the real pinned sources, so tools/import_blender_gp.sh must have run;
# set PG_REQUIRE_BLENDER_SOURCE=1 (CI) to fail instead of skip when it has not.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$ROOT/third_party/blender"
BL="$B/source/blender"
if [[ ! -f "$BL/blenlib/intern/rand.cc" ]]; then
  if [[ "${PG_ALLOW_SKIP:-0}" == "1" && "${PG_REQUIRE_BLENDER_SOURCE:-0}" != "1" ]]; then
    echo "SKIP RNG test (run tools/import_blender_gp.sh first)"
    exit 0
  fi
  echo "ERROR: Pinned Blender source missing: $BL/blenlib/intern/rand.cc (set PG_ALLOW_SKIP=1 to skip locally)" >&2
  exit 1
fi
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
INC=(-I"$ROOT/native/blender_gp/android_compat" -I"$ROOT/native/blender_gp" -I"$BL" -I"$BL/blenlib"
     -I"$BL/makesdna" -I"$B/intern/guardedalloc" -I"$B/intern/atomic")
gcc -std=gnu11 -c -DNDEBUG "${INC[@]}" "$BL/blenlib/intern/noise.c" -o "$OUT/noise.o"
g++ -std=gnu++17 -c -DNDEBUG "${INC[@]}" "$BL/blenlib/intern/rand.cc" -o "$OUT/rand.o"
g++ -std=gnu++17 -Wall -DNDEBUG "${INC[@]}" "$ROOT/native/blender_gp/tests/test_blender_rng.cc" \
  "$OUT/rand.o" "$OUT/noise.o" -o "$OUT/test_rng"
"$OUT/test_rng"
