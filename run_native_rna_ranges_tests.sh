#!/usr/bin/env bash
# L4 host test: (1) regenerates the RNA metadata from Blender 3.6.23's own RNA definitions and fails
# if native/blender_gp/rna/project_grease_rna_gen.c differs; (2) checks Project Grease's parameter
# sanitizing against those limits, plain and under ASan/UBSan.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DUMP="$(mktemp)"; trap 'rm -f "$DUMP"' EXIT
bash "$HERE/rna_ranges/run_rna_dump.sh" "$DUMP"
python3 -I "$HERE/rna_ranges/gen_rna_table.py" "$DUMP" --check
source "$HERE/native_host_closure.sh" "RNA ranges test"
PF=(-funsigned-char -fno-strict-aliasing -ffp-contract=off) # Blender platform_unix.cmake
build() {
  local sfx="$1"; shift
  gcc -std=gnu11 -Wall "${PF[@]}" "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/tests/test_rna_ranges.c" -o "$OUT/test_rna_ranges$sfx.o"
  g++ "$@" "$OUT/test_rna_ranges$sfx.o" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_rna_ranges$sfx"
}
build ""
timeout 60s "$OUT/test_rna_ranges"
build "_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 60s "$OUT/test_rna_ranges_asan"
# Every mapped parameter at Blender's hard limits through PG's modifiers. The PG sources (where the
# rewritten modifier code is) are instrumented too; the verbatim Blender objects are reused as built.
AOBJS=()
for s in "${C_SRC[@]}"; do
  if [[ "$s" == "$ROOT/native/"* ]]; then
    o="$OUT/x_$(basename "$s").o"; gcc "${CF[@]}" -g -fsanitize=address,undefined -fno-omit-frame-pointer "${INC[@]}" -c "$s" -o "$o"
  else o="$OUT/$(basename "$s").o"; fi
  AOBJS+=("$o")
done
for s in "${CXX_SRC[@]}"; do AOBJS+=("$OUT/$(basename "$s").o"); done
g++ "${XF[@]}" -g -fsanitize=address,undefined -fno-omit-frame-pointer "${INC[@]}" "$ROOT/native/blender_gp/tests/test_rna_extremes.cc" \
  "${AOBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_rna_extremes"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 timeout 600s "$OUT/test_rna_extremes"
