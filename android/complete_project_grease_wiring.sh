#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="$ROOT/vendor/blender_android_upstream"
ACTIVITY="$UPSTREAM/build_files/android/apk/app/src/main/java/org/blender/blender/BlenderActivity.java"
GHOST_MAIN="$UPSTREAM/intern/ghost/intern/GHOST_AndroidMain.cc"
test -f "$ACTIVITY"
test -f "$GHOST_MAIN"
python3 - "$ACTIVITY" "$GHOST_MAIN" <<'PY'
from pathlib import Path
import sys
activity = Path(sys.argv[1])
s = activity.read_text()
method = r'''  private void installProjectGreaseStartupScript() {
    File root = new File(getFilesDir(), "blender/" + VERSION);
    File out = new File(root, "project_grease_startup.py");
    try (InputStream is = getAssets().open("project_grease_startup.py");
         OutputStream os = new FileOutputStream(out)) {
      byte[] buf = new byte[65536];
      int n;
      while ((n = is.read(buf)) > 0) os.write(buf, 0, n);
      Os.setenv("BLENDER_ANDROID_STARTUP_SCRIPT", out.getAbsolutePath(), true);
    } catch (Exception ex) {
      Log.e(TAG, "Project Grease startup script install failed", ex);
    }
  }

'''
if "installProjectGreaseStartupScript()" not in s:
    anchor = "  /* Scoped storage confines the app to its sandbox"
    if anchor not in s: raise SystemExit("Activity insertion anchor not found")
    s = s.replace(anchor, method + anchor, 1)
if "installProjectGreaseStartupScript();" not in s:
    anchor = "    extractRuntimeIfNeeded();\n"
    if anchor not in s: raise SystemExit("Activity extractRuntimeIfNeeded anchor not found")
    s = s.replace(anchor, anchor + "    installProjectGreaseStartupScript();\n", 1)
activity.write_text(s)

main = Path(sys.argv[2])
s = main.read_text()
needle = '''        if (const char *env = getenv("BLENDER_ANDROID_OPEN_FILE")) {
          std::string open_file(env);
          argv.push_back(open_file.c_str());
        }
'''
insert = needle + '''        std::string project_grease_startup_script;
        if (const char *env = getenv("BLENDER_ANDROID_STARTUP_SCRIPT")) {
          project_grease_startup_script = env;
          argv.push_back("--python");
          argv.push_back(project_grease_startup_script.c_str());
        }
'''
if "BLENDER_ANDROID_STARTUP_SCRIPT" not in s:
    if needle not in s: raise SystemExit("GHOST launch-argv anchor not found")
    s = s.replace(needle, insert, 1)
main.write_text(s)
PY
echo "Project Grease startup execution wiring applied."
