#!/usr/bin/env python3
"""Compares a cell's screenshots with the stored goldens (android/app/src/androidTest/goldens/<cell>/).

A pixel differs when any channel differs by more than CHANNEL_TOL; a screenshot fails when more than
FRACTION_TOL of its pixels differ (antialiasing and emulator GPU noise stay under it). For each
failure a diff image (differing pixels in magenta over a faded screenshot) is written. Screenshots
without a golden are copied to <out>/candidates/<cell>/ so they can be reviewed and committed as the
cell's goldens. usage: golden_compare.py <screenshots dir> <goldens dir> <out dir> <cell>
"""
import os
import shutil
import sys

import numpy as np  # the bug-hunt workflow installs numpy and Pillow
from PIL import Image

CHANNEL_TOL = int(os.environ.get("GOLDEN_CHANNEL_TOL", "24"))
FRACTION_TOL = float(os.environ.get("GOLDEN_FRACTION_TOL", "0.01"))


def compare(shot, golden, diff_path):
    a = np.asarray(Image.open(shot).convert("RGB"), dtype=np.int16)
    b = np.asarray(Image.open(golden).convert("RGB"), dtype=np.int16)
    if a.shape != b.shape:
        return 1.0, f"size {a.shape[1]}x{a.shape[0]} vs golden {b.shape[1]}x{b.shape[0]}"
    differs = np.abs(a - b).max(axis=2) > CHANNEL_TOL
    frac = float(differs.mean())
    if frac > FRACTION_TOL:
        diff = (128 + a // 2).astype(np.uint8)
        diff[differs] = (255, 0, 255)
        Image.fromarray(diff).save(diff_path)
    return frac, ""


def main():
    shots, goldens, out, cell = sys.argv[1:5]
    gdir = os.path.join(goldens, cell)
    os.makedirs(os.path.join(out, "diffs", cell), exist_ok=True)
    lines, failed, new = [], 0, 0
    for name in sorted(os.listdir(shots)) if os.path.isdir(shots) else []:
        if not name.endswith(".png"):
            continue
        golden = os.path.join(gdir, name)
        if not os.path.exists(golden):
            os.makedirs(os.path.join(out, "candidates", cell), exist_ok=True)
            shutil.copy(os.path.join(shots, name), os.path.join(out, "candidates", cell, name))
            new += 1
            continue
        try:
            frac, why = compare(os.path.join(shots, name), golden, os.path.join(out, "diffs", cell, name))
        except Exception as e:  # unreadable image is a failure, not a crash of the comparator
            frac, why = 1.0, str(e)
        if frac > FRACTION_TOL:
            failed += 1
            lines.append(f"GOLDEN FAIL {cell}/{name}: {frac * 100:.2f}% pixels differ {why}")
    lines.insert(0, f"== goldens {cell}: {failed} differ, {new} without a golden (candidates written) ==")
    print("\n".join(lines))
    with open(os.path.join(out, f"goldens_{cell}.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
