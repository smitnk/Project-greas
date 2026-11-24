#!/usr/bin/env bash
# CurveMapping + elastic easing: tools/run_native_l5_tests.sh (needs Blender colortools.c since L5)
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_test_lib.sh"
pg_test test_mod2 1 cc -std=gnu11 -Wall -Wno-unused-parameter -I"$ROOT/native/blender_gp/tests/select_shim" -I"$ROOT/native/blender_gp" \
  "$ROOT/native/blender_gp/tests/test_blender_mod2.c" "$ROOT/native/blender_gp/project_grease_blender_mod2.c"
