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
[[ -f "$BLENDER/source/blender/draw/intern/draw_cache_impl_gpencil.cc" ]] || {
  echo "Pinned Blender GP cache source is missing" >&2
  exit 2
}

OUT="$ROOT/build/android-blender-gp-route-a-probe"
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
  "$BLENDER/source/blender/draw/engines/gpencil"
  "$BLENDER/source/blender/gpu"
  "$BLENDER/source/blender/gpu/intern"
  "$BLENDER/source/blender/render"
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
  -fPIC
  -fsyntax-only
  -Wno-unused-command-line-argument
)

for inc in "${INCLUDES[@]}"; do
  FLAGS+=("-I$inc")
done

# Route A intentionally excludes Blender's desktop GPU backend.
# Do not add gpu_context.cc, gl_backend.cc, gl_context.cc, gl_texture.cc,
# GLFramebuffer, GLShader, GLStorageBuf, GLQuery, or full DRW engine sources.
GPU_SOURCES=(
  "$BLENDER/source/blender/gpu/intern/gpu_batch.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_index_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_vertex_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_vertex_format.cc"
)

# This is the actual legacy GP cache producer used by Project Grease.
# It creates GPUVertBuf/GPUIndexBuf/GPUBatch and packs the GP stroke data.
GP_CACHE_SOURCES=(
  "$BLENDER/source/blender/draw/intern/draw_cache_impl_gpencil.cc"
)

compile_one() {
  local src="$1"
  local base
  base="$(basename "$src")"
  local err="$OUT/$base.err"
  echo "=== Android Route-A syntax probe: $src ==="
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
      sed -n '1,160p' "$OUT/$(basename "$src").err"
      failed=1
      break
    fi
  done
  if (( failed )); then
    echo "$name Route-A closure is not yet Android/GLES-compatible."
    return 1
  fi
  echo "$name Route-A syntax closure passed."
}

echo "Pinned Blender 3.6.23 Android Route-A GP closure probe"
echo "NDK=$NDK API=$API"
echo "This probe intentionally does NOT compile Blender's stock desktop GL backend."
echo "Target: legacy GP cache -> GPUVertBuf/GPUIndexBuf/GPUBatch -> Project Grease Android backend."

run_group "GPU buffer API" "${GPU_SOURCES[@]}"
run_group "Legacy GP cache" "${GP_CACHE_SOURCES[@]}"

echo "Android Route-A GP source closure probe passed."
