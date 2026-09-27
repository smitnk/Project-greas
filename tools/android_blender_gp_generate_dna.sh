#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "\${BASH_SOURCE[0]}")/.." && pwd)"
BLENDER="$ROOT/third_party/blender"
OUT="$ROOT/build/blender-dna"
GEN="$OUT/codegen"
OBJ="$OUT/obj"

mkdir -p "$GEN" "$OBJ"

if [[ ! -f "$BLENDER/source/blender/CMakeLists.txt" ]]; then
  echo "Pinned Blender source is missing: $BLENDER" >&2
  exit 2
fi

# Reproduce Blender 3.6.23's SRC_DNA_INC list without configuring Blender's
# top-level project. The top-level Unix platform CMake unconditionally probes
# unrelated desktop libraries; makesdna itself only needs the DNA headers plus
# the small blenlib/guardedalloc closure.
mapfile -t DNA_HEADERS < <(
  sed -n '/^set(SRC_DNA_INC$/,/^)/p' "$BLENDER/source/blender/CMakeLists.txt" |
    sed -n 's#.*\(/DNA_[A-Za-z0-9_]*\.h\).*#\1#p'
)

if (( \${#DNA_HEADERS[@]} == 0 )); then
  echo "Could not recover SRC_DNA_INC from pinned Blender source." >&2
  exit 2
fi

{
  echo '/* Generated from Blender 3.6.23 source/blender/CMakeLists.txt. */'
  for header in "\${DNA_HEADERS[@]}"; do
    printf '#include "%s"\\n' "$BLENDER/source/blender\${header}"
  done
} > "$GEN/dna_includes_all.h"

{
  echo '/* Generated from Blender 3.6.23 source/blender/CMakeLists.txt. */'
  for header in "\${DNA_HEADERS[@]}"; do
    printf '  "%s",\\n' "\${header##*/}"
  done
} > "$GEN/dna_includes_as_strings.h"

CFLAGS=(
  -DNDEBUG
  -DWITH_DNA_GHASH
  -std=gnu11
  -ffunction-sections
  -fdata-sections
  -I"$GEN"
  -I"$BLENDER/source/blender/makesdna"
  -I"$BLENDER/source/blender/blenlib"
  -I"$BLENDER/source"
  -I"$BLENDER/intern/atomic"
  -I"$BLENDER/intern/guardedalloc"
)

CXXFLAGS=(
  -DNDEBUG
  -DWITH_DNA_GHASH
  -std=gnu++17
  -ffunction-sections
  -fdata-sections
  -I"$GEN"
  -I"$BLENDER/source/blender/makesdna"
  -I"$BLENDER/source/blender/blenlib"
  -I"$BLENDER/source"
  -I"$BLENDER/intern/atomic"
  -I"$BLENDER/intern/guardedalloc"
)

CC="\${CC:-cc}"
CXX="\${CXX:-c++}"

C_SOURCES=(
  "$BLENDER/source/blender/makesdna/intern/makesdna.c"
  "$BLENDER/source/blender/makesdna/intern/dna_utils.c"
  "$BLENDER/source/blender/blenlib/intern/BLI_assert.c"
  "$BLENDER/source/blender/blenlib/intern/BLI_ghash.c"
  "$BLENDER/source/blender/blenlib/intern/BLI_ghash_utils.c"
  "$BLENDER/source/blender/blenlib/intern/BLI_memarena.c"
  "$BLENDER/source/blender/blenlib/intern/BLI_mempool.c"
  "$BLENDER/source/blender/blenlib/intern/hash_mm2a.c"
  "$BLENDER/intern/guardedalloc/intern/mallocn.c"
  "$BLENDER/intern/guardedalloc/intern/mallocn_guarded_impl.c"
  "$BLENDER/intern/guardedalloc/intern/mallocn_lockfree_impl.c"
)

CXX_SOURCES=(
  "$BLENDER/intern/guardedalloc/intern/leak_detector.cc"
  "$BLENDER/intern/guardedalloc/intern/memory_usage.cc"
)

OBJECTS=()

for src in "\${C_SOURCES[@]}"; do
  obj="$OBJ/$(basename "$src").o"
  echo "=== host C compile: $src ==="
  "$CC" "\${CFLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done

for src in "\${CXX_SOURCES[@]}"; do
  obj="$OBJ/$(basename "$src").o"
  echo "=== host C++ compile: $src ==="
  "$CXX" "\${CXXFLAGS[@]}" -c "$src" -o "$obj"
  OBJECTS+=("$obj")
done

echo "=== host link: minimal makesdna ==="
"$CXX" -Wl,--gc-sections "\${OBJECTS[@]}" -ldl -lpthread -lm -o "$OUT/makesdna"

echo "=== generate Blender DNA metadata ==="
"$OUT/makesdna" \
  "$OUT/dna.c" \
  "$OUT/dna_type_offsets.h" \
  "$OUT/dna_verify.c" \
  "$BLENDER/source/blender/makesdna/"

echo "Minimal Blender DNA generation passed without configuring the full Blender project."
