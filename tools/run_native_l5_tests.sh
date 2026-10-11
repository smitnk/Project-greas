#!/usr/bin/env bash
# L5 host tests: Blender 3.6.23 BKE kernels now in the closure (colortools.c, colorband.c, curve.cc,
# gpencil_update_cache_legacy.c) and Project Grease's code that must agree with them. Plain and
# under ASan/UBSan.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "L5 BKE test"
PF=(-funsigned-char -fno-strict-aliasing -ffp-contract=off) # Blender platform_unix.cmake
for t in test_curvemap test_l5_curvemap_parity; do
  for v in plain asan; do
    X=(); [[ $v == asan ]] && X=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
    gcc -std=gnu11 -Wall "${PF[@]}" "${X[@]}" "${INC[@]}" -c "$ROOT/native/blender_gp/tests/$t.c" -o "$OUT/$t.$v.o"
    g++ "${X[@]}" "$OUT/$t.$v.o" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/$t.$v"
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 120s "$OUT/$t.$v"
  done
done
