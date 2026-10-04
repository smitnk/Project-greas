#!/usr/bin/env bash
# Full emulator sweep (CI emulator-tests job): every instrumented test class, then a fixed-seed
# monkey run, with logcat captured for the whole run and scanned for crashes, ANRs, JNI aborts,
# GL errors and sanitizer reports. Artifacts: screenshots/, instrument.txt, logcat_full.txt,
# logcat.txt, monkey.txt, sweep_summary.txt.
set -x
PKG=com.smitnk.projectgrease
RUNNER=$PKG.test/androidx.test.runner.AndroidJUnitRunner
SEED=${MONKEY_SEED:-20240501}
EVENTS=${MONKEY_EVENTS:-20000}

adb install -r -g apks/app-debug.apk
adb install -r -g apks/app-debug-androidTest.apk
adb logcat -c
adb logcat -v threadtime > logcat_full.txt 2>&1 &
LOGCAT_PID=$!

# ${SWEEP_CLASS} reruns one class (or Class#method); default: the whole package.
if [ -n "${SWEEP_CLASS:-}" ]; then FILTER="-e class $PKG.$SWEEP_CLASS"; else FILTER="-e package $PKG"; fi
adb shell am instrument -w -r $FILTER $RUNNER | tee instrument.txt

mkdir -p screenshots
adb pull /data/local/tmp/pg_screenshots/. screenshots/ || true

# Monkey: fixed seed, app package only, no system keys (they would leave the app).
adb shell am force-stop $PKG
adb shell monkey -p $PKG -s $SEED --throttle 50 --pct-syskeys 0 --pct-appswitch 0 -v $EVENTS > monkey.txt 2>&1
MONKEY_RC=$?
adb shell screencap -p /data/local/tmp/monkey_end.png && adb pull /data/local/tmp/monkey_end.png screenshots/monkey_end.png || true

sleep 2
kill $LOGCAT_PID || true
adb logcat -d -s ProjectGrease:* TestRunner:* AndroidRuntime:* PGSweep:* > logcat.txt || true

{
  echo "== instrumentation =="
  grep -E "^(OK|FAILURES|Tests run)" instrument.txt
  echo "== per-test results (PGSweep) =="
  grep -o "RESULT .*" logcat_full.txt | sort -u
  echo "== frame time =="
  grep -o "FRAMETIME .*" logcat_full.txt
  echo "== monkey (seed $SEED, $EVENTS events, rc $MONKEY_RC) =="
  grep -E "Events injected|// CRASH|// NOT RESPONDING|Monkey aborted|Network stats" monkey.txt
  echo "== logcat scan =="
  for pat in "FATAL EXCEPTION" "ANR in $PKG" "JNI DETECTED ERROR" "Abort message" "Fatal signal" "AddressSanitizer" "glGetError" "GL_INVALID" "EGL_BAD"; do
    echo "$pat: $(grep -c "$pat" logcat_full.txt)"
  done
} > sweep_summary.txt
cat sweep_summary.txt

STATUS=0
grep -q "^OK (" instrument.txt || STATUS=1
grep -qE "// CRASH|// NOT RESPONDING" monkey.txt && STATUS=1
grep -q "Events injected: $EVENTS" monkey.txt || STATUS=1
grep -qE "FATAL EXCEPTION|ANR in $PKG|JNI DETECTED ERROR|Fatal signal|AddressSanitizer" logcat_full.txt && STATUS=1
exit $STATUS
