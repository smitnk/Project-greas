#!/usr/bin/env bash
# Host tests for the annotation data (native/blender_gp/project_grease_annotations.c), plain and
# under ASan/UBSan (with leak detection).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_test_lib.sh"
ASAN_OPTIONS=detect_leaks=1 pg_test test_annotations 1 cc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -I"$ROOT/native/blender_gp/tests/select_shim" -I"$ROOT/native/blender_gp" \
  "$ROOT/native/blender_gp/tests/test_annotations.c" "$ROOT/native/blender_gp/project_grease_annotations.c"
