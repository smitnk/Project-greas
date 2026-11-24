#!/usr/bin/env bash
# Host-side unit tests for the Blender 3.6.23 primitive geometry
# (native/blender_gp/project_grease_blender_primitive.c).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_test_lib.sh"
pg_test test_blender_primitive 0 cc -std=gnu11 -Wall -I"$ROOT/native/blender_gp/tests/shim" -I"$ROOT/native/blender_gp" \
  "$ROOT/native/blender_gp/tests/test_blender_primitive.c" "$ROOT/native/blender_gp/project_grease_blender_primitive.c"
