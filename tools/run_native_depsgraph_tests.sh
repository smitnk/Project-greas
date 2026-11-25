#!/usr/bin/env bash
# Depsgraph host tests (project_grease_depsgraph.cc, Blender 3.6.23 depsgraph builder / flush /
# evaluation and the pose / driver / animation callbacks): the operation order of a rig with
# Child Of, IK and drivers in both declaration orders, then through the real Backend + bridge the
# deferred evaluation of a loaded rig, driver refresh after a pose edit and the per-frame
# copy-on-write rig of onion-skin and exported frames.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "depsgraph"
BSRC_CXX=(gp_backend legacy_fill legacy_eraser armature_bridge rig_bridge)
BSRC_C=(legacy_sbuffer blender_primitive blender_select blender_eraser document_state wm wm_layer wm_stroke)
BOBJS=()
for s in "${BSRC_CXX[@]}"; do o="$OUT/b_$s.o"; g++ "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_$s.cpp" -o "$o"; BOBJS+=("$o"); done
for s in "${BSRC_C[@]}"; do o="$OUT/b_$s.o"; gcc "${CF[@]}" -D__ANDROID__ "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_$s.c" -o "$o"; BOBJS+=("$o"); done
gcc "${CF[@]}" "${INC[@]}" -c "$B/source/blender/blenlib/intern/math_color.c" -o "$OUT/b_math_color.o"; BOBJS+=("$OUT/b_math_color.o")
g++ "${XF[@]}" -D__ANDROID__ "${INC[@]}" "$ROOT/native/blender_gp/tests/test_depsgraph.cc" \
  "${BOBJS[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_depsgraph" > "$OUT/link.txt" 2>&1 || {
  grep -E "error|undefined reference|multiple definition" "$OUT/link.txt" | sort | uniq -c | sort -rn | head -40; exit 1; }
timeout 120s "$OUT/test_depsgraph" 2>/dev/null
