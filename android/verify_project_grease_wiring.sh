#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/android/apply_project_grease_wiring.sh"
bash "$ROOT/android/complete_project_grease_wiring.sh"

UPSTREAM="$ROOT/vendor/blender_android_upstream"
ACTIVITY="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender/BlenderActivity.java"
VIEW="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender/ProjectGreaseOverlayView.java"
MANIFEST="$UPSTREAM/build_files/android/apk/app/src/main/AndroidManifest.xml"
GHOST_H="$UPSTREAM/intern/ghost/intern/GHOST_SystemAndroid.hh"
GHOST_CC="$UPSTREAM/intern/ghost/intern/GHOST_SystemAndroid.cc"
GHOST_MAIN="$UPSTREAM/intern/ghost/intern/GHOST_AndroidMain.cc"
STARTUP="$UPSTREAM/build_files/android/apk/app/src/main/assets/project_grease_startup.py"

required=(
  "$ACTIVITY" "$VIEW" "$MANIFEST" "$GHOST_H" "$GHOST_CC" "$GHOST_MAIN"
  "$STARTUP" "$UPSTREAM/build_files/android/build.py"
)
for f in "${required[@]}"; do
  test -f "$f" || { echo "MISSING: $f" >&2; exit 1; }
done

grep -q 'installProjectGreaseOverlay' "$ACTIVITY"
grep -q 'addContentView' "$ACTIVITY"
! grep -q 'nativeProjectGreaseTouch' "$ACTIVITY"
grep -q 'package org.blender.blender;' "$VIEW"
grep -q 'return false;' "$VIEW"
! grep -q 'ProjectGreaseOverlayActivity' "$MANIFEST"

# Project Grease must not add a second MotionEvent -> GHOST bridge.
! grep -q 'handleProjectGreaseTouch' "$GHOST_H"
! grep -q 'dispatchProjectGreaseTouch' "$GHOST_CC"
! grep -q 'nativeProjectGreaseTouch' "$GHOST_MAIN"

# The actual Blender Android GHOST path must remain present.
grep -q 'handleInputEvent' "$GHOST_CC"
grep -q 'GHOST_SystemAndroid' "$GHOST_H"

grep -q 'grease_pencil_add' "$STARTUP"
grep -q 'PAINT_GREASE_PENCIL' "$STARTUP"
grep -q 'BLENDER_ANDROID_STARTUP_SCRIPT' "$UPSTREAM/intern/ghost/intern/GHOST_AndroidMain.cc"

python3 -m py_compile "$UPSTREAM/build_files/android/build.py"

git -C "$UPSTREAM" diff --check

echo "Project Grease hybrid wiring verification: PASS"
echo "Input architecture: Android InputView -> Blender GHOST only"
echo "Renderer architecture: Blender 3D Viewport -> Draw Manager -> GPU/Vulkan"
