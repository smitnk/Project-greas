#!/usr/bin/env bash
# L7 host tests: Project Grease's 2D modifiers against Blender 3.6.23's real
# MOD_gpencil_legacy_*.c (Shrinkwrap and Armature excluded: 2D scope).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/native_host_closure.sh" "L7 modifier parity"
GM="$BL/gpencil_modifiers_legacy"
PF=(-funsigned-char -fno-strict-aliasing -ffp-contract=off)
XI=(-I"$GM" -I"$GM/intern" -I"$BL/windowmanager" -I"$BL/editors/include" -I"$BL/blenfont" -I"$B/intern/clog"
    -I"$BL/render" -I"$BL/makesrna" -I"$BL/depsgraph" -I"$ROOT/build/blender-dna/codegen")
MODS=(noise subdiv simplify thick tint color opacity smooth offset length util)
objs=()
for m in "${MODS[@]}"; do
  o="$OUT/MOD_$m.o"; gcc "${CF[@]}" "${PF[@]}" "${XI[@]}" "${INC[@]}" -c "$GM/intern/MOD_gpencil_legacy_$m.c" -o "$o"; objs+=("$o")
done
for s in "$BL/makesdna/intern/dna_defaults.c" "$B/release/datafiles/userdef/userdef_default.c" \
         "$B/release/datafiles/userdef/userdef_default_theme.c" "$BL/blenkernel/intern/gpencil_modifier_legacy.c" "$BL/blenlib/intern/math_color.c" \
         "$ROOT/native/blender_gp/tests/l7_fixture/l7_fixture.c"; do
  o="$OUT/l7_$(basename "$s").o"; gcc "${CF[@]}" "${PF[@]}" "${XI[@]}" "${INC[@]}" -c "$s" -o "$o"; objs+=("$o")
done
g++ "${XF[@]}" "${PF[@]}" "${XI[@]}" "${INC[@]}" "$ROOT/native/blender_gp/tests/test_l7_modifier_parity.cc" "${objs[@]}" "${OBJS[@]}" \
  -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/test_l7" > "$OUT/link.txt" 2>&1 || {
  grep -oE "(undefined reference to|multiple definition of) \`[^']*'" "$OUT/link.txt" | sort | uniq -c | sort -rn | head -40; exit 1; }
timeout 300s "$OUT/test_l7"
