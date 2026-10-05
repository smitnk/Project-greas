#!/usr/bin/env bash
# Host tests for the self-intersection trim (native/blender_gp/project_grease_stroke_trim.c)
# against the real pinned Blender closure, plain and with the trim / test objects under ASan/UBSan.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "stroke trim test"
REST=()
for o in "${OBJS[@]}"; do [[ "$o" == */project_grease_stroke_trim.c.o ]] || REST+=("$o"); done
build() { # $1 = suffix, rest = extra flags for the trim / test objects
  local sfx="$1"; shift
  gcc "${CF[@]}" "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_stroke_trim.c" -o "$OUT/trim$sfx.o"
  gcc "${CF[@]}" "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/tests/test_stroke_trim.c" -o "$OUT/test_trim$sfx.o"
  g++ "$@" "$OUT/test_trim$sfx.o" "$OUT/trim$sfx.o" "${REST[@]}" -Wl,--gc-sections -lpthread -lm -o "$OUT/test_trim$sfx"
}
build ""
"$OUT/test_trim"
build "_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 "$OUT/test_trim_asan"
