#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="${1:-$ROOT/vendor/blender_android_upstream}"
SRC="$ROOT/android/projectgrease"

JAVA_TARGET="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender"
ASSETS="$UPSTREAM/build_files/android/apk/app/src/main/assets"

if [ ! -d "$UPSTREAM/source" ]; then
  echo "Blender source not found at: $UPSTREAM" >&2
  exit 1
fi

mkdir -p "$JAVA_TARGET" "$ASSETS"
cp "$SRC/ProjectGreaseOverlayView.java" "$JAVA_TARGET/"
cp "$SRC/project_grease_startup.py" "$ASSETS/"

python3 - "$UPSTREAM" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])

activity_java = root / "build_files/android/apk/app/src/main/java/org/blender/blender/BlenderActivity.java"
s = activity_java.read_text()

# Project Grease is installed inside BlenderActivity, not as a second Activity.
# Blender is a NativeActivity with one real Vulkan/GHOST window; keeping one
# Activity avoids pausing/restarting Blender or creating a second native loop.
imports = "import android.view.ViewGroup;\n"
if imports not in s:
    anchor = "import android.view.View;\n"
    if anchor not in s:
        raise SystemExit("BlenderActivity View import anchor not found")
    s = s.replace(anchor, anchor + imports, 1)

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
        raise SystemExit("BlenderActivity method insertion anchor not found")
    s = s.replace(anchor, method + anchor, 1)

if "installProjectGreaseOverlay();" not in s:
    anchor = "    super.onCreate(state);\n    enterImmersive();\n"
    if anchor not in s:
        raise SystemExit("BlenderActivity onCreate overlay anchor not found")
    s = s.replace(anchor, anchor + "    installProjectGreaseOverlay();\n", 1)

s = s.replace(
    "    startActivity(new Intent(this, com.smitnk.projectgrease.ProjectGreaseOverlayActivity.class));\n",
    "")
activity_java.write_text(s)

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
m = m.replace(old, "")
manifest.write_text(m)

# Project Grease deliberately does not inject touch into GHOST. Blender's
# Android InputView already owns the MotionEvent -> GHOST path, including
# stylus pressure, eraser, gestures, focus and coordinate handling.
# The overlay returns false for center events so Android dispatches them to
# the underlying NativeActivity input surface.
PY
activity_java.write_text(s)

# Remove the old experimental manifest Activity if it exists.
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
m = m.replace(old, "")
manifest.write_text(m)

# GHOST receives overlay input on Blender's own event thread.
header = root / "intern/ghost/intern/GHOST_SystemAndroid.hh"
s = header.read_text()
needle = "  void handleOpenMainFile(const char *path);\n"
insert = needle + """\n  /* Touch forwarded by the Project Grease view living in BlenderActivity. */\n  void handleProjectGreaseTouch(\n      int32_t action, float x, float y, float pressure, int32_t tool_type, int32_t meta_state);\n"""
if "handleProjectGreaseTouch(" not in s:
    if needle not in s:
        raise SystemExit("GHOST_SystemAndroid.hh open-file anchor not found")
    s = s.replace(needle, insert, 1)

struct_needle = """  struct JavaKeyEvent {
    int32_t keycode, action, meta_state;
  };
"""
struct_insert = struct_needle + """  struct ProjectGreaseTouchEvent {
    int32_t action, tool_type, meta_state;
    float x, y, pressure;
  };
"""
if "struct ProjectGreaseTouchEvent" not in s:
    if struct_needle not in s:
        raise SystemExit("GHOST_SystemAndroid.hh JavaKeyEvent anchor not found")
    s = s.replace(struct_needle, struct_insert, 1)

dispatch_needle = "  void dispatchJavaKeyEvent(int32_t keycode, int32_t action, int32_t meta_state);\n"
if "dispatchProjectGreaseTouch(" not in s:
    if dispatch_needle not in s:
        raise SystemExit("GHOST_SystemAndroid.hh dispatch anchor not found")
    s = s.replace(
        dispatch_needle,
        dispatch_needle + "  void dispatchProjectGreaseTouch(const ProjectGreaseTouchEvent &event);\n",
        1)

vector_needle = "  std::vector<std::string> java_open_files_;\n"
if "project_grease_touches_" not in s:
    if vector_needle not in s:
        raise SystemExit("GHOST_SystemAndroid.hh vector anchor not found")
    s = s.replace(vector_needle, vector_needle + "  std::vector<ProjectGreaseTouchEvent> project_grease_touches_;\n", 1)
header.write_text(s)

cc = root / "intern/ghost/intern/GHOST_SystemAndroid.cc"
s = cc.read_text()
if "#include <algorithm>" not in s:
    anchor = "#include <cmath>\n"
    if anchor not in s:
        raise SystemExit("GHOST_SystemAndroid.cc cmath include anchor not found")
    s = s.replace(anchor, "#include <algorithm>\n" + anchor, 1)
s = cc.read_text()

