#!/usr/bin/env bash
# Lasso warp: Blender 3.6.23 lattice deform (verbatim) on canvas points.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$ROOT/third_party/blender"; BL="$B/source/blender"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
INC=(-I"$ROOT/native/blender_gp/android_compat" -I"$ROOT/native/blender_gp" -I"$BL/blenlib" -I"$BL/blenkernel"
     -I"$BL/makesdna" -I"$B/intern/guardedalloc" -I"$B/intern/atomic")
SRC=("$ROOT/native/blender_gp/tests/test_blender_lattice.c" "$ROOT/native/blender_gp/project_grease_blender_lattice.c"
     "$ROOT/native/blender_gp/project_grease_key_extract.c"
     "$ROOT/native/blender_gp/android_blender_math_matrix_compat.c" "$ROOT/native/blender_gp/android_blender_math_vector_compat.c" "$ROOT/native/blender_gp/android_blender_math_rotation_compat.c"
     "$B/intern/guardedalloc/intern/mallocn.c" "$B/intern/guardedalloc/intern/mallocn_guarded_impl.c"
     "$B/intern/guardedalloc/intern/mallocn_lockfree_impl.c" "$BL/blenlib/intern/BLI_assert.c")
for SAN in "" "-g -fsanitize=address,undefined"; do
  rm -f "$OUT"/*.o
  for f in "${SRC[@]}"; do cc -c -std=gnu11 -DNDEBUG -DMATH_STANDALONE -DMALLOC_USABLE_SIZE_DISABLED -w -ffunction-sections $SAN "${INC[@]}" "$f" -o "$OUT/$(basename "$f").o"; done
  c++ -c -std=gnu++17 -DNDEBUG -w $SAN "${INC[@]}" "$B/intern/guardedalloc/intern/leak_detector.cc" -o "$OUT/leak.o"
  c++ -c -std=gnu++17 -DNDEBUG -w $SAN "${INC[@]}" "$B/intern/guardedalloc/intern/memory_usage.cc" -o "$OUT/usage.o"
  c++ $SAN "$OUT"/*.o -Wl,--gc-sections -lm -lpthread -o "$OUT/t"
  ASAN_OPTIONS=detect_leaks=0 "$OUT/t"
done
