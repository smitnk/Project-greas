#!/usr/bin/env bash
# L6 host tests: Blender 3.6.23 copy-on-write behaviour of Project Grease's evaluated data.
# Plain and under ASan/UBSan (PG code instrumented; verbatim Blender objects as built).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "L6 depsgraph test"
for v in plain asan; do
  X=(); [[ $v == asan ]] && X=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
  OB=()
  for s in "${C_SRC[@]}"; do
    if [[ $v == asan && "$s" == "$ROOT/native/"* ]]; then
      o="$OUT/l6a_$(basename "$s").o"; gcc "${CF[@]}" "${X[@]}" "${INC[@]}" -c "$s" -o "$o"
    else o="$OUT/$(basename "$s").o"; fi
    OB+=("$o")
  done
  for s in "${CXX_SRC[@]}"; do OB+=("$OUT/$(basename "$s").o"); done
  g++ "${XF[@]}" "${X[@]}" "${INC[@]}" "$ROOT/native/blender_gp/tests/test_l6_cow_links.cc" "${OB[@]}" \
    -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_l6.$v"
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 120s "$OUT/test_l6.$v"
done
