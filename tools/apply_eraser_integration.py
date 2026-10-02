#!/usr/bin/env python3
"""Register the eraser core in the native source manifest and CI (idempotent, exact anchors)."""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EDITS = [
    (
        ROOT / "tools/android_gp_source_manifest.txt",
        "project_grease_blender_eraser.c",
        "C|native/blender_gp/project_grease_blender_select.c\n",
        "C|native/blender_gp/project_grease_blender_select.c\n"
        "C|native/blender_gp/project_grease_blender_eraser.c\n",
    ),
    (
        ROOT / ".github/workflows/android-shell.yml",
        "run_native_eraser_tests.sh",
        "      - name: Run native selection tests\n"
        "        run: bash tools/run_native_select_tests.sh\n",
        "      - name: Run native selection tests\n"
        "        run: bash tools/run_native_select_tests.sh\n\n"
        "      - name: Run native eraser tests\n"
        "        run: bash tools/run_native_eraser_tests.sh\n",
    ),
]


def main():
    pending = []
    for path, marker, old, new in EDITS:
        text = path.read_text(encoding="utf-8")
        if marker in text:
            print(f"already applied: {path.relative_to(ROOT)}")
            continue
        if text.count(old) != 1:
            print(f"anchor in {path.relative_to(ROOT)} found {text.count(old)} times (expected 1); nothing changed",
                  file=sys.stderr)
            return 1
        pending.append((path, text.replace(old, new)))
    for path, text in pending:
        path.write_text(text, encoding="utf-8")
        print(f"patched: {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
