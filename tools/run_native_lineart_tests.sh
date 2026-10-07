#!/usr/bin/env bash
# Host test for Line Art batch 2 (native/blender_gp/lineart): the generated verbatim Blender 3.6.23
# Line Art core on Scene-lite, linked with the real pinned closure (see native_host_closure.sh),
# plain and with the Line Art / Scene-lite objects under ASan/UBSan.
# With LINEART_REFERENCE_DIR set (CI, after tools/lineart_reference/run_blender_reference.sh), the
# result is also compared against Blender's own Line Art output for the reference scenes.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "Line Art test"
python3 "$ROOT/tools/gen_lineart_lite.py" --check
LA="$BL/gpencil_modifiers_legacy/intern/lineart"
LINC=(-I"$ROOT/native/blender_gp/lineart" -I"$LA" -I"$BL/gpencil_modifiers_legacy")
TDEF=(-DPG_LINEART_SCENES="\"$ROOT/tools/lineart_reference\"")
# Blender 3.6.23 task pool / parallel range / spin locks / PIL timer (L1 closure, no TBB): these
# replaced the old custom copies in lineart_lite_runtime.cc, so the host test links them too.
RT_OBJS=()
for s in "$BL/blenlib/intern/time.c" "$BL/blenlib/intern/gsqueue.c"; do
  o="$OUT/rt_$(basename "$s").o"; gcc "${CF[@]}" "${INC[@]}" -c "$s" -o "$o"; RT_OBJS+=("$o")
done
for s in "$BL/blenlib/intern/task_pool.cc" "$BL/blenlib/intern/task_range.cc" \
         "$BL/blenlib/intern/task_scheduler.cc" "$BL/blenlib/intern/threads.cc"; do
  o="$OUT/rt_$(basename "$s").o"; g++ "${XF[@]}" "${INC[@]}" -c "$s" -o "$o"; RT_OBJS+=("$o")
done
build() { # $1 = suffix, rest = extra flags for the Line Art / Scene-lite / test objects
  local sfx="$1"; shift
  g++ "${XF[@]}" -g "$@" "${LINC[@]}" "${INC[@]}" -c "$ROOT/native/blender_gp/lineart/project_grease_lineart_cpu.cc" -o "$OUT/lineart_cpu$sfx.o"
  g++ "${XF[@]}" "$@" "${LINC[@]}" "${INC[@]}" -c "$ROOT/native/blender_gp/lineart/lineart_lite_runtime.cc" -o "$OUT/lineart_runtime$sfx.o"
  gcc "${CF[@]}" -g "$@" "${LINC[@]}" "${INC[@]}" -c "$LA/lineart_util.c" -o "$OUT/lineart_util$sfx.o"
  gcc "${CF[@]}" "$@" "${LINC[@]}" "${INC[@]}" -c "$LA/lineart_chain.c" -o "$OUT/lineart_chain$sfx.o"
  gcc "${CF[@]}" "$@" "${LINC[@]}" "${INC[@]}" -c "$ROOT/native/blender_gp/lineart/project_grease_lineart_shadow.c" -o "$OUT/lineart_shadow$sfx.o"
  gcc "${CF[@]}" -g "$@" "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_scene_lite.c" -o "$OUT/scene_lite$sfx.o"
  gcc -std=gnu11 -g -Wall "$@" -I"$ROOT/native/blender_gp" -c "$ROOT/native/blender_gp/tests/test_lineart.c" -o "$OUT/test_lineart$sfx.o"
  gcc -std=gnu11 -g -Wall "$@" "${TDEF[@]}" -I"$ROOT/native/blender_gp" -c "$ROOT/native/blender_gp/tests/test_lineart_reference.c" -o "$OUT/test_lineart_reference$sfx.o"
  g++ "$@" "$OUT/test_lineart$sfx.o" "$OUT/test_lineart_reference$sfx.o" "$OUT/lineart_cpu$sfx.o" "$OUT/lineart_runtime$sfx.o" \
    "$OUT/lineart_util$sfx.o" "$OUT/lineart_chain$sfx.o" "$OUT/lineart_shadow$sfx.o" "$OUT/scene_lite$sfx.o" "${RT_OBJS[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_lineart$sfx"
}
run_lineart() {
  local bin="$1"
  local log="$OUT/$(basename "$bin").log"
  set +e
  timeout 90s "$bin" ${LINEART_REFERENCE_DIR:+"$LINEART_REFERENCE_DIR"} 2>&1 | tee "$log"
  local rc=${PIPESTATUS[0]}
  set -e
  if grep -q "LINEART WATCHDOG" "$log"; then
    echo "LINEART WATCHDOG: resolving captured addresses"
    local text_vma
    text_vma="$(readelf -SW "$bin" | awk '$2 == ".text" {print "0x"$4; exit}')"
    grep -o '(+0x[0-9a-fA-F]*)' "$log" | sed -E 's/^\\(\\+0x//; s/\\)$//' | while read -r off; do
      printf '0x%x\\n' "$((16#$off - text_vma))"
    done | addr2line -j .text -Cfipe "$bin" || true
  fi
  return "$rc"
}
build ""
run_lineart "$OUT/test_lineart"
build "_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 run_lineart "$OUT/test_lineart_asan"
