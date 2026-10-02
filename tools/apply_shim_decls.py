#!/usr/bin/env python3
"""Append test-shim declarations needed by new native tests, without replacing the repo's shims."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
SHIM = ROOT / "native/blender_gp/tests/select_shim/BKE_gpencil_geom_legacy.h"
DECLS = [
    "void BKE_gpencil_stroke_flip(bGPDstroke *gps);",
    "bGPDstroke *BKE_gpencil_stroke_duplicate(bGPDstroke *gps_src, bool dup_points, bool dup_curve);",
    "void BKE_gpencil_stroke_join(bGPDstroke *gps_a, bGPDstroke *gps_b, bool leave_gaps, bool fit_thickness, bool smooth, bool auto_flip);",
]
text = SHIM.read_text(encoding="utf-8")
added = [d for d in DECLS if d.split("(")[0].split()[-1] not in text]
if added:
    SHIM.write_text(text.rstrip("\n") + "\n" + "\n".join(added) + "\n", encoding="utf-8")
print("added:" if added else "already present", *added)

# Struct field used by the dissolve operator (real DNA has `struct MDeformVert *dvert`).
DNA = ROOT / "native/blender_gp/tests/select_shim/DNA_gpencil_legacy_types.h"
dna = DNA.read_text(encoding="utf-8")
if "*dvert;" not in dna:
    anchor = "  int totpoints;\n"
    if dna.count(anchor) != 1:
        sys.exit("DNA shim: totpoints anchor not unique; add 'struct MDeformVert *dvert;' to bGPDstroke by hand")
    dna = dna.replace(anchor, anchor + "  struct MDeformVert *dvert;\n")
    DNA.write_text(dna, encoding="utf-8")
    print("added: bGPDstroke.dvert")
