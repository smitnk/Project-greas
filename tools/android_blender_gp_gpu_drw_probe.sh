#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLENDER="$ROOT/third_party/blender"
NDK="${ANDROID_NDK_ROOT:-}"
API="${ANDROID_API:-26}"

if [[ -z "$NDK" ]]; then
  echo "ANDROID_NDK_ROOT is required" >&2
  exit 2
fi

CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang++"
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang"

[[ -x "$CXX" ]] || { echo "Missing Android ARM64 C++ compiler: $CXX" >&2; exit 2; }
[[ -x "$CC" ]] || { echo "Missing Android ARM64 C compiler: $CC" >&2; exit 2; }
[[ -f "$BLENDER/source/blender/gpu/opengl/gl_backend.cc" ]] || {
  echo "Pinned Blender source is missing" >&2
  exit 2
}

OUT="$ROOT/build/android-blender-gp-gpu-draw-probe"
COMPAT="$ROOT/native/blender_gp/android_compat"
mkdir -p "$OUT"

INCLUDES=(
  "$COMPAT"
  "$BLENDER/source/blender"
  "$BLENDER/source"
  "$BLENDER/source/blender/blenkernel"
  "$BLENDER/source/blender/blenlib"
  "$BLENDER/source/blender/bmesh"
  "$BLENDER/source/blender/makesdna"
  "$BLENDER/source/blender/makesrna"
  "$BLENDER/source/blender/depsgraph"
  "$BLENDER/source/blender/editors/include"
  "$BLENDER/source/blender/windowmanager"
  "$BLENDER/source/blender/blentranslation"
  "$BLENDER/source/blender/blentranslation/intern"
  "$BLENDER/source/blender/imbuf"
  "$BLENDER/source/blender/blenloader"
  "$BLENDER/source/blender/draw"
  "$BLENDER/source/blender/draw/intern"
  "$BLENDER/source/blender/gpu"
  "$BLENDER/source/blender/gpu/intern"
  "$BLENDER/source/blender/render"
  "$BLENDER/source/blender/nodes"
  "$BLENDER/intern/atomic"
  "$BLENDER/intern/clog"
  "$BLENDER/intern/guardedalloc"
  "$BLENDER/intern/ghost"
  "$ROOT/build/blender-dna/source/blender/makesdna/intern"
)

FLAGS=(
  -D__ANDROID__
  -DNDEBUG
  -DWITH_OPENGL
  -DWITH_OPENGL_BACKEND
  -DGPU_OPENGL
  -fPIC
  -fsyntax-only
  -Wno-unused-command-line-argument
)

for inc in "${INCLUDES[@]}"; do
  FLAGS+=("-I$inc")
done

GPU_SOURCES=(
  "$BLENDER/source/blender/gpu/intern/gpu_batch.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_capabilities.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_context.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_framebuffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_index_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_init_exit.c"
  "$BLENDER/source/blender/gpu/intern/gpu_matrix.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_platform.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_shader.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_shader_dependency.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_shader_interface.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_shader_log.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_state.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_texture.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_uniform_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_vertex_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_vertex_format.cc"
)

OPENGL_SOURCES=(
  "$BLENDER/source/blender/gpu/opengl/gl_backend.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_batch.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_context.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_framebuffer.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_index_buffer.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_shader.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_shader_interface.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_state.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_texture.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_uniform_buffer.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_vertex_array.cc"
  "$BLENDER/source/blender/gpu/opengl/gl_vertex_buffer.cc"
)

DRW_SOURCES=(
  "$BLENDER/source/blender/draw/intern/draw_cache.c"
  "$BLENDER/source/blender/draw/intern/draw_manager.c"
  "$BLENDER/source/blender/draw/intern/draw_manager_exec.c"
  "$BLENDER/source/blender/draw/intern/draw_manager_shader.c"
  "$BLENDER/source/blender/draw/intern/draw_manager_texture.c"
  "$BLENDER/source/blender/draw/intern/draw_view.c"
  "$BLENDER/source/blender/draw/intern/draw_view_data.cc"
  "$BLENDER/source/blender/draw/intern/draw_cache_impl_gpencil.cc"
  "$BLENDER/source/blender/draw/engines/gpencil/gpencil_cache_utils.c"
  "$BLENDER/source/blender/draw/engines/gpencil/gpencil_draw_data.c"
  "$BLENDER/source/blender/draw/engines/gpencil/gpencil_engine.c"
  "$BLENDER/source/blender/draw/engines/gpencil/gpencil_render.c"
  "$BLENDER/source/blender/draw/engines/gpencil/gpencil_shader.c"
)

compile_one() {
  local src="$1"
  local base
  base="$(basename "$src")"
  local err="$OUT/$base.err"
  echo "=== Android syntax probe: $src ==="
  if [[ "$src" == *.c ]]; then
    "$CC" "${FLAGS[@]}" -std=gnu11 "$src" 2>"$err"
  else
    "$CXX" "${FLAGS[@]}" -std=gnu++17 "$src" 2>"$err"
  fi
}

run_group() {
  local name="$1"; shift
  local failed=0
  for src in "$@"; do
    if compile_one "$src"; then
      echo "PASS $src"
    else
      echo "FAIL $src"
      sed -n '1,120p' "$OUT/$(basename "$src").err"
      failed=1
      break
    fi
  done
  if (( failed )); then
    echo "$name closure is not yet Android/GLES-compatible."
    return 1
  fi
  echo "$name syntax closure passed."
}

echo "Pinned Blender Android GPU/DRW closure probe"
echo "NDK=$NDK API=$API"
echo "This is a compile-closure probe only; it does not link or replace the Project Grease renderer."

run_group "GPU core" "${GPU_SOURCES[@]}"
run_group "OpenGL backend" "${OPENGL_SOURCES[@]}"
run_group "DRW + legacy GP" "${DRW_SOURCES[@]}"

echo "Android GPU/DRW source closure probe passed."
