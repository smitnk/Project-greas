#!/usr/bin/env bash
# Rig host tests: bone matrices, pose evaluation, F-Curve interpolation, armature modifier deform
# and bind weights of project_grease_armature*.c / .cc against Blender 3.6.23's maths.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "armature"
g++ "${XF[@]}" "${INC[@]}" "$ROOT/native/blender_gp/tests/test_armature.cc" "${OBJS[@]}" \
  -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_armature" > "$OUT/link.txt" 2>&1 || {
  grep -E "error|undefined reference|multiple definition" "$OUT/link.txt" | sort | uniq -c | sort -rn | head -40; exit 1; }
timeout 120s "$OUT/test_armature"
