#!/usr/bin/env bash
# Host tests for the fill tool port (native/blender_gp/project_grease_blender_fill.c, Blender 3.6.23
# gpencil_fill.c) against the real pinned Blender closure, plain and under ASan/UBSan.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "fill test"
REST=()
for o in "${OBJS[@]}"; do [[ "$o" == */project_grease_blender_fill.c.o ]] || REST+=("$o"); done
build() { # $1 = suffix, rest = extra flags for the fill / test objects
  local sfx="$1"; shift
  gcc "${CF[@]}" "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_blender_fill.c" -o "$OUT/fill$sfx.o"
  gcc "${CF[@]}" "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/tests/test_fill.c" -o "$OUT/test_fill$sfx.o"
  g++ "$@" "$OUT/test_fill$sfx.o" "$OUT/fill$sfx.o" "${REST[@]}" -Wl,--gc-sections -lpthread -lm -o "$OUT/test_fill$sfx"
}
build ""
"$OUT/test_fill"
build "_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 "$OUT/test_fill_asan"
