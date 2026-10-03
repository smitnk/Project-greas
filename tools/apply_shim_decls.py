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
    "bGPDframe *BKE_gpencil_frame_addnew(bGPDlayer *gpl, int cframe);",
    "bool BKE_gpencil_layer_frame_delete(bGPDlayer *gpl, bGPDframe *gpf);",
    "void BKE_gpencil_stroke_merge_distance(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps, float threshold, bool use_unselected);",
    "bGPDlayer *BKE_gpencil_layer_addnew(bGPdata *gpd, const char *name, bool setactive, bool add_to_header);",
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

dna = DNA.read_text(encoding="utf-8")
if "caps[2]" not in dna:
    anchor = "  int totpoints;\n"
    if dna.count(anchor) != 1:
        sys.exit("DNA shim: totpoints anchor not unique; add 'short caps[2];' to bGPDstroke by hand")
    DNA.write_text(dna.replace(anchor, anchor + "  short caps[2];\n"), encoding="utf-8")
    print("added: bGPDstroke.caps")

LB = SHIM_DIR / "BLI_listbase.h"
lb = LB.read_text(encoding="utf-8")
extra = []
if "BLI_addtail" not in lb: extra.append("void BLI_addtail(ListBase *listbase, void *vlink);")
if "BLI_findlink" not in lb: extra.append("void *BLI_findlink(const ListBase *listbase, int number);")
if extra:
    LB.write_text(lb.rstrip("\n") + "\n" + "\n".join(extra) + "\n", encoding="utf-8")
    print("added:", *extra)
