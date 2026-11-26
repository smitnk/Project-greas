#!/usr/bin/env bash
# Undo system host tests (project_grease_undo.h: Blender 3.6.23 undo_system.cc, memfile chunk
# sharing, armature Edit mode undo) through the real Backend and bridge.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "undo"
BSRC_CXX=(
  "$ROOT/native/blender_gp/project_grease_gp_backend.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_fill.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_eraser.cpp"
  "$ROOT/native/blender_gp/project_grease_armature_bridge.cpp"
  "$ROOT/native/blender_gp/project_grease_rig_bridge.cpp"
)
BSRC_C=(
  "$ROOT/native/blender_gp/project_grease_legacy_sbuffer.c"
  "$ROOT/native/blender_gp/project_grease_blender_primitive.c"
  "$ROOT/native/blender_gp/project_grease_blender_select.c"
  "$ROOT/native/blender_gp/project_grease_blender_eraser.c"
  "$ROOT/native/blender_gp/project_grease_document_state.c"
  "$ROOT/native/blender_gp/project_grease_wm.c"
  "$ROOT/native/blender_gp/project_grease_wm_layer.c"
  "$ROOT/native/blender_gp/project_grease_wm_stroke.c"
  "$B/source/blender/blenlib/intern/math_color.c"
)
BOBJS=()
for s in "${BSRC_CXX[@]}"; do o="$OUT/b_$(basename "$s").o"; g++ "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$s" -o "$o"; BOBJS+=("$o"); done
for s in "${BSRC_C[@]}"; do o="$OUT/b_$(basename "$s").o"; gcc "${CF[@]}" -D__ANDROID__ "${INC[@]}" -c "$s" -o "$o"; BOBJS+=("$o"); done
g++ "${XF[@]}" -D__ANDROID__ "${INC[@]}" "$ROOT/native/blender_gp/tests/test_undo_system.cc" \
  "${BOBJS[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_undo_system" > "$OUT/link.txt" 2>&1 || {
  grep -E "error|undefined reference|multiple definition" "$OUT/link.txt" | sort | uniq -c | sort -rn | head -40; exit 1; }
timeout 300s "$OUT/test_undo_system" 2>/dev/null
