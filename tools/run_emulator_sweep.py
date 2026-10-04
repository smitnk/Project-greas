#!/usr/bin/env python3
"""Full emulator sweep driver (CI emulator-tests job).

Runs every instrumented test class; when a class run stops early (a process crash takes the rest of
the class with it) the tests that did not report are re-run one by one, so one crash never hides
other results. Then a fixed-seed monkey run. Logcat is captured for the whole run and scanned for
crashes, ANRs, JNI aborts, GL errors and sanitizer reports. Failures, crash backtraces and frame
times are printed to the job log as well as written to the artifacts.

Env: SWEEP_CLASS (one class or Class#method), MONKEY_SEED, MONKEY_EVENTS.
"""
import os
import re
import subprocess
import sys
import time

PKG = "com.smitnk.projectgrease"
RUNNER = PKG + ".test/androidx.test.runner.AndroidJUnitRunner"
SEED = os.environ.get("MONKEY_SEED", "20240501")
EVENTS = int(os.environ.get("MONKEY_EVENTS", "20000"))


def sh(cmd, out=None, check=False):
    print("+ " + cmd, flush=True)
    r = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    if out:
        with open(out, "a") as f:
            f.write(r.stdout)
    if check and r.returncode != 0:
        print(r.stdout)
        sys.exit(r.returncode)
    return r.stdout


def instrument(filter_args):
    return sh(f"adb shell am instrument -w -r {filter_args} {RUNNER}", out="instrument.txt")


def blocks(output):
    """The status blocks of `am instrument -r`: ({key: value}, code), multi-line values joined."""
    cur, key = {}, None
    for line in output.splitlines():
        if line.startswith("INSTRUMENTATION_STATUS: "):
            k, _, v = line[len("INSTRUMENTATION_STATUS: "):].partition("=")
            cur[k] = v
            key = k
        elif line.startswith("INSTRUMENTATION_STATUS_CODE: "):
            try:
                code = int(line.split(":", 1)[1])
            except ValueError:
                code = 0
            yield cur, code
            cur, key = {}, None
        elif line.startswith("INSTRUMENTATION_"):
            key = None
        elif key:
            cur[key] += "\n" + line


def parse(output):
    """{(class, test): (code, stack)} for every test that reported an end status (0 ok, -2 fail, -3 skip)."""
    results = {}
    for b, code in blocks(output):
        if code != 1 and "class" in b and "test" in b:
            results[(b["class"], b["test"])] = (code, b.get("stack", ""))
    return results


def listing():
    out = sh(f"adb shell am instrument -w -r -e log true -e package {PKG} {RUNNER}")
    tests = []
    for b, code in blocks(out):
        k = (b.get("class"), b.get("test"))
        if code == 1 and k[0] and k[1] and k not in tests:
            tests.append(k)
    return tests


