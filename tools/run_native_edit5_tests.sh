#!/usr/bin/env bash
# Host tests for segment select, material slot removal and onion settings
# (native/blender_gp/project_grease_blender_edit5.c) against the real pinned Blender closure, plain
# and with the edit5 / test objects under ASan/UBSan.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "edit5 test"
REST=()
for o in "${OBJS[@]}"; do [[ "$o" == */project_grease_blender_edit5.c.o ]] || REST+=("$o"); done
build() { # $1 = suffix, rest = extra flags for the edit5 / test objects
  local sfx="$1"; shift
  gcc "${CF[@]}" "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_blender_edit5.c" -o "$OUT/edit5$sfx.o"
  gcc "${CF[@]}" "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/tests/test_edit5.c" -o "$OUT/test_edit5$sfx.o"
  g++ "$@" "$OUT/test_edit5$sfx.o" "$OUT/edit5$sfx.o" "${REST[@]}" -Wl,--gc-sections -lpthread -lm -o "$OUT/test_edit5$sfx"
}
build ""
"$OUT/test_edit5"
build "_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 "$OUT/test_edit5_asan"
