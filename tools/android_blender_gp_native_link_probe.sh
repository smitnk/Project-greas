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
LLVM_NM="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm"
[[ -x "$LLVM_NM" ]] || { echo "Missing Android LLVM nm: $LLVM_NM" >&2; exit 2; }

INCLUDES=(
  "$COMPAT"
  "$ROOT/native/blender_gp"
  "$BLENDER/source/blender"
  "$BLENDER/source"
  "$BLENDER/source/blender/blenlib"
  "$BLENDER/extern/wcwidth"
  "$BLENDER/extern/curve_fit_nd"
  "$BLENDER/source/blender/gpu"
  "$BLENDER/source/blender/gpu/intern"
  "$BLENDER/source/blender/draw"
  "$BLENDER/source/blender/draw/intern"
  "$BLENDER/source/blender/draw/engines/gpencil"
  "$BLENDER/source/blender/editors/include"
  "$BLENDER/source/blender/gpencil_modifiers_legacy"
  "$BLENDER/source/blender/gpencil_modifiers_legacy/intern/lineart"
  "$ROOT/native/blender_gp/lineart"
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
  "$BLENDER/intern/eigen"
  "$BLENDER/extern/Eigen3"
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
CXXFLAGS=("${COMMON_FLAGS[@]}" -std=gnu++17 -ffp-contract=off)
CFLAGS=("${COMMON_FLAGS[@]}" -std=gnu11 -ffp-contract=off)
CXXFLAGS+=(-DWITH_SSE2NEON "-I$ROOT/third_party/sse2neon")
CFLAGS+=(-DWITH_SSE2NEON "-I$ROOT/third_party/sse2neon")
for inc in "${INCLUDES[@]}"; do
  CXXFLAGS+=("-I$inc")
  CFLAGS+=("-I$inc")
done

CXX_SOURCES=()
C_SOURCES=()
declare -A SEEN_PATHS
while IFS='|' read -r lang rel; do
  [[ -z "$lang" ]] && continue
  [[ "$lang" == \#* ]] && continue
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
declare -A OBJECT_FOR_REL
idx=0
for src in "${CXX_SOURCES[@]}"; do
  obj="$OUT/$(printf '%03d' "$idx")_$(basename "$src").o"; idx=$((idx+1))
  echo "=== compile C++ $src ==="
  "$CXX" "${CXXFLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
  rel="${src#"$ROOT/"}"
  OBJECT_FOR_REL["$rel"]="$obj"
done
for src in "${C_SOURCES[@]}"; do
  obj="$OUT/$(printf '%03d' "$idx")_$(basename "$src").o"; idx=$((idx+1))
  echo "=== compile C $src ==="
  EXTRA_CFLAGS=()
  case "${src#$ROOT/}" in
    native/blender_gp/android_blender_string_compat.c|native/blender_gp/android_blender_string_utf8_compat.c)
      EXTRA_CFLAGS=(-Wno-sign-conversion -Wno-error=sign-conversion -Wno-error=implicit-int-conversion)
      ;;
  esac
  "$CC" "${CFLAGS[@]}" "${EXTRA_CFLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
  rel="${src#"$ROOT/"}"
  OBJECT_FOR_REL["$rel"]="$obj"
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
  done < <("$LLVM_NM" -g --defined-only "$obj" | awk '{if ($2 ~ /^[BCDGRST]$/ && $3 != "") print $2, $3}')
done


echo "=== regression: authoritative symbol-owner map ==="
SYMBOL_MAP="$ROOT/native/blender_gp/android_gp_symbol_owners.tsv"
[[ -f "$SYMBOL_MAP" ]] || { echo "Missing symbol-owner map: $SYMBOL_MAP" >&2; exit 2; }
while read -r sym owner provenance; do
  [[ -z "$sym" || "$sym" == \#* ]] && continue
  owner_obj="${OBJECT_FOR_REL[$owner]:-}"
  [[ -n "$owner_obj" ]] || { echo "Symbol owner is not in source manifest: $sym -> $owner" >&2; exit 1; }
  if ! "$LLVM_NM" -g --defined-only "$owner_obj" | awk -v s="$sym" '$3 == s {found=1} END {exit(found ? 0 : 1)}'; then
    echo "Mapped owner does not define symbol: $sym -> $owner ($provenance)" >&2
    exit 1
  fi
  owners=$("$LLVM_NM" -g --defined-only "${OBJECTS[@]}" 2>/dev/null | awk -v s="$sym" '$3 == s {count++} END {print count+0}')
  if [[ "$owners" -ne 1 ]]; then
    echo "Symbol ownership regression: $sym is defined by $owners objects" >&2
    exit 1
  fi
done < "$SYMBOL_MAP"

echo "=== link actual Android GP native boundary ==="
"$CXX" -shared -Wl,--no-undefined -Wl,--gc-sections \
  "${OBJECTS[@]}" \
  -lEGL -lGLESv3 -landroid -llog \
  -o "$OUT/libproject_grease_blender_gp_android.so"

echo "Android GP native library link closure passed."
