#!/usr/bin/env python3
"""Append test-shim declarations needed by new native tests, without replacing the repo's shims."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
SHIM = ROOT / "native/blender_gp/tests/select_shim/BKE_gpencil_geom_legacy.h"
DECLS = [
    "void BKE_gpencil_stroke_flip(bGPDstroke *gps);",
    "bGPDstroke *BKE_gpencil_stroke_duplicate(bGPDstroke *gps_src, bool dup_points, bool dup_curve);",
    "void BKE_gpencil_stroke_simplify_fixed(bGPdata *gpd, bGPDstroke *gps);",
    "bool BKE_gpencil_stroke_sample(bGPdata *gpd, bGPDstroke *gps, float dist, bool select, float sharp_threshold);",
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

# New shim headers for vertex weights (created only if the repo does not have them).
SHIM_DIR = ROOT / "native/blender_gp/tests/select_shim"
NEW_FILES = {
    "DNA_meshdata_types.h": """/* Test-only stand-in for vertex weights. */
#pragma once
typedef struct MDeformWeight { unsigned int def_nr; float weight; } MDeformWeight;
typedef struct MDeformVert { MDeformWeight *dw; int totweight; int flag; } MDeformVert;
""",
    "MEM_guardedalloc.h": """/* Test-only stand-in for guarded allocation. */
#pragma once
#include <stdlib.h>
#define MEM_freeN(p) free(p)
#define MEM_SAFE_FREE(v) do { if (v) { free(v); (v) = NULL; } } while (0)
""",
}
for name, body in NEW_FILES.items():
    path = SHIM_DIR / name
    if path.exists():
        print("kept existing shim:", name)
    else:
        path.write_text(body, encoding="utf-8")
        print("created shim:", name)

MEM = SHIM_DIR / "MEM_guardedalloc.h"
mem = MEM.read_text(encoding="utf-8")
if "MEM_callocN" not in mem:
    MEM.write_text(mem.rstrip("\n") + "\n#define MEM_callocN(size, name) calloc(1, (size))\n", encoding="utf-8")
    print("added: MEM_callocN")
