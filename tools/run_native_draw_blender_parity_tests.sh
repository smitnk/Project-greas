#!/usr/bin/env bash
# Draw input vs a real Blender run: replays the traces of tools/draw_input_golden/blender_draw_reference.py
# through the native Draw input (Blender's gpencil_paint.c code, project_grease_blender_paint.c) and
# compares the committed stroke points. DRAW_BLENDER_RUN=<file> uses a fresh Blender output instead of
# the committed native/blender_gp/tests/draw_blender_golden.txt; DRAW_BLENDER_REPORT=<file> writes the
# per-brush max differences.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "draw blender parity"
G="$ROOT/native/blender_gp"
gcc "${CF[@]}" "${INC[@]}" -c "$G/project_grease_legacy_sbuffer.c" -o "$OUT/sbuffer.o"
gcc -std=gnu11 -Wall "${INC[@]}" -c "$G/tests/test_draw_blender_parity.c" -o "$OUT/tdbp.o"
g++ "$OUT/tdbp.o" "$OUT/sbuffer.o" "${OBJS[@]}" -Wl,--gc-sections -lpthread -lm -o "$OUT/test_draw_blender_parity"
"$OUT/test_draw_blender_parity" "${DRAW_BLENDER_RUN:-$G/tests/draw_blender_golden.txt}" ${DRAW_BLENDER_REPORT:+"$DRAW_BLENDER_REPORT"}
