#!/usr/bin/env bash
# L3 host test: Blender 3.6.23's DNA defaults table (dna_defaults.c + the userdef default data it
# references, all verbatim) linked into the closure, compared with every Project Grease default that
# has a direct DNA meaning. Plain and under ASan/UBSan.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "DNA defaults test"
PF=(-funsigned-char -fno-strict-aliasing -ffp-contract=off) # Blender platform_unix.cmake
L3_SRC=("$BL/makesdna/intern/dna_defaults.c"
        "$B/release/datafiles/userdef/userdef_default.c" "$B/release/datafiles/userdef/userdef_default_theme.c")
build() {
  local sfx="$1"; shift; local objs=()
  for s in "${L3_SRC[@]}"; do
    o="$OUT/l3_$(basename "$s")$sfx.o"; gcc "${CF[@]}" "${PF[@]}" "$@" "${INC[@]}" -c "$s" -o "$o"; objs+=("$o")
  done
  gcc -std=gnu11 -Wall "${PF[@]}" "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/tests/test_dna_defaults.c" -o "$OUT/test_dna_defaults$sfx.o"
  g++ "$@" "$OUT/test_dna_defaults$sfx.o" "${objs[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_dna_defaults$sfx"
}
build ""
timeout 60s "$OUT/test_dna_defaults"
build "_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 60s "$OUT/test_dna_defaults_asan"
