#!/usr/bin/env bash
# Host tests for Scene-lite (native/blender_gp/project_grease_scene_lite.c: OBJ meshes and the
# Line Art camera), plain and under ASan/UBSan with leak detection.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_test_lib.sh"
ASAN_OPTIONS=detect_leaks=1 pg_test test_scene_lite 1 cc -std=gnu11 -Wall -Wextra -I"$ROOT/native/blender_gp" \
  "$ROOT/native/blender_gp/tests/test_scene_lite.c" "$ROOT/native/blender_gp/project_grease_scene_lite.c"
