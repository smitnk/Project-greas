#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "${BASH_SOURCE[0]%/*}/.." && pwd)"
BLENDER="$ROOT/third_party/blender"
BUILD="$ROOT/build/android-gp-minimal"
NDK="${ANDROID_NDK_ROOT:?ANDROID_NDK_ROOT must be set}"
CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++"
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang"

test -x "$CXX"
test -f "$BLENDER/source/blender/blenkernel/intern/gpencil_geom_legacy.cc"

rm -rf "$BUILD"
mkdir -p "$BUILD"

COMMON=(
  -D__ANDROID__
  -DNDEBUG
  -fPIC
  -I"$BLENDER/source/blender"
  -I"$BLENDER/source"
  -I"$BLENDER/source/blender/blenkernel"
  -I"$BLENDER/source/blender/blenlib"
  -I"$BLENDER/source/blender/bmesh"
  -I"$BLENDER/source/blender/makesdna"
  -I"$BLENDER/source/blender/makesrna"
  -I"$BLENDER/source/blender/depsgraph"
  -I"$BLENDER/source/blender/editors/include"
  -I"$BLENDER/source/blender/windowmanager"
  -I"$BLENDER/intern/guardedalloc"
  -I"$BLENDER/intern/atomic"
  -I"$BLENDER/intern/clog"
  -I"$BLENDER/source/blender/blentranslation"
  -I"$BLENDER/source/blender/blentranslation/intern"
)

echo "=== Minimal Android legacy GP source compile probe ==="
echo "This intentionally bypasses Blender's desktop platform dependency discovery."
echo "Target: arm64-v8a / API 26"

set +e
"$CXX" -std=gnu++17 "${COMMON[@]}" -c   "$BLENDER/source/blender/blenkernel/intern/gpencil_geom_legacy.cc"   -o "$BUILD/gpencil_geom_legacy.o"   2>"$BUILD/gpencil_geom_legacy.err"
GEOM_RC=$?

"$CC" -std=gnu11 "${COMMON[@]}" -c   "$BLENDER/source/blender/blenkernel/intern/gpencil_legacy.c"   -o "$BUILD/gpencil_legacy.o"   2>"$BUILD/gpencil_legacy.err"
LEGACY_RC=$?
set -e

for f in "$BUILD"/*.err; do
  echo "----- ${f##*/} -----"
  cat "$f"
done

if [[ $GEOM_RC -ne 0 || $LEGACY_RC -ne 0 ]]; then
  echo "=== Minimal GP probe found real source-level Android boundaries ==="
  echo "gpencil_geom_legacy.cc rc=$GEOM_RC"
  echo "gpencil_legacy.c rc=$LEGACY_RC"
  exit 1
fi

echo "=== Minimal Android legacy GP sources compile successfully ==="
