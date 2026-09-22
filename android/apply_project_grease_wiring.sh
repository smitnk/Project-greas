#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="${1:-$ROOT/vendor/blender_android_upstream}"
SRC="$ROOT/android/projectgrease"

JAVA_TARGET="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender"
ASSETS="$UPSTREAM/build_files/android/apk/app/src/main/assets"

test -d "$UPSTREAM/source"
mkdir -p "$JAVA_TARGET" "$ASSETS"
cp "$SRC/ProjectGreaseOverlayView.java" "$JAVA_TARGET/"
cp "$SRC/project_grease_startup.py" "$ASSETS/"

python3 - "$UPSTREAM" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])

# Project Grease lives in Blender's single NativeActivity. The overlay is UI
# only; center events are not consumed, so Blender's existing Android InputView
# remains the sole MotionEvent -> GHOST input path.
activity = root / "build_files/android/apk/app/src/main/java/org/blender/blender/BlenderActivity.java"
s = activity.read_text()

if "import android.view.ViewGroup;" not in s:
    anchor = "import android.view.View;\n"
    if anchor not in s:
        raise SystemExit("View import anchor not found")
    s = s.replace(anchor, anchor + "import android.view.ViewGroup;\n", 1)

method = """  private void installProjectGreaseOverlay() {
    ProjectGreaseOverlayView overlay = new ProjectGreaseOverlayView(this);
    addContentView(
        overlay,
        new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));
  }

"""
if "private void installProjectGreaseOverlay()" not in s:
    anchor = "  /* Scoped storage confines the app to its sandbox"
    if anchor not in s:
        raise SystemExit("overlay insertion anchor not found")
    s = s.replace(anchor, method + anchor, 1)

if "installProjectGreaseOverlay();" not in s:
    anchor = "    super.onCreate(state);\n    enterImmersive();\n"
    if anchor not in s:
        raise SystemExit("onCreate anchor not found")
    s = s.replace(anchor, anchor + "    installProjectGreaseOverlay();\n", 1)

s = s.replace(
    "    startActivity(new Intent(this, com.smitnk.projectgrease.ProjectGreaseOverlayActivity.class));\n",
    "")
activity.write_text(s)

manifest = root / "build_files/android/apk/app/src/main/AndroidManifest.xml"
m = manifest.read_text()
old = """        <activity
            android:name="com.smitnk.projectgrease.ProjectGreaseOverlayActivity"
            android:theme="@android:style/Theme.Translucent.NoTitleBar.Fullscreen"
            android:screenOrientation="user"
            android:configChanges="orientation|keyboardHidden|keyboard|screenSize|screenLayout|density|navigation|uiMode"
            android:launchMode="singleTop"
            android:exported="false" />
"""
manifest.write_text(m.replace(old, ""))

# Do not patch GHOST with a second Project Grease input bridge.
# The Android Blender port already handles touch/stylus/pressure/gestures,
# focus and coordinate scaling in GHOST_SystemAndroid.
print("Project Grease: single Activity + UI overlay installed; native Blender Android input preserved.")
PY

echo "Project Grease Blender wiring applied."
