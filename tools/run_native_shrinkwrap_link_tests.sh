#!/usr/bin/env bash
# Force-link the real Blender 3.6.23 Legacy GP Shrinkwrap entry point.
# This is intentionally separate from the generic modifier-stack closure:
# Shrinkwrap requires the evaluated Mesh/BVH closure and must not be hidden by
# --gc-sections when no current runtime fixture calls it.
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "shrinkwrap link test"

SW_CXX=(
  "$BL/blenkernel/intern/shrinkwrap.cc"
  "$BL/blenkernel/intern/bvhutils.cc"
  "$BL/blenlib/intern/generic_virtual_array.cc"
)
SW_C=(
  "$BL/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_shrinkwrap.c"
)

SWOBJS=()
for s in "${SW_CXX[@]}"; do
  o="$OUT/sw_$(basename "$s").o"
  g++ "${XF[@]}" "${INC[@]}" -c "$s" -o "$o"
  SWOBJS+=("$o")
done
for s in "${SW_C[@]}"; do
  o="$OUT/sw_$(basename "$s").o"
  gcc "${CF[@]}" "${INC[@]}" -c "$s" -o "$o"
  SWOBJS+=("$o")
done

g++ "${XF[@]}" -Wall "${INC[@]}"   "$ROOT/native/blender_gp/tests/test_shrinkwrap_link.cc"   "${SWOBJS[@]}" "${OBJS[@]}"   -Wl,--gc-sections -ldl -lpthread -lm   -o "$OUT/test_shrinkwrap_link"

"$OUT/test_shrinkwrap_link"
echo "Shrinkwrap Legacy GP link reachability: PASS"
