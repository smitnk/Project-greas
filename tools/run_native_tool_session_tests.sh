#!/usr/bin/env bash
# Host tests of the native tool session (draw input engine + sculpt / vertex paint / weight paint
# ports) against the real pinned Blender closure, plain and with the session objects under
# ASan/UBSan. The draw input engine is also checked against tools/draw_input_golden.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "tool session test"
G="$ROOT/native/blender_gp"
SESSION=(project_grease_tool_session.c project_grease_draw_input.c project_grease_tool_util.c
         project_grease_tool_sculpt.c project_grease_tool_vertex_paint.c project_grease_tool_weight_paint.c)
REST=()
for o in "${OBJS[@]}"; do
  skip=0
  for s in "${SESSION[@]}"; do [[ "$o" == */$s.o ]] && skip=1; done
  [[ $skip == 0 ]] && REST+=("$o")
done
build() { # $1 = suffix, rest = extra flags for the session / test objects
  local sfx="$1"; shift
  local objs=()
  for s in "${SESSION[@]}"; do gcc "${CF[@]}" "$@" "${INC[@]}" -c "$G/$s" -o "$OUT/$s$sfx.o"; objs+=("$OUT/$s$sfx.o"); done
  gcc "${CF[@]}" "$@" "${INC[@]}" -c "$G/tests/test_tool_session.c" -o "$OUT/tts$sfx.o"
  g++ "$@" "$OUT/tts$sfx.o" "${objs[@]}" "${REST[@]}" -Wl,--gc-sections -lpthread -lm -o "$OUT/test_tool_session$sfx"
  gcc -std=gnu11 -Wall "$@" -I"$G" "$G/tests/test_draw_input.c" "$G/project_grease_draw_input.c" -lm -o "$OUT/test_draw_input$sfx"
}
build ""
"$OUT/test_draw_input" "$G/tests/draw_input_golden.txt"
"$OUT/test_tool_session"
build "_asan" -g -fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=0 "$OUT/test_draw_input_asan" "$G/tests/draw_input_golden.txt"
ASAN_OPTIONS=detect_leaks=0 "$OUT/test_tool_session_asan"
