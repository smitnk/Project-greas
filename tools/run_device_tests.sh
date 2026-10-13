#!/usr/bin/env bash
# Device tests on a running emulator (CI: reactivecircus/android-emulator-runner): installs the debug
# APK and its test APK, runs the device-style test classes, prints failures as GitHub annotations
# and pulls the screenshots to android/device-shots/.
set -uo pipefail
APK=android/app/build/outputs/apk/debug/app-debug.apk
TAPK=android/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
CLASSES="${PG_DEVICE_TESTS:-com.smitnk.projectgrease.DeviceColorTest,com.smitnk.projectgrease.DeviceStudioTest,com.smitnk.projectgrease.DeviceTouchTest,com.smitnk.projectgrease.DeviceRotoTest,com.smitnk.projectgrease.DeviceBouncingBallTest,com.smitnk.projectgrease.DeviceReportedBugsTest}"
adb wait-for-device
adb install -r -g "$APK" && adb install -r -g "$TAPK" || exit 1
# A system dialog on a fresh CI emulator ("System UI isn't responding") takes the focus and blocks
# event injection (SecurityException INJECT_EVENTS): dismiss dialogs first, rerun once if it happened.
dismiss() { adb shell settings put global window_animation_scale 0 >/dev/null 2>&1
  adb shell am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS >/dev/null 2>&1
  adb shell input keyevent KEYCODE_WAKEUP; adb shell input keyevent 82; sleep 5; }
adb shell 'while [ "$(getprop sys.boot_completed)" != "1" ]; do sleep 2; done'
dismiss
run() { adb shell am instrument -w -r -e class "$CLASSES" \
  com.smitnk.projectgrease.test/androidx.test.runner.AndroidJUnitRunner | tee device-tests.txt; }
run
if grep -q "INJECT_EVENTS" device-tests.txt; then echo "::warning title=device tests::system dialog blocked input, rerunning"; dismiss; run; fi
mkdir -p android/device-shots
adb logcat -d > android/device-shots/logcat.txt 2>/dev/null || true
# app crashes: the first FATAL EXCEPTION with its stack, as one annotation
grep -A30 -m1 "FATAL EXCEPTION" android/device-shots/logcat.txt | sed 's/%/%25/g' | awk 'BEGIN{printf "::error title=device crash::"}{printf "%s%%0A", $0}END{print ""}' || true
# which renderer ran: the device tests must run on the Blender engine (OpenGL ES 3.2)
grep -m1 "GL_VERSION" android/device-shots/logcat.txt | sed 's/^/::notice title=device GL::/' || true
# native / renderer start-up lines in one annotation (the job log is not always readable)
grep -iE "GL_VERSION|PGErase|ProjectGrease|FATAL|AndroidRuntime|libc  " android/device-shots/logcat.txt \
  | grep -v "RESULT" | tail -60 | sed 's/%/%25/g' | awk 'BEGIN{printf "::notice title=device logcat::"}{printf "%s%%0A", $0}END{print ""}'
adb pull /data/local/tmp/pg_screenshots/sweep android/device-shots/ >/dev/null 2>&1 || true
python3 - <<'PY'
import re,sys
t=open("device-tests.txt",errors="replace").read()
fails=re.findall(r"INSTRUMENTATION_STATUS: class=(\S+)\nINSTRUMENTATION_STATUS: current=\d+\nINSTRUMENTATION_STATUS: id=\S+\nINSTRUMENTATION_STATUS: numtests=\d+\nINSTRUMENTATION_STATUS: stack=(.*?)\nINSTRUMENTATION_STATUS: stream=",t,re.S)
for cls,stack in fails:
    print("::error title=device test %s::%s" % (cls.split('.')[-1], stack.strip().splitlines()[0][:1500]))
for m in re.finditer(r"INSTRUMENTATION_STATUS: test=(\S+)\nINSTRUMENTATION_STATUS_CODE: (-?\d)",t):
    print("::notice title=device::%s %s" % (m.group(1), {"0":"PASS","-2":"FAIL","-1":"ERROR"}.get(m.group(2),m.group(2))))
ok=re.search(r"^OK \(\d+ tests?\)",t,re.M) is not None
print("device tests", "passed" if ok else "FAILED")
sys.exit(0 if ok else 1)
PY
