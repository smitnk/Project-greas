#!/usr/bin/env python3
"""Append test-shim declarations needed by new native tests, without replacing the repo's shims."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
SHIM = ROOT / "native/blender_gp/tests/select_shim/BKE_gpencil_geom_legacy.h"
DECLS = ["void BKE_gpencil_stroke_flip(bGPDstroke *gps);"]
text = SHIM.read_text(encoding="utf-8")
added = [d for d in DECLS if d.split("(")[0].split()[-1] not in text]
if added:
    SHIM.write_text(text.rstrip("\n") + "\n" + "\n".join(added) + "\n", encoding="utf-8")
print("added:" if added else "already present", *added)
