#!/usr/bin/env bash
# Builds tools/rna_ranges/pg_rna_dump against Blender 3.6.23's rna_define.c and the GP RNA definition
# files (preprocess mode, as makesrna) and writes its dump to $1 (default: stdout).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/../native_host_closure.sh" "RNA range dump"
PF=(-funsigned-char -fno-strict-aliasing -ffp-contract=off) # Blender platform_unix.cmake
RINC=(-I"$BL/makesrna/intern" -I"$B/intern/clog" -I"$BL/modifiers" -I"$BL/nodes" -I"$BL/functions" -I"$BL/simulation")
SRC=("$BL/makesrna/intern/rna_define.c" "$BL/makesrna/intern/rna_gpencil_legacy.c"
     "$BL/makesrna/intern/rna_gpencil_legacy_modifier.c" "$BL/makesrna/intern/rna_shader_fx.c"
     "$BL/makesdna/intern/dna_genfile.c" "$BL/makesdna/intern/dna_defaults.c"
     "$B/release/datafiles/userdef/userdef_default.c" "$B/release/datafiles/userdef/userdef_default_theme.c"
     "$BL/makesrna/intern/rna_rna.c" "$BL/makesrna/intern/rna_ID.c" "$BL/makesrna/intern/rna_ui_api.c" "$BL/makesrna/intern/rna_animation.c" "$BL/makesrna/intern/rna_modifier.c"
     "$BL/makesrna/intern/makesrna.c" "$BL/blenlib/intern/endian_switch.c"
     "$B/intern/clog/clog.c" "$ROOT/tools/rna_ranges/pg_rna_dump.c")
objs=()
for s in "${SRC[@]}"; do
  # makesrna.c is linked only for rna_calloc and friends; its own main() is renamed out of the way.
  extra=(); [[ "$s" == */makesrna.c ]] && extra=(-Dmain=makesrna_main_unused)
  o="$OUT/rna_$(basename "$s").o"; gcc "${CF[@]}" "${PF[@]}" "${extra[@]}" "${RINC[@]}" "${INC[@]}" -c "$s" -o "$o"; objs+=("$o")
done
# Blender's real CLOG (rna_define.c reports definition errors through it) instead of PG's silent
# CLOG stand-in in android_legacy_runtime_compat.c.
CLOSURE=(); for o in "${OBJS[@]}"; do [[ "$o" == */android_legacy_runtime_compat.c.o ]] || CLOSURE+=("$o"); done
g++ "${objs[@]}" "${CLOSURE[@]}" -Wl,--gc-sections -ldl -lpthread -lm -o "$OUT/pg_rna_dump"
if [[ $# -gt 0 ]]; then "$OUT/pg_rna_dump" > "$1"; else "$OUT/pg_rna_dump"; fi
