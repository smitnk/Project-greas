#!/usr/bin/env bash
# Rig host tests: bone matrices, pose evaluation, F-Curve interpolation, armature modifier deform
# and bind weights of project_grease_armature*.c / .cc against Blender 3.6.23's maths.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "armature"
g++ "${XF[@]}" "${INC[@]}" "$ROOT/native/blender_gp/tests/test_armature.cc" "${OBJS[@]}" \
  -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_armature" > "$OUT/link.txt" 2>&1 || {
  grep -E "error|undefined reference|multiple definition" "$OUT/link.txt" | sort | uniq -c | sort -rn | head -40; exit 1; }
timeout 120s "$OUT/test_armature"

# The deform of the device test through the real Backend + bridge (stroke, bind, pose, key, apply).
BSRC_CXX=(gp_backend legacy_fill legacy_eraser armature_bridge rig_bridge)
BSRC_C=(legacy_sbuffer blender_primitive blender_select blender_eraser document_state wm wm_layer wm_stroke)
BOBJS=()
for s in "${BSRC_CXX[@]}"; do o="$OUT/b_$s.o"; g++ "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_$s.cpp" -o "$o"; BOBJS+=("$o"); done
for s in "${BSRC_C[@]}"; do o="$OUT/b_$s.o"; gcc "${CF[@]}" -D__ANDROID__ "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_$s.c" -o "$o"; BOBJS+=("$o"); done
gcc "${CF[@]}" "${INC[@]}" -c "$B/source/blender/blenlib/intern/math_color.c" -o "$OUT/b_math_color.o"; BOBJS+=("$OUT/b_math_color.o")
for T in test_backend_rig_deform test_backend_rig_eval_order test_backend_rig_extrude_ui test_backend_rig_bbone; do
g++ "${XF[@]}" -D__ANDROID__ "${INC[@]}" "$ROOT/native/blender_gp/tests/$T.cc" \
  "${BOBJS[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/$T" > "$OUT/link2.txt" 2>&1 || {
  grep -E "error|undefined reference|multiple definition" "$OUT/link2.txt" | sort | uniq -c | sort -rn | head -40; exit 1; }
timeout 120s "$OUT/$T" 2>/dev/null
done
