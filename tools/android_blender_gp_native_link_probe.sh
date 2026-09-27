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
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang"
[[ -x "$CXX" ]] || { echo "Missing Android ARM64 C++ compiler: $CXX" >&2; exit 2; }
[[ -x "$CC" ]] || { echo "Missing Android ARM64 C compiler: $CC" >&2; exit 2; }

OUT="$ROOT/build/android-blender-gp-native-link-probe"
COMPAT="$ROOT/native/blender_gp/android_compat"
mkdir -p "$OUT"

INCLUDES=(
  "$COMPAT"
  "$ROOT/native/blender_gp"
  "$BLENDER/source/blender"
  "$BLENDER/source"
  "$BLENDER/source/blender/blenlib"
  "$BLENDER/source/blender/gpu"
  "$BLENDER/source/blender/gpu/intern"
  "$BLENDER/source/blender/draw"
  "$BLENDER/source/blender/draw/intern"
  "$BLENDER/source/blender/draw/engines/gpencil"
  "$BLENDER/source/blender/editors/include"
  "$BLENDER/source/blender/blenkernel"
  "$BLENDER/source/blender/bmesh"
  "$BLENDER/source/blender/makesdna"
  "$BLENDER/source/blender/makesrna"
  "$BLENDER/source/blender/render"
  "$BLENDER/source/blender/depsgraph"
  "$BLENDER/source/blender/windowmanager"
  "$BLENDER/source/blender/blentranslation"
  "$BLENDER/source/blender/blentranslation/intern"
  "$BLENDER/source/blender/imbuf"
  "$BLENDER/source/blender/blenloader"
  "$BLENDER/intern/guardedalloc"
  "$BLENDER/intern/atomic"
  "$BLENDER/intern/clog"
  "$ROOT/build/blender-dna"
)

COMMON_FLAGS=(
  -D__ANDROID__
  -DNDEBUG
  -DWITH_OPENGL
  -fPIC
  -ffunction-sections
  -fdata-sections
)

CXXFLAGS=("${COMMON_FLAGS[@]}" -std=gnu++17)
CFLAGS=("${COMMON_FLAGS[@]}" -std=gnu11)

for inc in "${INCLUDES[@]}"; do
  CXXFLAGS+=("-I$inc")
  CFLAGS+=("-I$inc")
done

CXX_SOURCES=(
  "$ROOT/native/blender_gp/project_grease_gp_backend.cpp"
  "$ROOT/native/blender_gp/project_grease_gp_bridge.cpp"
  "$ROOT/native/blender_gp/project_grease_gp_jni.cpp"
  "$BLENDER/source/blender/draw/intern/draw_cache_impl_gpencil.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_vertex_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_index_buffer.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_batch.cc"
  "$BLENDER/source/blender/gpu/intern/gpu_vertex_format.cc"
  "$ROOT/native/blender_gp/android_gpu_backend.cpp"
  "$BLENDER/source/blender/blenkernel/intern/gpencil_geom_legacy.cc"
)

C_SOURCES=(
  "$BLENDER/source/blender/blenkernel/intern/gpencil_legacy.c"
)

OBJECTS=()
for src in "${CXX_SOURCES[@]}"; do
  obj="$OUT/$(basename "$src").o"
  echo "=== compile C++ $src ==="
  "$CXX" "${CXXFLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done

for src in "${C_SOURCES[@]}"; do
  obj="$OUT/$(basename "$src").o"
  echo "=== compile C $src ==="
  "$CC" "${CFLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done

echo "=== link actual Android GP native boundary ==="
"$CXX" -shared -Wl,--no-undefined -Wl,--gc-sections \
  "${OBJECTS[@]}" \
  -lGLESv3 -landroid -llog \
  -o "$OUT/libproject_grease_blender_gp_android.so"

echo "Android GP native library link closure passed."
