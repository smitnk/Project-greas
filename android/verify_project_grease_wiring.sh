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
  if [ ! -f "$f" ]; then
    echo "FAIL: missing $f" >&2
    exit 1
  fi
done

require_text() {
  local file="$1" text="$2" name="$3"
  if ! grep -Fq "$text" "$file"; then
    echo "FAIL: missing [$name] in $file" >&2
    exit 1
  fi
}

require_absent() {
  local file="$1" text="$2" name="$3"
  if grep -Fq "$text" "$file"; then
    echo "FAIL: forbidden [$name] found in $file" >&2
    exit 1
  fi
}

require_text "$ACTIVITY" "installProjectGreaseOverlay" "overlay installer"
require_text "$ACTIVITY" "addContentView" "overlay addContentView"
require_absent "$ACTIVITY" "nativeProjectGreaseTouch" "custom native touch bridge"
require_text "$VIEW" "package org.blender.blender;" "overlay package"
require_text "$VIEW" "return false;" "center event fall-through"
require_absent "$MANIFEST" "ProjectGreaseOverlayActivity" "obsolete second Activity"

require_absent "$GHOST_H" "handleProjectGreaseTouch" "custom GHOST touch handler"
require_absent "$GHOST_CC" "dispatchProjectGreaseTouch" "custom GHOST dispatch bridge"
require_absent "$GHOST_MAIN" "nativeProjectGreaseTouch" "custom native touch bridge"

require_text "$GHOST_CC" "handleInputEvent" "native Android input handler"
require_text "$GHOST_H" "GHOST_SystemAndroid" "Android GHOST class"
require_text "$STARTUP" "grease_pencil_add" "real Grease Pencil creation"
require_text "$STARTUP" "PAINT_GREASE_PENCIL" "native Grease Pencil Draw Mode"
require_text "$GHOST_MAIN" "BLENDER_ANDROID_STARTUP_SCRIPT" "startup-script environment wiring"

python3 -m py_compile "$UPSTREAM/build_files/android/build.py"

if ! git -C "$UPSTREAM" diff --check; then
  echo "FAIL: Blender submodule diff --check reported whitespace errors" >&2
  exit 1
fi
echo "Project Grease hybrid wiring verification: PASS"
echo "Input architecture: Android InputView -> Blender GHOST only"
echo "Renderer architecture: Blender 3D Viewport -> Draw Manager -> GPU/Vulkan"
