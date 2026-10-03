#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
SRC=("$ROOT/native/blender_gp/tests/test_blender_mod2.c" "$ROOT/native/blender_gp/project_grease_blender_mod2.c")
INC=(-I"$ROOT/native/blender_gp/tests/select_shim" -I"$ROOT/native/blender_gp")
cc -std=gnu11 -Wall -Wno-unused-parameter "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/t" && "$OUT/t"
cc -std=gnu11 -g -fsanitize=address,undefined -w "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/a" && ASAN_OPTIONS=detect_leaks=0 "$OUT/a"
# CurveMapping port + elastic easing (batch 21 items 18 / 19)
cc -std=gnu11 -Wall -I"$ROOT/native/blender_gp" "$ROOT/native/blender_gp/tests/test_curvemap.c" "$ROOT/native/blender_gp/project_grease_curvemap.c" -lm -o "$OUT/c" && "$OUT/c"
