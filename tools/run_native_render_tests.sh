#!/usr/bin/env bash
# Pixel tests of the Android GLES presenter on a software GLES2 context (Mesa llvmpipe, EGL
# surfaceless). Needs libegl/libgles/mesa (apt: libegl1 libgles2 libegl-dev libgles-dev
# libegl-mesa0 libgl1-mesa-dri); PG_REQUIRE_GL=1 (CI) fails instead of skipping without GL.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "render test"
if ! echo '#include <EGL/egl.h>' | g++ -x c++ -fsyntax-only - 2>/dev/null; then
  if [[ "${PG_REQUIRE_GL:-0}" == "1" ]]; then echo "EGL headers missing" >&2; exit 1; fi
  echo "SKIP render test (EGL/GLES development files missing)"; exit 0
fi
g++ "${XF[@]}" -D__ANDROID__ -DWITH_OPENGL "${INC[@]}" -c "$ROOT/native/blender_gp/android_gp_presentation.cpp" -o "$OUT/presentation.o"
g++ "${XF[@]}" -D__ANDROID__ "${INC[@]}" -c "$ROOT/native/blender_gp/android_gp_shader_fx.cpp" -o "$OUT/shader_fx.o"
gcc -std=gnu11 -O1 "${INC[@]}" -c "$ROOT/native/blender_gp/project_grease_stroke_outline.c" -o "$OUT/stroke_outline.o"
g++ "${XF[@]}" -Wall "${INC[@]}" "$ROOT/native/blender_gp/tests/test_render.cc" "$OUT/presentation.o" "$OUT/shader_fx.o" "$OUT/stroke_outline.o" "${OBJS[@]}" \
  -Wl,--gc-sections -lEGL -lGLESv2 -ldl -lpthread -lm -o "$OUT/test_render"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 "$OUT/test_render"
