#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="$ROOT/vendor/blender_android_upstream"
ACTIVITY="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender/BlenderActivity.java"

snapshot() {
  local stage="$1"
  cp "$ACTIVITY" "/tmp/BlenderActivity.$stage.java"
}

validate_activity() {
  local stage="$1"
  python3 - "$ACTIVITY" "$stage" <<'PY'
from pathlib import Path
import sys

p = Path(sys.argv[1])
stage = sys.argv[2]
s = p.read_text()
required = [
    "import android.app.NativeActivity;",
    "import android.view.View;",
    "import android.view.ViewGroup;",
    "public class BlenderActivity extends NativeActivity",
    "getWindow().getDecorView()",
]
missing = [x for x in required if x not in s]
if missing:
    print(f"::error::BlenderActivity corrupted AFTER {stage}")
    for x in missing:
        print(f"  missing: {x}")
    raise SystemExit(1)
print(f"BlenderActivity OK after {stage}")
PY
}

test -f "$ACTIVITY"
snapshot pre
validate_activity "pre-wiring (pristine checkout)"

bash "$ROOT/android/apply_project_grease_wiring.sh"
snapshot post-apply
validate_activity "apply_project_grease_wiring.sh"
echo "[diagnostic] diff introduced by apply_project_grease_wiring.sh"
git -C "$UPSTREAM" diff --no-index "/tmp/BlenderActivity.pre.java" "$ACTIVITY" || true

bash "$ROOT/android/complete_project_grease_wiring.sh"
snapshot post-complete
validate_activity "complete_project_grease_wiring.sh"
echo "[diagnostic] diff introduced by complete_project_grease_wiring.sh"
git -C "$UPSTREAM" diff --no-index "/tmp/BlenderActivity.post-apply.java" "$ACTIVITY" || true

VIEW="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender/ProjectGreaseOverlayView.java"
MANIFEST="$UPSTREAM/build_files/android/apk/app/src/main/AndroidManifest.xml"
GHOST_H="$UPSTREAM/intern/ghost/intern/GHOST_SystemAndroid.hh"
GHOST_CC="$UPSTREAM/intern/ghost/intern/GHOST_SystemAndroid.cc"
GHOST_MAIN="$UPSTREAM/intern/ghost/intern/GHOST_AndroidMain.cc"
STARTUP="$UPSTREAM/build_files/android/apk/app/src/main/assets/project_grease_startup.py"
OBJECT_ADD="$UPSTREAM/source/blender/editors/object/object_add.cc"

required=(
  "$ACTIVITY" "$VIEW" "$MANIFEST" "$GHOST_H" "$GHOST_CC" "$GHOST_MAIN"
  "$STARTUP" "$OBJECT_ADD" "$UPSTREAM/build_files/android/build.py"
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

PACKAGE="$UPSTREAM/build_files/android/apk/package.sh"
if [ ! -f "$PACKAGE" ]; then
  echo "FAIL: missing $PACKAGE" >&2
  exit 1
fi
require_text "$PACKAGE" 'find "$SCRIPT_DIR/app/src/main/java" -type f -name '\''*.java'\'' -print0' "compile all Java sources"
require_text "$PACKAGE" '"${JAVA_SOURCES[@]}"' "javac receives all Java sources"
require_absent "$PACKAGE" '\${JAVA_SOURCES[@]}' "escaped Java source array"

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
require_text "$STARTUP" "type='STROKE'" "Grease Pencil STROKE creation"
require_text "$STARTUP" "PAINT_GREASE_PENCIL" "native Grease Pencil Draw Mode"

# Verify the startup operator against the exact pinned Blender source rather than
# assuming a newer/different Blender API.
require_text "$OBJECT_ADD" "OBJECT_OT_grease_pencil_add" "pinned Grease Pencil add operator"
require_text "$OBJECT_ADD" "ELEM(type, GP_EMPTY, GP_STROKE, GP_MONKEY)" "pinned STROKE enum"
require_text "$GHOST_MAIN" "BLENDER_ANDROID_STARTUP_SCRIPT" "startup-script environment wiring"

python3 -m py_compile "$UPSTREAM/build_files/android/build.py"
python3 -m py_compile "$STARTUP"

if ! git -C "$UPSTREAM" diff --check; then
  echo "FAIL: Blender submodule diff --check reported whitespace errors" >&2
  exit 1
fi
echo "Project Grease hybrid wiring verification: PASS"
echo "Input architecture: Android InputView -> Blender GHOST only"
echo "Renderer architecture: Blender 3D Viewport -> Draw Manager -> GPU/Vulkan"
