#!/usr/bin/env bash
# Host tests for selection-aware editing (native/blender_gp/project_grease_blender_edit*.c),
# plain and under ASan/UBSan. Select random links the real pinned rand.cc, so
# tools/import_blender_gp.sh must have run; set PG_REQUIRE_BLENDER_SOURCE=1 (CI) to fail instead of
# skip when it has not.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$ROOT/third_party/blender"
BL="$B/source/blender"
if [[ ! -f "$BL/blenlib/intern/rand.cc" ]]; then
  if [[ "${PG_REQUIRE_BLENDER_SOURCE:-0}" == "1" ]]; then
    echo "Pinned Blender source missing: $BL/blenlib/intern/rand.cc" >&2
    exit 1
  fi
  echo "SKIP edit test (run tools/import_blender_gp.sh first)"
  exit 0
fi
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=("$ROOT/native/blender_gp/tests/test_blender_edit.c" "$ROOT/native/blender_gp/project_grease_blender_edit.c" "$ROOT/native/blender_gp/project_grease_blender_edit2.c" "$ROOT/native/blender_gp/project_grease_blender_edit3.c" "$ROOT/native/blender_gp/project_grease_blender_edit4.c" "$ROOT/native/blender_gp/project_grease_blender_edit6.c" "$ROOT/native/blender_gp/project_grease_blender_edit7.c" "$ROOT/native/blender_gp/project_grease_curvemap.c")
# Shims first; BLI_rand.h comes from the pinned tree.
INC=(-I"$ROOT/native/blender_gp/tests/select_shim" -I"$ROOT/native/blender_gp" -I"$BL/blenlib")
BINC=(-I"$ROOT/native/blender_gp/android_compat" -I"$ROOT/native/blender_gp" -I"$BL" -I"$BL/blenlib"
      -I"$BL/makesdna" -I"$B/intern/guardedalloc" -I"$B/intern/atomic")
WARN=(-Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter)
build() { # $1 = output, rest = extra flags
  local out="$1"; shift
  gcc -std=gnu11 -c -DNDEBUG "$@" "${BINC[@]}" "$BL/blenlib/intern/noise.c" -o "$out.noise.o"
  g++ -std=gnu++17 -c -DNDEBUG "$@" "${BINC[@]}" "$BL/blenlib/intern/rand.cc" -o "$out.rand.o"
  g++ -std=gnu++17 -c "$@" "$ROOT/native/blender_gp/tests/blender_mem_stub.cc" -o "$out.mem.o"
  local objs=()
  for s in "${SRC[@]}"; do
    gcc -std=gnu11 -c "$@" "${WARN[@]}" "${INC[@]}" "$s" -o "$out.$(basename "$s").o"
    objs+=("$out.$(basename "$s").o")
  done
  g++ "$@" "${objs[@]}" "$out.rand.o" "$out.noise.o" "$out.mem.o" -lm -o "$out"
}
build "$OUT/test_edit"
"$OUT/test_edit"
build "$OUT/test_edit_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 "$OUT/test_edit_asan"
