#!/usr/bin/env bash
# L8 part 2: Blender 3.6.23's GP draw engine (unmodified draw/engines/gpencil + draw cache) through
# native/blender_gp/gles on a software OpenGL ES 3.2 context (Mesa llvmpipe, EGL surfaceless).
# Needs libegl / libgles / mesa like run_native_render_tests.sh; PG_REQUIRE_GL=1 (CI) fails
# instead of skipping without GL.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "L8 engine render test"
if ! echo '#include <EGL/egl.h>
#include <GLES3/gl32.h>' | g++ -x c++ -fsyntax-only - 2>/dev/null; then
  if [[ "${PG_ALLOW_SKIP:-0}" == "1" && "${PG_REQUIRE_GL:-0}" != "1" ]]; then
    echo "SKIP L8 engine render test (EGL/GLES 3.2 development files missing)"; exit 0
  fi
  echo "ERROR: L8 engine render test: EGL/GLES 3.2 development files missing (set PG_ALLOW_SKIP=1 to skip locally)" >&2; exit 1
fi
GP="$BL/draw/engines/gpencil"
LI=(-I"$ROOT/native/blender_gp/gles")
objs=()
cc() { local o="$OUT/l8_$(basename "$1").o"; gcc "${CF[@]}" "${INC[@]}" "${LI[@]}" -c "$1" -o "$o"; objs+=("$o"); }
cx() { local o="$OUT/l8_$(basename "$1").o"; g++ "${XF[@]}" "${INC[@]}" "${LI[@]}" -c "$1" -o "$o"; objs+=("$o"); }
for f in gpencil_engine.c gpencil_cache_utils.c gpencil_draw_data.c gpencil_shader.c gpencil_antialiasing.c gpencil_shader_fx.c; do cc "$GP/$f"; done
cc "$BL/blenlib/intern/smaa_textures.c"; cc "$BL/blenlib/intern/BLI_memblock.c"; cc "$BL/blenlib/intern/bitmap.c"
cc "$BL/blenlib/intern/math_color.c"; cc "$BL/makesdna/intern/dna_defaults.c"
cc "$B/release/datafiles/userdef/userdef_default.c"; cc "$B/release/datafiles/userdef/userdef_default_theme.c"
cc "$ROOT/native/blender_gp/gles/pg_gp_engine_glue.c"
for f in gpu_vertex_buffer.cc gpu_index_buffer.cc gpu_batch.cc gpu_vertex_format.cc gpu_capabilities.cc; do cx "$BL/gpu/intern/$f"; done
cx "$BL/draw/intern/draw_cache_impl_gpencil.cc"
cx "$ROOT/native/blender_gp/android_gpu_backend.cpp"
cx "$ROOT/native/blender_gp/gles/pg_gpu_gles.cc"
cx "$ROOT/native/blender_gp/gles/pg_drw_gles.cc"
# The Android presenter (engine path on GLES 3.2) as run_native_render_tests.sh builds it.
o="$OUT/l8_presentation.o"; g++ "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$ROOT/native/blender_gp/android_gp_presentation.cpp" -o "$o"; objs+=("$o")
o="$OUT/l8_shader_fx.o"; g++ "${XF[@]}" -D__ANDROID__ "${INC[@]}" -c "$ROOT/native/blender_gp/android_gp_shader_fx.cpp" -o "$o"; objs+=("$o")
o="$OUT/l8_stroke_outline.o"; gcc -std=gnu11 -O1 "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_stroke_outline.c" -o "$o"; objs+=("$o")
g++ "${XF[@]}" -Wall "${INC[@]}" "${LI[@]}" -I"$ROOT/native/blender_gp" "$ROOT/native/blender_gp/tests/test_l8_engine_render.cc" "${objs[@]}" "${OBJS[@]}" \
  -Wl,--gc-sections -lEGL -lGLESv2 -ldl -lpthread -lm -o "$OUT/test_l8_render" > "$OUT/link.txt" 2>&1 || {
  cat "$OUT/link.txt" | grep -v "pragma\|note:" | head -20
  grep -oE "(undefined reference to|multiple definition of) \`[^']*'" "$OUT/link.txt" | sort | uniq -c | sort -rn | head -60; exit 1; }
PG_DRW_TRACE=1 EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 timeout 300s "$OUT/test_l8_render"
# the OpenGL ES 3.1 path (3.1 context features through extensions, ESSL 3.10 shaders) on the same driver
echo "--- OpenGL ES 3.1 path (PG_FORCE_GLES31) ---"
PG_FORCE_GLES31=1 EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 timeout 300s "$OUT/test_l8_render"
