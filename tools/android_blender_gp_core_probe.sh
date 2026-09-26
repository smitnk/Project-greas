#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "${BASH_SOURCE[0]%/*}/.." && pwd)"
BLENDER="$ROOT/third_party/blender"
BUILD="$ROOT/build/android-blender-gp-core"
TOOLCHAIN="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"

test -f "$TOOLCHAIN"
test -f "$BLENDER/CMakeLists.txt"

rm -rf "$BUILD"
STUB="$ROOT/build/android-jpeg-config-stub"
rm -rf "$STUB"
mkdir -p "$STUB/include" "$STUB/lib"
cat > "$STUB/include/jpeglib.h" <<'EOF'
#ifndef JPEGLIB_H
#define JPEGLIB_H
typedef struct jpeg_error_mgr jpeg_error_mgr;
typedef struct jpeg_compress_struct jpeg_compress_struct;
typedef struct jpeg_decompress_struct jpeg_decompress_struct;
typedef jpeg_error_mgr *jpeg_error_ptr;
typedef jpeg_compress_struct *j_compress_ptr;
typedef jpeg_decompress_struct *j_decompress_ptr;
#endif
EOF
cat > "$STUB/empty.c" <<'EOF'
void project_grease_android_jpeg_probe_stub(void) {}
EOF
"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" \
  --target=aarch64-linux-android26 -c "$STUB/empty.c" -o "$STUB/empty.o"
"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar" \
  rcs "$STUB/lib/libjpeg.a" "$STUB/empty.o"

echo "=== Android toolchain ==="
echo "$TOOLCHAIN"
"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++" --version | head -n 1

echo "=== Configure Blender core for Android ARM64 ==="
cmake -S "$BLENDER" -B "$BUILD" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_ANDROID_NDK="$ANDROID_NDK_ROOT" \
  -C "$BLENDER/build_files/cmake/config/blender_lite.cmake" \
  -DWITH_PYTHON=OFF \
  -DWITH_GHOST=OFF \
  -DWITH_HEADLESS=ON \
  -DWITH_X11=OFF \
  -DWITH_WAYLAND=OFF \
  -DWITH_SDL=OFF \
  -DWITH_OPENGL=OFF \
  -DWITH_VULKAN_BACKEND=OFF \
  -DWITH_AUDASPACE=OFF \
  -DWITH_SYSTEM_AUDASPACE=OFF \
  -DWITH_OPENAL=OFF \
  -DWITH_OPENCOLORIO=OFF \
  -DWITH_OPENIMAGEIO=OFF \
  -DWITH_IMAGE_TIFF=OFF \
  -DWITH_OPENSUBDIV=OFF \
  -DWITH_TBB=OFF \
  -DWITH_OPENMP=OFF \
  -DWITH_LIBS_PRECOMPILED=OFF \
  -DWITH_SYSTEM_FREETYPE=OFF \
  -DWITH_GTESTS=OFF \
  -DJPEG_LIBRARY="$STUB/lib/libjpeg.a" \
  -DJPEG_INCLUDE_DIR="$STUB/include"

echo "=== Verify Android target configuration ==="
grep -E 'CMAKE_SYSTEM_NAME:|CMAKE_ANDROID_ARCH_ABI:|CMAKE_ANDROID_API:|CMAKE_CXX_COMPILER:' \
  "$BUILD/CMakeCache.txt" || true

echo "=== Build only Blender's BlenKernel target ==="
cmake --build "$BUILD" --target bf_blenkernel --parallel 2

echo "=== Android GP core probe passed ==="
