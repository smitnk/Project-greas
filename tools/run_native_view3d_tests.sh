#!/usr/bin/env bash
# 3D viewport maths (native/blender_gp/project_grease_view3d.c) against Blender 3.6.23's BLI math:
# view / window matrices, turntable orbit, pan, zoom, axis views and the GP drawing planes.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_test_lib.sh"
INC=(-I"$ROOT/native/blender_gp/android_compat" -I"$ROOT/native/blender_gp" -I"$BL/blenlib" -I"$BL/makesdna"
     -I"$B/intern/guardedalloc" -I"$B/intern/atomic")
SRC=("$ROOT/native/blender_gp/project_grease_view3d.c"
     "$BL/blenlib/intern/math_geom.c" "$BL/blenlib/intern/math_base.c"
     "$ROOT/native/blender_gp/android_blender_math_matrix_compat.c" "$ROOT/native/blender_gp/android_blender_math_vector_compat.c"
     "$ROOT/native/blender_gp/android_blender_math_rotation_compat.c" "$BL/blenlib/intern/BLI_assert.c")
for SAN in "" "-g -fsanitize=address,undefined"; do
  rm -f "$OUT"/*.o
  for f in "${SRC[@]}"; do cc -c -std=gnu11 -DNDEBUG -DMATH_STANDALONE -w -ffunction-sections $SAN "${INC[@]}" "$f" -o "$OUT/$(basename "$f").o"; done
  c++ -c -std=gnu++17 -DNDEBUG -DMATH_STANDALONE -w $SAN "${INC[@]}" "$ROOT/native/blender_gp/tests/test_view3d.cc" -o "$OUT/test.o"
  c++ $SAN "$OUT"/*.o -Wl,--gc-sections -lm -o "$OUT/t"
  ASAN_OPTIONS=detect_leaks=0 "$OUT/t"
done
