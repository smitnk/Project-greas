#!/usr/bin/env bash
# Host-side unit tests for the Blender 3.6.23 primitive geometry
# (native/blender_gp/project_grease_blender_primitive.c).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
cc -std=gnu11 -Wall \
  -I"$ROOT/native/blender_gp/tests/shim" \
  -I"$ROOT/native/blender_gp" \
  "$ROOT/native/blender_gp/tests/test_blender_primitive.c" \
  "$ROOT/native/blender_gp/project_grease_blender_primitive.c" \
  -lm -o "$OUT/test_blender_primitive"
"$OUT/test_blender_primitive"
