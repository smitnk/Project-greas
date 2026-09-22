#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/android/apply_project_grease_wiring.sh"
"$ROOT/android/complete_project_grease_wiring.sh"

UPSTREAM="$ROOT/vendor/blender_android_upstream"
ACTIVITY="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender/BlenderActivity.java"
VIEW="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender/ProjectGreaseOverlayView.java"
MANIFEST="$UPSTREAM/build_files/android/apk/app/src/main/AndroidManifest.xml"
GHOST_H="$UPSTREAM/intern/ghost/intern/GHOST_SystemAndroid.hh"
GHOST_CC="$UPSTREAM/intern/ghost/intern/GHOST_SystemAndroid.cc"
GHOST_MAIN="$UPSTREAM/intern/ghost/intern/GHOST_AndroidMain.cc"
STARTUP="$UPSTREAM/build_files/android/apk/app/src/main/assets/project_grease_startup.py"

required=(
  "$ACTIVITY"
  "$VIEW"
  "$MANIFEST"
  "$GHOST_H"
  "$GHOST_CC"
  "$GHOST_MAIN"
  "$STARTUP"
  "$UPSTREAM/build_files/android/build.py"
)

for f in "${required[@]}"; do
  test -f "$f" || { echo "MISSING: $f" >&2; exit 1; }
done

grep -q 'installProjectGreaseOverlay' "$ACTIVITY"
grep -q 'nativeProjectGreaseTouch' "$ACTIVITY"
grep -q 'addContentView' "$ACTIVITY"
grep -q 'package org.blender.blender;' "$VIEW"
! grep -q 'ProjectGreaseOverlayActivity' "$MANIFEST"
grep -q 'handleProjectGreaseTouch' "$GHOST_H"
grep -q 'dispatchProjectGreaseTouch' "$GHOST_CC"
grep -q 'Java_org_blender_blender_BlenderActivity_nativeProjectGreaseTouch' "$GHOST_MAIN"
grep -q 'grease_pencil_add' "$STARTUP"
grep -q 'PAINT_GREASE_PENCIL' "$STARTUP"

python3 -m py_compile "$UPSTREAM/build_files/android/build.py"

if git -C "$UPSTREAM" diff --check; then
  echo "Blender submodule diff check: OK"
fi

echo "Project Grease wiring verification: PASS"
echo "Next step: ./android/build_project_grease_lite.sh lite"
