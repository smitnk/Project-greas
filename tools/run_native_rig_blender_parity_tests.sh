#!/usr/bin/env bash
# Rig vs a real Blender run: builds the identical rig through the real Backend + bridge and
# (RIG_BLENDER_RUN=<json from tools/rig_reference/blender_rig_reference.py>) compares pose bone
# matrices, head / tail, evaluated stroke points and bind weights with Blender 3.6.23's results.
# Without RIG_BLENDER_RUN it runs the self-consistency check. RIG_BLENDER_REPORT=<file> also writes the per-case lines.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "rig blender parity"
BSRC_CXX=(gp_backend legacy_fill legacy_primitive legacy_eraser legacy_sculpt armature_bridge rig_bridge)
BSRC_C=(legacy_sbuffer blender_primitive blender_select blender_eraser document_state wm wm_layer wm_stroke)
BOBJS=()
for s in "${BSRC_CXX[@]}"; do o="$OUT/b_$s.o"; g++ "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_$s.cpp" -o "$o"; BOBJS+=("$o"); done
for s in "${BSRC_C[@]}"; do o="$OUT/b_$s.o"; gcc "${CF[@]}" -D__ANDROID__ "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_$s.c" -o "$o"; BOBJS+=("$o"); done
gcc "${CF[@]}" "${INC[@]}" -c "$B/source/blender/blenlib/intern/math_color.c" -o "$OUT/b_math_color.o"; BOBJS+=("$OUT/b_math_color.o")
g++ "${XF[@]}" -D__ANDROID__ "${INC[@]}" "$ROOT/native/blender_gp/tests/test_rig_blender_parity.cc" \
  "${BOBJS[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_rig_blender_parity" > "$OUT/link.txt" 2>&1 || {
  grep -E "error|undefined reference|multiple definition" "$OUT/link.txt" | sort | uniq -c | sort -rn | head -40; exit 1; }
timeout 300s "$OUT/test_rig_blender_parity" 2>/dev/null
