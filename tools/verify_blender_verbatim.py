#!/usr/bin/env python3
"""Fail when code marked as verbatim Blender differs from the pinned source.

Any region in Project Grease sources written as

    /* BEGIN VERBATIM <path relative to the Blender tree> */
    ...
    /* END VERBATIM */

must appear, byte for byte, as one contiguous block of that file in
third_party/blender (imported by tools/import_blender_gp.sh).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BLENDER = ROOT / "third_party" / "blender"
SCAN_DIRS = [ROOT / "native" / "blender_gp"]
SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".inc"}
BEGIN = re.compile(r"^/\* BEGIN VERBATIM (\S+) \*/$")
END = "/* END VERBATIM */"


def regions(path):
    lines = path.read_text(encoding="utf-8").split("\n")
    i = 0
    while i < len(lines):
        match = BEGIN.match(lines[i])
        if match:
            start = i
            j = i + 1
            while j < len(lines) and lines[j] != END:
                if BEGIN.match(lines[j]):
                    raise SystemExit(f"{path}:{j + 1}: nested BEGIN VERBATIM")
                j += 1
            if j == len(lines):
                raise SystemExit(f"{path}:{start + 1}: BEGIN VERBATIM without END")
            yield start + 1, match.group(1), "\n".join(lines[i + 1:j])
            i = j
        i += 1


def main():
    if not BLENDER.is_dir():
        print(f"Pinned Blender source missing: {BLENDER}", file=sys.stderr)
        return 2
    checked = 0
    failed = 0
    for directory in SCAN_DIRS:
        for path in sorted(directory.rglob("*")):
            if path.suffix not in SUFFIXES or not path.is_file():
                continue
            for line, rel, text in regions(path):
                checked += 1
                source = BLENDER / rel
                where = f"{path.relative_to(ROOT)}:{line}"
                if not source.is_file():
                    print(f"FAIL {where}: {rel} not found in pinned Blender", file=sys.stderr)
                    failed += 1
                elif not text.strip():
                    print(f"FAIL {where}: empty verbatim region", file=sys.stderr)
                    failed += 1
                elif text not in source.read_text(encoding="utf-8"):
                    print(f"FAIL {where}: region differs from {rel}", file=sys.stderr)
                    failed += 1
    if checked == 0:
        print("No verbatim regions found", file=sys.stderr)
        return 1
    print(f"Verbatim Blender regions checked: {checked}, failed: {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
