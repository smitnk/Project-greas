#!/usr/bin/env bash
# Host tests of the shader-effect data model, pass construction and the CPU port of
# gpencil_vfx_frag.glsl (native/blender_gp/project_grease_shader_fx.c). Links the real pinned
# Blender math (matrix/vector) code, see tools/native_host_closure.sh.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "shader fx test"
g++ "${XF[@]}" -Wall "${INC[@]}" "$ROOT/native/blender_gp/tests/test_shader_fx.cc" "${OBJS[@]}" \
  -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_shader_fx"
"$OUT/test_shader_fx"
