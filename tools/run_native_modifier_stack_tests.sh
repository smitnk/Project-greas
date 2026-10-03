#!/usr/bin/env bash
# Host tests for the live modifier stack (native/blender_gp/project_grease_modifier_stack.c).
# Links the REAL pinned Blender 3.6.23 stroke code (see tools/native_host_closure.sh).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "modifier stack test"
g++ "${XF[@]}" -Wall "${INC[@]}" "$ROOT/native/blender_gp/tests/test_modifier_stack.cc" "${OBJS[@]}" \
  -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_modifier_stack"

# Backend wiring (storage per layer, undo/redo, apply, evaluated-frame cache): the real backend is
# compiled for Android (no Blender Main) and the draw/GPU layer is not linked; nothing here renders.
BSRC_CXX=(
  "$ROOT/native/blender_gp/project_grease_gp_backend.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_fill.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_primitive.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_eraser.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_sculpt.cpp"
)
BSRC_C=(
  "$ROOT/native/blender_gp/project_grease_legacy_sbuffer.c"
  "$ROOT/native/blender_gp/project_grease_blender_primitive.c"
  "$ROOT/native/blender_gp/project_grease_blender_select.c"
  "$ROOT/native/blender_gp/project_grease_blender_eraser.c"
  "$ROOT/native/blender_gp/project_grease_document_state.c"
)
BOBJS=()
for s in "${BSRC_CXX[@]}"; do o="$OUT/b_$(basename "$s").o"; g++ "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$s" -o "$o"; BOBJS+=("$o"); done
for s in "${BSRC_C[@]}"; do o="$OUT/b_$(basename "$s").o"; gcc "${CF[@]}" -D__ANDROID__ "${INC[@]}" -c "$s" -o "$o"; BOBJS+=("$o"); done
g++ "${XF[@]}" -Wall -D__ANDROID__ "${INC[@]}" "$ROOT/native/blender_gp/tests/test_backend_modifier_stack.cc" \
  "${BOBJS[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_backend_modifier_stack"
"$OUT/test_backend_modifier_stack" 2>/dev/null
"$OUT/test_modifier_stack"

# Backend wiring (storage per layer, undo/redo, apply, evaluated-frame cache): the real backend is
# compiled for Android (no Blender Main) and the draw/GPU layer is not linked; nothing here renders.
BSRC_CXX=(
  "$ROOT/native/blender_gp/project_grease_gp_backend.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_fill.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_primitive.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_eraser.cpp"
  "$ROOT/native/blender_gp/project_grease_legacy_sculpt.cpp"
)
BSRC_C=(
  "$ROOT/native/blender_gp/project_grease_legacy_sbuffer.c"
  "$ROOT/native/blender_gp/project_grease_blender_primitive.c"
  "$ROOT/native/blender_gp/project_grease_blender_select.c"
  "$ROOT/native/blender_gp/project_grease_blender_eraser.c"
  "$ROOT/native/blender_gp/project_grease_document_state.c"
)
BOBJS=()
for s in "${BSRC_CXX[@]}"; do o="$OUT/b_$(basename "$s").o"; g++ "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$s" -o "$o"; BOBJS+=("$o"); done
for s in "${BSRC_C[@]}"; do o="$OUT/b_$(basename "$s").o"; gcc "${CF[@]}" -D__ANDROID__ "${INC[@]}" -c "$s" -o "$o"; BOBJS+=("$o"); done
g++ "${XF[@]}" -Wall -D__ANDROID__ "${INC[@]}" "$ROOT/native/blender_gp/tests/test_backend_modifier_stack.cc" \
  "${BOBJS[@]}" "${OBJS[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_backend_modifier_stack"
"$OUT/test_backend_modifier_stack" 2>/dev/null

# Batch 21 document-state commands (edit8) against the real DNA.
g++ "${XF[@]}" -Wall "${INC[@]}" "$ROOT/native/blender_gp/tests/test_edit8.cc" "${OBJS[@]}" \
  -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_edit8"
"$OUT/test_edit8"
