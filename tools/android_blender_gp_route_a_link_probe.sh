#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BLENDER="$ROOT/third_party/blender"
NDK="${ANDROID_NDK_ROOT:-}"
API="${ANDROID_API:-26}"

if [[ -z "$NDK" ]]; then
  echo "ANDROID_NDK_ROOT is required" >&2
  exit 2
fi

CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang++"
[[ -x "$CXX" ]] || { echo "Missing Android ARM64 C++ compiler: $CXX" >&2; exit 2; }

OUT="$ROOT/build/android-blender-gp-route-a-link-probe"
COMPAT="$ROOT/native/blender_gp/android_compat"
mkdir -p "$OUT"

INCLUDES=(
  "$COMPAT"
  "$BLENDER/source/blender"
  "$BLENDER/source"
  "$BLENDER/source/blender/blenlib"
  "$BLENDER/source/blender/gpu"
  "$BLENDER/source/blender/gpu/intern"
  "$BLENDER/source/blender/draw"
  "$BLENDER/source/blender/draw/intern"
  "$BLENDER/source/blender/draw/engines/gpencil"
  "$BLENDER/source/blender/blenkernel"
  "$BLENDER/source/blender/bmesh"
  "$BLENDER/source/blender/makesdna"
  "$BLENDER/source/blender/makesrna"
  "$BLENDER/source/blender/depsgraph"
  "$BLENDER/source/blender/windowmanager"
  "$BLENDER/source/blender/blentranslation"
  "$BLENDER/source/blender/blentranslation/intern"
  "$BLENDER/source/blender/imbuf"
  "$BLENDER/source/blender/blenloader"
  "$BLENDER/intern/guardedalloc"
  "$BLENDER/intern/atomic"
  "$BLENDER/intern/clog"
  "$ROOT/build/blender-dna/source/blender/makesdna/intern"
)

FLAGS=(
  -D__ANDROID__
  -DNDEBUG
  -DWITH_OPENGL
  -fPIC
  -std=gnu++17
  -ffunction-sections
  -fdata-sections
)

for inc in "${INCLUDES[@]}"; do
  FLAGS+=("-I$inc")
done

SOURCES=(
  "$BLENDER/source/blender/gpu/intern/gpu_vertex_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_index_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_batch.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_vertex_format.cc"
  "$BLENDER/source/blender/draw/intern/draw_cache_impl_gpencil.cc"
  "$ROOT/native/blender_gp/android_gpu_backend.cpp"
  "$ROOT/native/blender_gp/android_gpu_buffer_backend_test.cpp"
)

OBJECTS=()
for src in "${SOURCES[@]}"; do
  obj="$OUT/$(basename "$src").o"
  echo "=== compile $src ==="
  "$CXX" "${FLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done

echo "=== link Route-A Android GLES backend probe ==="
"$CXX" -shared -Wl,--no-undefined -Wl,--gc-sections \
  "${OBJECTS[@]}" \
  -lGLESv3 \
  -o "$OUT/libproject_grease_route_a_link_probe.so"

echo "Route-A Android backend link closure passed."
