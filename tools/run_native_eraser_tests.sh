#!/usr/bin/env bash
# Host tests for the Blender 3.6.23 eraser core (native/blender_gp/project_grease_blender_eraser.c),
# plain and under ASan/UBSan.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_test_lib.sh"
pg_test test_eraser 1 cc -std=gnu11 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter \
  -I"$ROOT/native/blender_gp/tests/select_shim" -I"$ROOT/native/blender_gp" "$ROOT/native/blender_gp/tests/test_blender_eraser.c" "$ROOT/native/blender_gp/project_grease_blender_eraser.c"
