#!/usr/bin/env bash
# Host tests for the annotation data (native/blender_gp/project_grease_annotations.c), plain and
# under ASan/UBSan (with leak detection).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=("$ROOT/native/blender_gp/tests/test_annotations.c" "$ROOT/native/blender_gp/project_grease_annotations.c")
INC=(-I"$ROOT/native/blender_gp/tests/select_shim" -I"$ROOT/native/blender_gp")
cc -std=gnu11 -Wall -Wextra -Wno-unused-parameter "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/test_annotations"
"$OUT/test_annotations"
cc -std=gnu11 -g -fsanitize=address,undefined -fno-omit-frame-pointer "${INC[@]}" "${SRC[@]}" -lm -o "$OUT/test_annotations_asan"
"$OUT/test_annotations_asan"