def main():
    for f in ["instrument.txt", "monkey.txt"]:
        open(f, "w").close()
    sh("adb install -r -g apks/app-debug.apk", check=True)
    sh("adb install -r -g apks/app-debug-androidTest.apk", check=True)
    sh("adb logcat -c")
    logcat = subprocess.Popen("adb logcat -v threadtime", shell=True, stdout=open("logcat_full.txt", "w"),
                              stderr=subprocess.STDOUT)

    results = {}
    only = os.environ.get("SWEEP_CLASS", "").strip()
    if only:
        results.update(parse(instrument(f"-e class {PKG}.{only}")))
        planned = list(results.keys())
    else:
        planned = listing()
        classes = []
        for cls, _ in planned:
            if cls not in classes:
                classes.append(cls)
        for cls in classes:
            results.update(parse(instrument(f"-e class {cls}")))
            missing = [(c, t) for (c, t) in planned if c == cls and (c, t) not in results]
            for c, t in missing:  # a crash ended the class run early: isolate the rest
                r = parse(instrument(f"-e class {c}#{t}"))
                results.update(r)
                if (c, t) not in r:
                    results[(c, t)] = (-99, "no result (process crashed before the test reported)")

    os.makedirs("screenshots", exist_ok=True)
    sh("adb pull /data/local/tmp/pg_screenshots/. screenshots/")

    # Monkey: fixed seed, app package only, no system keys / app switches. System ANRs (systemui on
    # a slow emulator) are not the app's and must not abort the run; the app's own crashes and ANRs
    # are detected below.
    sh(f"adb shell am force-stop {PKG}")
    monkey = sh(f"adb shell monkey -p {PKG} -s {SEED} --throttle 50 --pct-syskeys 0 --pct-appswitch 0 "
                f"--ignore-timeouts -v {EVENTS}", out="monkey.txt")
    sh("adb shell screencap -p /data/local/tmp/monkey_end.png && adb pull /data/local/tmp/monkey_end.png screenshots/monkey_end.png")
    # ANR traces (google_apis images allow adb root): the app's main-thread stack goes to the log.
    sh("adb root && sleep 2 && mkdir -p anr && adb pull /data/anr/. anr/")
    anr_stacks = []
    for name in sorted(os.listdir("anr")) if os.path.isdir("anr") else []:
        text = open(os.path.join("anr", name), errors="replace").read()
        if PKG not in text:
            continue
        m = re.search(r'"main".*?(?=\n\n)', text, re.S)
        if m:
            anr_stacks.append(f"  {name}:\n  " + m.group(0)[:3000].replace("\n", "\n  "))
    time.sleep(2)
    logcat.terminate()
    sh("adb logcat -d -s ProjectGrease:* TestRunner:* AndroidRuntime:* PGSweep:* DEBUG:*", out="logcat.txt")

    log = open("logcat_full.txt", errors="replace").read()
    passed = sorted(k for k, v in results.items() if v[0] == 0)
    failed = sorted((k, v) for k, v in results.items() if v[0] != 0 and v[0] != -3)
    skipped = sorted(k for k, v in results.items() if v[0] == -3)
    lines = [f"== instrumentation: {len(planned)} planned, {len(passed)} passed, {len(failed)} failed, {len(skipped)} skipped =="]
    for (c, t), (code, stack) in failed:
        first = "\n      ".join(stack.splitlines()[:4])
        lines.append(f"FAIL {c.split('.')[-1]}.{t} (code {code}): {first}")
    lines.append("== frame time ==")
    lines += re.findall(r"FRAMETIME .*", log)
    injected = re.findall(r"Events injected: (\d+)", monkey)
    app_crash = re.findall(r"// CRASH: " + re.escape(PKG) + r".*", monkey)
    app_anr = re.findall(r"// NOT RESPONDING: " + re.escape(PKG) + r".*", monkey)
    lines.append(f"== monkey (seed {SEED}, {EVENTS} events): injected {injected[-1] if injected else 0}, "
                 f"app crashes {len(app_crash)}, app ANRs {len(app_anr)} ==")
    lines += app_crash + app_anr
    lines += re.findall(r"// NOT RESPONDING: (?!" + re.escape(PKG) + r").*", monkey)[:3]
    lines.append("== logcat scan ==")
    pats = {"FATAL EXCEPTION": r"FATAL EXCEPTION", "ANR in app": r"ANR in " + re.escape(PKG),
            "JNI DETECTED ERROR": r"JNI DETECTED ERROR", "Abort message": r"Abort message",
            "Fatal signal": r"Fatal signal", "AddressSanitizer": r"AddressSanitizer",
            "GL errors": r"GL_INVALID|glGetError|EGL_BAD"}
    counts = {k: len(re.findall(p, log)) for k, p in pats.items()}
    lines += [f"{k}: {v}" for k, v in counts.items()]
    # crash details: fatal signal + abort message + the first frames of each tombstone backtrace
    for m in re.finditer(r"(Fatal signal.*|Abort message.*|FATAL EXCEPTION.*(?:\n.*AndroidRuntime.*){0,12})", log):
        lines.append("  " + m.group(1)[:400].replace("\n", "\n  "))
    for m in re.finditer(r"ANR in " + re.escape(PKG) + r".*(?:\n.*ActivityManager.*){0,14}", log):
        lines.append("  " + m.group(0)[:2000].replace("\n", "\n  "))
    lines += anr_stacks[:3]
    for m in re.finditer(r"backtrace:\n((?:.*DEBUG.*#\d\d.*\n){1,12})", log):
        lines.append("  backtrace:\n" + m.group(1))
    summary = "\n".join(lines)
    open("sweep_summary.txt", "w").write(summary + "\n")
    print(summary)

    ok = (not failed and len(passed) + len(skipped) == len(planned) and injected and int(injected[-1]) == EVENTS
          and not app_crash and not app_anr and counts["FATAL EXCEPTION"] == 0 and counts["ANR in app"] == 0
          and counts["JNI DETECTED ERROR"] == 0 and counts["Fatal signal"] == 0 and counts["AddressSanitizer"] == 0)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
