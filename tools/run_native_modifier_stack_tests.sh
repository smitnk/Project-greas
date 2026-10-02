#!/usr/bin/env bash
# Host tests for the live modifier stack (native/blender_gp/project_grease_modifier_stack.c).
# Links the REAL pinned Blender 3.6.23 stroke code (gpencil_geom_legacy.cc, gpencil_legacy.c, ...),
# so tools/import_blender_gp.sh and tools/android_blender_gp_generate_dna.sh must have run.
# Set PG_REQUIRE_BLENDER_SOURCE=1 (CI) to fail instead of skip when the source is missing.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$ROOT/third_party/blender"
BL="$B/source/blender"
if [[ ! -f "$BL/blenkernel/intern/gpencil_geom_legacy.cc" || ! -f "$ROOT/build/blender-dna/dna.c" ]]; then
  if [[ "${PG_REQUIRE_BLENDER_SOURCE:-0}" == "1" ]]; then
    echo "Pinned Blender source or generated DNA missing" >&2
    exit 1
  fi
  echo "SKIP modifier stack test (run tools/import_blender_gp.sh and the DNA generator first)"
  exit 0
fi
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
INC=(-I"$BL/draw" -I"$BL/draw/intern" -I"$BL/draw/engines/gpencil" -I"$BL/gpu" -I"$BL/gpu/intern" -I"$ROOT/native/blender_gp/android_compat" -I"$ROOT/native/blender_gp" -I"$BL" -I"$B/source"
     -I"$BL/blenlib" -I"$BL/blenkernel" -I"$BL/makesdna" -I"$BL/makesrna" -I"$BL/depsgraph"
     -I"$BL/gpencil_modifiers_legacy" -I"$BL/blentranslation" -I"$BL/blentranslation/intern"
     -I"$BL/imbuf" -I"$BL/blenloader" -I"$BL/gpu" -I"$BL/gpu/intern" -I"$BL/draw" -I"$BL/bmesh" -I"$BL/editors/include" -I"$BL/windowmanager" -I"$BL/render"
     -I"$B/extern/curve_fit_nd" -I"$B/extern/wcwidth" -I"$B/intern/guardedalloc" -I"$B/intern/atomic"
     -I"$B/intern/clog" -I"$ROOT/build/blender-dna")
CF=(-std=gnu11 -DNDEBUG -DMATH_STANDALONE -w -DMALLOC_USABLE_SIZE_DISABLED -ffunction-sections -fdata-sections)
XF=(-std=gnu++17 -DNDEBUG -w -ffunction-sections -fdata-sections)
C_SRC=(
  "$ROOT/native/blender_gp/project_grease_modifier_stack.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit.c"
  "$ROOT/native/blender_gp/android_legacy_runtime_compat.c"
  "$ROOT/native/blender_gp/android_blender_math_vector_compat.c"
  "$ROOT/native/blender_gp/android_blender_math_matrix_compat.c"
  "$ROOT/native/blender_gp/android_blender_math_rotation_compat.c"
  "$ROOT/native/blender_gp/android_blender_string_compat.c"
  "$ROOT/native/blender_gp/android_blender_string_utf8_compat.c"
  "$ROOT/build/blender-dna/dna.c"
  "$BL/makesdna/intern/dna_utils.c"
  "$BL/blenlib/intern/array_utils.c" "$BL/blenlib/intern/noise.c" "$BL/blenlib/intern/math_geom.c"
  "$BL/blenlib/intern/lasso_2d.c" "$BL/blenlib/intern/polyfill_2d.c" "$BL/blenlib/intern/BLI_memarena.c"
  "$BL/blenlib/intern/BLI_heap.c" "$BL/blenlib/intern/string_utils.c"
  "$BL/blenkernel/intern/gpencil_curve_legacy.c" "$BL/blenkernel/intern/gpencil_legacy.c"
  "$B/extern/curve_fit_nd/intern/curve_fit_cubic.c" "$B/extern/curve_fit_nd/intern/curve_fit_cubic_refit.c"
  "$B/extern/curve_fit_nd/intern/curve_fit_corners_detect.c" "$B/extern/curve_fit_nd/intern/generic_heap.c"
  "$BL/blentranslation/intern/blt_translation.c"
  "$B/intern/guardedalloc/intern/mallocn.c" "$B/intern/guardedalloc/intern/mallocn_guarded_impl.c"
  "$B/intern/guardedalloc/intern/mallocn_lockfree_impl.c"
)
CXX_SRC=(
  "$BL/blenkernel/intern/gpencil_geom_legacy.cc" "$BL/blenkernel/intern/deform.cc"
  "$BL/blenlib/intern/listbase.cc" "$BL/blenlib/intern/rand.cc"
  "$ROOT/native/blender_gp/project_grease_legacy_curve_helpers.cc"
  "$B/intern/guardedalloc/intern/leak_detector.cc" "$B/intern/guardedalloc/intern/memory_usage.cc"
)
OBJS=()
for s in "${C_SRC[@]}"; do o="$OUT/$(basename "$s").o"; gcc "${CF[@]}" "${INC[@]}" -c "$s" -o "$o"; OBJS+=("$o"); done
for s in "${CXX_SRC[@]}"; do o="$OUT/$(basename "$s").o"; g++ "${XF[@]}" "${INC[@]}" -c "$s" -o "$o"; OBJS+=("$o"); done
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