method = """void GHOST_SystemAndroid::handleProjectGreaseTouch(
    int32_t action, float x, float y, float pressure, int32_t tool_type, int32_t meta_state)
{
  std::scoped_lock lock(java_input_mutex_);
  project_grease_touches_.push_back({action, tool_type, meta_state, x, y, pressure});
}

"""
if "void GHOST_SystemAndroid::handleProjectGreaseTouch(" not in s:
    needle = "void GHOST_SystemAndroid::handleOpenMainFile(const char *path)\n"
    if needle not in s:
        raise SystemExit("GHOST_SystemAndroid.cc open-file method anchor not found")
    s = s.replace(needle, method + needle, 1)

old = """  std::vector<std::string> open_files;
  {
    std::scoped_lock lock(java_input_mutex_);
    if (java_text_.empty() && java_keys_.empty() && java_open_files_.empty()) {
      return;
    }
    text.swap(java_text_);
    keys.swap(java_keys_);
    open_files.swap(java_open_files_);
  }
"""
new = """  std::vector<std::string> open_files;
  std::vector<ProjectGreaseTouchEvent> project_grease_touches;
  {
    std::scoped_lock lock(java_input_mutex_);
    if (java_text_.empty() && java_keys_.empty() && java_open_files_.empty() &&
        project_grease_touches_.empty()) {
      return;
    }
    text.swap(java_text_);
    keys.swap(java_keys_);
    open_files.swap(java_open_files_);
    project_grease_touches.swap(project_grease_touches_);
  }
"""
if "project_grease_touches.swap" not in s:
    if old not in s:
        raise SystemExit("GHOST_SystemAndroid.cc drain block anchor not found")
    s = s.replace(old, new, 1)

needle = """  for (const JavaKeyEvent &key : keys) {
    dispatchJavaKeyEvent(key.keycode, key.action, key.meta_state);
  }
"""
if "for (const ProjectGreaseTouchEvent &event : project_grease_touches)" not in s:
    if needle not in s:
        raise SystemExit("GHOST_SystemAndroid.cc Java key dispatch anchor not found")
    s = s.replace(needle, needle + """  for (const ProjectGreaseTouchEvent &event : project_grease_touches) {
    dispatchProjectGreaseTouch(event);
  }
""", 1)

dispatch = """void GHOST_SystemAndroid::dispatchProjectGreaseTouch(
    const ProjectGreaseTouchEvent &event)
{
  if (!window_) {
    return;
  }

  const int32_t x = int32_t(ghost_android_scale_input(event.x));
  const int32_t y = int32_t(ghost_android_scale_input(event.y));
  cursor_x_ = x;
  cursor_y_ = y;
  meta_state_ = event.meta_state;

  GHOST_TabletData tablet = GHOST_TABLET_DATA_NONE;
  if (event.tool_type == 2) {
    tablet.Active = GHOST_kTabletModeStylus;
    tablet.Pressure = std::max(0.0f, std::min(1.0f, event.pressure));
  }
  else if (event.tool_type == 4) {
    tablet.Active = GHOST_kTabletModeEraser;
    tablet.Pressure = std::max(0.0f, std::min(1.0f, event.pressure));
  }
  touch_tablet_ = tablet;

  pushEvent(std::make_unique<GHOST_EventCursor>(
      getMilliSeconds(), GHOST_kEventCursorMove, window_, x, y, tablet));

  if (event.action == 0) {
    touch_pending_ = false;
    touchSendButton(GHOST_kButtonMaskLeft, GHOST_kEventButtonDown);
  }
  else if (event.action == 1 || event.action == 3) {
    if (touch_button_down_) {
      touchSendButton(touch_button_, GHOST_kEventButtonUp);
    }
    touch_pending_ = false;
  }
}
"""
if "void GHOST_SystemAndroid::dispatchProjectGreaseTouch(" not in s:
    needle = "void GHOST_SystemAndroid::dispatchJavaKeyEvent(int32_t keycode, int32_t action, int32_t meta_state)\n"
    if needle not in s:
        raise SystemExit("GHOST_SystemAndroid.cc Java dispatch anchor not found")
    s = s.replace(needle, dispatch + "\n" + needle, 1)
cc.write_text(s)

main = root / "intern/ghost/intern/GHOST_AndroidMain.cc"
s = main.read_text()
jni = """extern "C" JNIEXPORT void JNICALL
Java_org_blender_blender_BlenderActivity_nativeProjectGreaseTouch(
    JNIEnv * /*env*/,
    jobject /*thiz*/,
    jint action,
    jfloat x,
    jfloat y,
    jfloat pressure,
    jint tool_type,
    jint meta_state)
{
  if (GHOST_SystemAndroid *system = android_system_if_ready()) {
    system->handleProjectGreaseTouch(action, x, y, pressure, tool_type, meta_state);
  }
}

"""
if "Java_org_blender_blender_BlenderActivity_nativeProjectGreaseTouch" not in s:
    anchor = "extern " + '"C" JNIEXPORT void JNICALL Java_org_blender_blender_BlenderActivity_nativeOpenMainFile('
    if anchor not in s:
        raise SystemExit("GHOST_AndroidMain.cc nativeOpenMainFile anchor not found")
    s = s.replace(anchor, jni + anchor, 1)
main.write_text(s)

print("Project Grease wiring script updated: single Blender NativeActivity, real Vulkan surface preserved, overlay installed in the same Activity, GHOST JNI bridge retained.")
PY

echo "Project Grease Blender wiring applied."
