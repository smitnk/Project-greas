#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BLENDER="$ROOT/third_party/blender"
NDK="${ANDROID_NDK_ROOT:-}"
API="${ANDROID_API:-26}"
MANIFEST="$ROOT/tools/android_gp_source_manifest.txt"

if [[ -z "$NDK" ]]; then
  echo "ANDROID_NDK_ROOT is required" >&2
  exit 2
fi
[[ -f "$MANIFEST" ]] || { echo "Missing Android GP source manifest: $MANIFEST" >&2; exit 2; }

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
  "$BLENDER/extern/curve_fit_nd"
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
  -fvisibility=hidden
)
CXXFLAGS=("${COMMON_FLAGS[@]}" -std=gnu++17)
CFLAGS=("${COMMON_FLAGS[@]}" -std=gnu11)
for inc in "${INCLUDES[@]}"; do
  CXXFLAGS+=("-I$inc")
  CFLAGS+=("-I$inc")
done

CXX_SOURCES=()
C_SOURCES=()
declare -A SEEN_PATHS
while IFS='|' read -r lang rel; do
  [[ -z "$lang" || "$lang" == #* ]] && continue
  [[ -n "$rel" ]] || { echo "Malformed manifest line: $lang|$rel" >&2; exit 2; }
  if [[ -n "${SEEN_PATHS[$rel]:-}" ]]; then
    echo "Duplicate Android GP source manifest entry: $rel" >&2
    exit 2
  fi
  SEEN_PATHS["$rel"]=1
  src="$ROOT/$rel"
  [[ -f "$src" ]] || { echo "Manifest source missing: $rel" >&2; exit 2; }
  case "$lang" in
    CXX) CXX_SOURCES+=("$src") ;;
    C) C_SOURCES+=("$src") ;;
    *) echo "Unknown source language '$lang' for $rel" >&2; exit 2 ;;
  esac
done < "$MANIFEST"

OBJECTS=()
idx=0
for src in "${CXX_SOURCES[@]}"; do
  obj="$OUT/$(printf '%03d' "$idx")_$(basename "$src").o"; idx=$((idx+1))
  echo "=== compile C++ $src ==="
  "$CXX" "${CXXFLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done
for src in "${C_SOURCES[@]}"; do
  obj="$OUT/$(printf '%03d' "$idx")_$(basename "$src").o"; idx=$((idx+1))
  echo "=== compile C $src ==="
  "$CC" "${CFLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done

echo "=== regression: duplicate defined symbols ==="
declare -A SYMBOL_OWNER
for obj in "${OBJECTS[@]}"; do
  while read -r _type sym; do
    [[ -n "$sym" ]] || continue
    if [[ -n "${SYMBOL_OWNER[$sym]:-}" ]]; then
      echo "Duplicate defined symbol: $sym" >&2
      echo "  first: ${SYMBOL_OWNER[$sym]}" >&2
      echo "  second: $obj" >&2
      exit 1
    fi
    SYMBOL_OWNER["$sym"]="$obj"
  done < <(llvm-nm -g --defined-only "$obj" | awk '{if ($2 ~ /^[A-ZB-DG-RSTVW]$/ && $3 != "") print $2, $3}')
done

echo "=== link actual Android GP native boundary ==="
"$CXX" -shared -Wl,--no-undefined -Wl,--gc-sections \
  "${OBJECTS[@]}" \
  -lGLESv3 -landroid -llog \
  -o "$OUT/libproject_grease_blender_gp_android.so"

echo "Android GP native library link closure passed."
