#!/usr/bin/env bash
# Shared by the host tests that link the real pinned Blender 3.6.23 legacy GP code: sets ROOT, B, BL,
# OUT (temp dir, removed on exit), INC, CF, XF and compiles the closure into OBJS.
# Requires tools/import_blender_gp.sh and tools/android_blender_gp_generate_dna.sh to have run;
# A missing source fails; PG_ALLOW_SKIP=1 (local only, never CI) turns it into a skip.
# usage: source tools/native_host_closure.sh "<test name>"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$ROOT/third_party/blender"
BL="$B/source/blender"
if [[ ! -f "$BL/blenkernel/intern/gpencil_geom_legacy.cc" || ! -f "$ROOT/build/blender-dna/dna.c" ]]; then
  if [[ "${PG_ALLOW_SKIP:-0}" == "1" && "${PG_REQUIRE_BLENDER_SOURCE:-0}" != "1" ]]; then
    echo "SKIP $1 (run tools/import_blender_gp.sh and the DNA generator first)"
    exit 0
  fi
  echo "ERROR: $1: missing pinned Blender source or generated DNA (set PG_ALLOW_SKIP=1 to skip locally)" >&2
  exit 1
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
  "$ROOT/native/blender_gp/project_grease_modifier_stack2.c"
  "$ROOT/native/blender_gp/project_grease_blender_build.c"
  "$ROOT/native/blender_gp/project_grease_blender_interp.c"
  "$ROOT/native/blender_gp/project_grease_blender_fill.c"
  "$ROOT/native/blender_gp/project_grease_blender_mod2.c"
  "$ROOT/native/blender_gp/project_grease_curvemap.c"
  "$ROOT/native/blender_gp/project_grease_shader_fx.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit2.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit3.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit4.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit5.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit6.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit7.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit8.c"
  "$ROOT/native/blender_gp/project_grease_blender_edit9.c"
  "$ROOT/native/blender_gp/project_grease_tool_session.c"
  "$ROOT/native/blender_gp/project_grease_draw_input.c"
  "$ROOT/native/blender_gp/project_grease_tool_util.c"
  "$ROOT/native/blender_gp/project_grease_tool_sculpt.c"
  "$ROOT/native/blender_gp/project_grease_tool_vertex_paint.c"
  "$ROOT/native/blender_gp/project_grease_tool_weight_paint.c"
  "$ROOT/native/blender_gp/project_grease_annotations.c"
  "$ROOT/native/blender_gp/android_legacy_runtime_compat.c"
  "$ROOT/native/blender_gp/android_blender_math_vector_compat.c"
  "$ROOT/native/blender_gp/android_blender_math_matrix_compat.c"
  "$ROOT/native/blender_gp/android_blender_math_rotation_compat.c"
  "$ROOT/native/blender_gp/android_blender_string_compat.c"
  "$ROOT/native/blender_gp/android_blender_string_utf8_compat.c"
  "$ROOT/build/blender-dna/dna.c"
  "$BL/makesdna/intern/dna_utils.c"
  "$BL/blenlib/intern/BLI_assert.c" "$BL/blenlib/intern/array_utils.c" "$BL/blenlib/intern/noise.c" "$BL/blenlib/intern/math_geom.c"
  "$BL/blenlib/intern/lasso_2d.c" "$BL/blenlib/intern/polyfill_2d.c" "$BL/blenlib/intern/BLI_memarena.c"
  "$BL/blenlib/intern/BLI_heap.c" "$BL/blenlib/intern/string_utils.c" "$BL/blenlib/intern/stack.c"
  "$BL/blenlib/intern/BLI_ghash.c" "$BL/blenlib/intern/BLI_ghash_utils.c" "$BL/blenlib/intern/BLI_mempool.c"
  "$BL/blenlib/intern/hash_mm2a.c" "$BL/blenlib/intern/rct.c" "$BL/blenlib/intern/kdtree_2d.c"
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
