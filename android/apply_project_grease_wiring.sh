#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="${1:-$ROOT/vendor/blender_android_upstream}"
SRC="$ROOT/android/projectgrease"
TARGET="$UPSTREAM/build_files/android/apk/app/src/main/java/com/smitnk/projectgrease"

if [ ! -d "$UPSTREAM/source" ]; then
  echo "Blender source not found at: $UPSTREAM" >&2
  exit 1
fi

mkdir -p "$TARGET"
cp "$SRC/ProjectGreaseOverlayActivity.java" "$TARGET/"
cp "$SRC/ProjectGreaseOverlayView.java" "$TARGET/"

python3 - "$UPSTREAM" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])

manifest = root / "build_files/android/apk/app/src/main/AndroidManifest.xml"
s = manifest.read_text()
activity = '''        <activity
            android:name="com.smitnk.projectgrease.ProjectGreaseOverlayActivity"
            android:theme="@android:style/Theme.Translucent.NoTitleBar.Fullscreen"
            android:screenOrientation="user"
            android:exported="false" />'''
if "com.smitnk.projectgrease.ProjectGreaseOverlayActivity" not in s:
    s = s.replace("    </application>", activity + "\n    </application>")
manifest.write_text(s)

activity_java = root / "build_files/android/apk/app/src/main/java/org/blender/blender/BlenderActivity.java"
s = activity_java.read_text()
needle = "    enterImmersive();\n"
insert = "    enterImmersive();\n    startActivity(new Intent(this, com.smitnk.projectgrease.ProjectGreaseOverlayActivity.class));\n"
if "startActivity(new Intent(this, com.smitnk.projectgrease.ProjectGreaseOverlayActivity.class))" not in s:
    if needle not in s:
        raise SystemExit("BlenderActivity anchor not found")
    s = s.replace(needle, insert, 1)
activity_java.write_text(s)

header = root / "intern/ghost/intern/GHOST_SystemAndroid.hh"
s = header.read_text()
needle = "  int32_t handleInputEvent(AInputEvent *event);\n"
insert = needle + '''
  /* Touch forwarded by the Project Grease translucent UI activity. */
  void handleProjectGreaseTouch(
      int32_t action, float x, float y, float pressure, int32_t tool_type, int32_t meta_state);
'''
if "handleProjectGreaseTouch(" not in s:
    if needle not in s:
        raise SystemExit("GHOST header input anchor not found")
    s = s.replace(needle, insert, 1)

needle = "  struct JavaKeyEvent {\n    int32_t keycode, action, meta_state;\n  };\n"
insert = needle + '''
  struct ProjectGreaseTouchEvent {
    int32_t action, tool_type, meta_state;
    float x, y, pressure;
  };
'''
if "struct ProjectGreaseTouchEvent" not in s:
    s = s.replace(needle, insert, 1)

needle = "  void dispatchJavaKeyEvent(int32_t keycode, int32_t action, int32_t meta_state);\n"
insert = needle + "  void dispatchProjectGreaseTouch(const ProjectGreaseTouchEvent &event);\n"
if "dispatchProjectGreaseTouch(" not in s:
    s = s.replace(needle, insert, 1)

needle = "  std::vector<std::string> java_open_files_;\n"
insert = needle + "  std::vector<ProjectGreaseTouchEvent> project_grease_touches_;\n"
if "project_grease_touches_" not in s:
    s = s.replace(needle, insert, 1)
header.write_text(s)

cc = root / "intern/ghost/intern/GHOST_SystemAndroid.cc"
s = cc.read_text()

needle = '''void GHOST_SystemAndroid::handleOpenMainFile(const char *path)
{
  if (!path || !path[0]) {
    return;
  }
  std::scoped_lock lock(java_input_mutex_);
  java_open_files_.push_back(path);
}
'''
insert = needle + '''
void GHOST_SystemAndroid::handleProjectGreaseTouch(
    int32_t action, float x, float y, float pressure, int32_t tool_type, int32_t meta_state)
{
  std::scoped_lock lock(java_input_mutex_);
  project_grease_touches_.push_back({action, tool_type, meta_state, x, y, pressure});
}
'''
if "void GHOST_SystemAndroid::handleProjectGreaseTouch(" not in s:
    if needle not in s:
        raise SystemExit("GHOST cc open-file anchor not found")
    s = s.replace(needle, insert, 1)

needle = '''  std::vector<std::string> open_files;
  {
    std::scoped_lock lock(java_input_mutex_);
    if (java_text_.empty() && java_keys_.empty() && java_open_files_.empty()) {
      return;
    }
    text.swap(java_text_);
    keys.swap(java_keys_);
    open_files.swap(java_open_files_);
  }
'''
insert = '''  std::vector<std::string> open_files;
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
'''
if "project_grease_touches.swap" not in s:
    if needle not in s:
        raise SystemExit("GHOST drain anchor not found")
    s = s.replace(needle, insert, 1)

needle = '''  for (const JavaKeyEvent &key : keys) {
    dispatchJavaKeyEvent(key.keycode, key.action, key.meta_state);
  }
'''
insert = needle + '''  for (const ProjectGreaseTouchEvent &event : project_grease_touches) {
    dispatchProjectGreaseTouch(event);
  }
'''
if "for (const ProjectGreaseTouchEvent &event" not in s:
    s = s.replace(needle, insert, 1)

needle = '''void GHOST_SystemAndroid::dispatchJavaKeyEvent(int32_t keycode, int32_t action, int32_t meta_state)
{
'''
insert = '''void GHOST_SystemAndroid::dispatchProjectGreaseTouch(
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

  pushEvent(std::make_unique<GHOST_EventCursor>(
      getMilliSeconds(), GHOST_kEventCursorMove, window_, x, y, tablet));

  if (event.action == 0) {
    buttons_.set(GHOST_kButtonMaskLeft, true);
    pushEvent(std::make_unique<GHOST_EventButton>(
        getMilliSeconds(), GHOST_kEventButtonDown, window_, GHOST_kButtonMaskLeft, tablet));
  }
  else if (event.action == 1 || event.action == 3) {
    buttons_.set(GHOST_kButtonMaskLeft, false);
    pushEvent(std::make_unique<GHOST_EventButton>(
        getMilliSeconds(), GHOST_kEventButtonUp, window_, GHOST_kButtonMaskLeft, tablet));
  }
}

''' + needle
if "void GHOST_SystemAndroid::dispatchProjectGreaseTouch(" not in s:
    if needle not in s:
        raise SystemExit("GHOST dispatch key anchor not found")
    s = s.replace(needle, insert, 1)

cc.write_text(s)

main = root / "intern/ghost/intern/GHOST_AndroidMain.cc"
s = main.read_text()
needle = '''extern "C" JNIEXPORT void JNICALL Java_org_blender_blender_BlenderActivity_nativeOpenMainFile(
'''
jni = '''extern "C" JNIEXPORT void JNICALL
Java_com_smitnk_projectgrease_ProjectGreaseOverlayActivity_nativeProjectGreaseTouch(
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

'''
if "Java_com_smitnk_projectgrease_ProjectGreaseOverlayActivity_nativeProjectGreaseTouch" not in s:
    if needle not in s:
        raise SystemExit("GHOST Android JNI anchor not found")
    s = s.replace(needle, jni + needle, 1)
main.write_text(s)
PY

echo "Project Grease Blender wiring applied."
