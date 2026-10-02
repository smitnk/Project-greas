#!/usr/bin/env bash
# Host tests for Scene-lite (native/blender_gp/project_grease_scene_lite.c: OBJ meshes and the
# Line Art camera), plain and under ASan/UBSan with leak detection.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=("$ROOT/native/blender_gp/tests/test_scene_lite.c" "$ROOT/native/blender_gp/project_grease_scene_lite.c")
INC=(-I"$ROOT/native/blender_gp")
cc -std=gnu11 -Wall -Wextra "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/test_scene_lite"
"$OUT/test_scene_lite"
cc -std=gnu11 -g -fsanitize=address,undefined -fno-omit-frame-pointer "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/test_scene_lite_asan"
"$OUT/test_scene_lite_asan"
