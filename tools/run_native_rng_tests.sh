#!/usr/bin/env bash
# Host test for the Blender RNG/hash closure (rand.cc, noise.c, BLI_hash.h) used by the Offset and
# Noise modifiers. Compiles the real pinned sources, so tools/import_blender_gp.sh must have run;
# set PG_REQUIRE_BLENDER_SOURCE=1 (CI) to fail instead of skip when it has not.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_test_lib.sh"
pg_require_file "RNG test" "$BL/blenlib/intern/rand.cc"
INC=(-I"$ROOT/native/blender_gp/android_compat" -I"$ROOT/native/blender_gp" -I"$BL" -I"$BL/blenlib"
     -I"$BL/makesdna" -I"$B/intern/guardedalloc" -I"$B/intern/atomic")
gcc -std=gnu11 -c -DNDEBUG "${INC[@]}" "$BL/blenlib/intern/noise.c" -o "$OUT/noise.o"
g++ -std=gnu++17 -c -DNDEBUG "${INC[@]}" "$BL/blenlib/intern/rand.cc" -o "$OUT/rand.o"
pg_test test_rng 0 g++ -std=gnu++17 -Wall -DNDEBUG "${INC[@]}" "$ROOT/native/blender_gp/tests/test_blender_rng.cc" "$OUT/rand.o" "$OUT/noise.o"
