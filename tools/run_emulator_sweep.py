#!/usr/bin/env python3
"""Full emulator sweep driver (CI emulator-tests job).

Runs every instrumented test class; when a class run stops early (a process crash takes the rest of
the class with it) the tests that did not report are re-run one by one, so one crash never hides
other results. Then a fixed-seed monkey run. Logcat is captured for the whole run and scanned for
crashes, ANRs, JNI aborts, GL errors and sanitizer reports. Failures, crash backtraces and frame
times are printed to the job log as well as written to the artifacts.

Logcat findings (crash / abort / Fatal signal / JNI / ASan / FATAL EXCEPTION / ANR) fail the run only
when they come from the app (pids sampled with pidof during the run plus pids named by crash lines);
the same findings from other processes (systemui on a slow emulator) are warnings. A test that failed
with the INJECT_EVENTS SecurityException while a systemui ANR was visible is retried once.
Triage logic lives in tools/logcat_triage.py.

Env: SWEEP_CLASS (one class or Class#method), MONKEY_SEED, MONKEY_EVENTS.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import logcat_triage  # noqa: E402

PKG = "com.smitnk.projectgrease"
RUNNER = PKG + ".test/androidx.test.runner.AndroidJUnitRunner"
SEED = os.environ.get("MONKEY_SEED", "20240501")
# The bug-hunt environment's long tests (soak, fuzz, frame-time budget, process death) run in the
# bug-hunt workflow only (tools/run_bughunt.py).
BUGHUNT = PKG + ".bughunt.BugHunt"
EVENTS = int(os.environ.get("MONKEY_EVENTS", "20000"))
LOGCAT_FORMAT = os.environ.get("SWEEP_LOGCAT_FORMAT", "threadtime")
IDLE_TIMEOUT = int(os.environ.get("SWEEP_IDLE_TIMEOUT", "60"))
APP_PIDS = {}  # pid -> process name, sampled during the run
RETRIES = []   # "RETRY Cls#m: ..." lines for the summary


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


def quiet_system_dialogs():
    # A slow emulator's own "System UI isn't responding" dialog takes focus and every injected
    # gesture then fails with INJECT_EVENTS: keep system error dialogs off and close any open one.
    sh("adb shell settings put global hide_error_dialogs 1")
    sh("adb shell am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS")


def sample_pids():
    for name in (PKG, PKG + ".test"):
        out = subprocess.run(f"adb shell pidof {name}", shell=True, stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, text=True, errors="replace").stdout
        for p in out.split():
            if p.isdigit():
                APP_PIDS[int(p)] = name


def anr_dialog_visible():
    w = subprocess.run("adb shell dumpsys window windows", shell=True, stdout=subprocess.PIPE,
                       stderr=subprocess.DEVNULL, text=True, errors="replace").stdout
    focus = "\n".join(l for l in w.splitlines() if "mCurrentFocus" in l or "mFocusedApp" in l)
    return ("Application Not Responding" in w or "Application Error" in w
            or re.search(r"\bANR\b|isn't responding|NotResponding", focus) is not None), w


def wait_idle(timeout=IDLE_TIMEOUT):
    """Bounded wait until no ANR / error dialog is shown and the activity manager is idle."""
    sh("adb shell settings put global hide_error_dialogs 1")
    deadline = time.time() + timeout
    while True:
        visible, _ = anr_dialog_visible()
        if visible:
            print("ANR/error dialog visible: dismissing", flush=True)
            sh("adb shell am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS")
            sh("adb shell input keyevent KEYCODE_BACK")
        else:
            act = subprocess.run("adb shell dumpsys activity activities", shell=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.DEVNULL, text=True, errors="replace").stdout
            busy = re.search(r"mSleeping=false.*?mHasPendingActivities=true|state=(?:INITIALIZING|PAUSING|STOPPING)\b"
                             r"|Application Not Responding", act, re.S)
            try:
                bc_idle = subprocess.run("adb shell am wait-for-broadcast-idle", shell=True, stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT, text=True, errors="replace",
                                         timeout=max(5, deadline - time.time())).returncode == 0
            except subprocess.TimeoutExpired:
                bc_idle = False
            if not busy and bc_idle:
                return True
        if time.time() > deadline:
            print(f"WARNING: device not idle after {timeout}s, continuing", flush=True)
            return False
        time.sleep(2)


def log_since(offset):
    """New logcat_full.txt text since a byte offset (the background logcat writes it)."""
    try:
        with open("logcat_full.txt", errors="replace") as f:
            f.seek(offset)
            return f.read()
    except OSError:
        return ""


def log_size():
    try:
        return os.path.getsize("logcat_full.txt")
    except OSError:
        return 0


def instrument(filter_args):
    """(output, systemui_anr_seen) of one `am instrument` run, waiting for idle first."""
    quiet_system_dialogs()
    wait_idle()
    sample_pids()
    start = log_size()
    visible_before, _ = anr_dialog_visible()
    out = sh(f"adb shell am instrument -w -r {filter_args} {RUNNER}", out="instrument.txt")
    sample_pids()
    visible_after, wdump = anr_dialog_visible()
    seen = bool(logcat_triage.SYSTEMUI_ANR.search(log_since(start) + "\n" + out)
                or ((visible_before or visible_after) and "com.android.systemui" in wdump))
    return out, seen


def run_with_retry(filter_args):
    """Results of one run; tests that failed with INJECT_EVENTS while a systemui ANR was seen get one retry."""
    out, sysui_anr = instrument(filter_args)
    res = parse(out)
    if not sysui_anr:
        return res
    for (c, t), (code, stack) in list(res.items()):
        if code in (0, -3) or not logcat_triage.is_inject_events_failure(stack):
            continue
        RETRIES.append(f"RETRY {c.split('.')[-1]}#{t}: INJECT_EVENTS + systemui ANR")
        print(RETRIES[-1], flush=True)
        r, _ = instrument(f"-e class {c}#{t}")
        r = parse(r)
        if (c, t) in r:
            res[(c, t)] = r[(c, t)]
            RETRIES[-1] += " -> " + ("passed" if r[(c, t)][0] == 0 else "failed again (stays failed)")
        else:
            RETRIES[-1] += " -> no result on retry (stays failed)"
    return res


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


def listing(filters=f"-e notAnnotation {BUGHUNT}"):
    out = sh(f"adb shell am instrument -w -r -e log true -e package {PKG} {filters} {RUNNER}")
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
    logcat = subprocess.Popen(f"adb logcat -v {LOGCAT_FORMAT}", shell=True, stdout=open("logcat_full.txt", "w"),
                              stderr=subprocess.STDOUT)

    results = {}
    only = os.environ.get("SWEEP_CLASS", "").strip()
    if only:
        results.update(run_with_retry(f"-e class {PKG}.{only}"))
        planned = list(results.keys())
    else:
        planned = listing()
        classes = []
        for cls, _ in planned:
            if cls not in classes:
                classes.append(cls)
        for cls in classes:
            results.update(run_with_retry(f"-e class {cls}"))
            missing = [(c, t) for (c, t) in planned if c == cls and (c, t) not in results]
            for c, t in missing:  # a crash ended the class run early: isolate the rest
                r = run_with_retry(f"-e class {c}#{t}")
                results.update(r)
                if (c, t) not in r:
                    results[(c, t)] = (-99, "no result (process crashed before the test reported)")

    os.makedirs("screenshots", exist_ok=True)
    sh("adb pull /data/local/tmp/pg_screenshots/. screenshots/")

    # Monkey: fixed seed, app package only, no system keys / app switches. System ANRs (systemui on
    # a slow emulator) are not the app's and must not abort the run; the app's own crashes and ANRs
    # are detected below.
    sh(f"adb shell am force-stop {PKG}")
    wait_idle()
    sample_pids()
    stop_sampling = [False]

    def sampler():
        while not stop_sampling[0]:
            sample_pids()
            time.sleep(3)
    import threading
    th = threading.Thread(target=sampler, daemon=True)
    th.start()
    monkey = sh(f"adb shell monkey -p {PKG} -s {SEED} --throttle 50 --pct-syskeys 0 --pct-appswitch 0 "
                f"--ignore-timeouts -v {EVENTS}", out="monkey.txt")
    stop_sampling[0] = True
    th.join(timeout=10)
    sh("adb shell screencap -p /data/local/tmp/monkey_end.png && adb pull /data/local/tmp/monkey_end.png screenshots/monkey_end.png")
    # ANR traces (google_apis images allow adb root): the app's main-thread stack goes to the log.
    sh("adb root && sleep 2 && mkdir -p anr && adb pull /data/anr/. anr/")
    anr_stacks = []
    for name in sorted(os.listdir("anr")) if os.path.isdir("anr") else []:
        text = open(os.path.join("anr", name), errors="replace").read()
        if f"Cmd line: {PKG}" not in text:  # only the app's own ANR traces, not systemui's
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
    lines.append("== annotation pass ==")
    lines += re.findall(r"annot(?:pass|cmd) .*", log)[:30]
    lines.append("== frame time ==")
    lines += re.findall(r"FRAMETIME .*", log)
    # native input-path timing of the long gestures (the timed strokes): tool vs. redraw per batch
    lines += [m for m in re.findall(r"toolstats .*", log) if int(re.search(r"batches=(\d+)", m).group(1)) >= 300]
    injected = re.findall(r"Events injected: (\d+)", monkey)
    app_crash = re.findall(r"// CRASH: " + re.escape(PKG) + r".*", monkey)
    app_anr = re.findall(r"// NOT RESPONDING: " + re.escape(PKG) + r".*", monkey)
    lines.append(f"== monkey (seed {SEED}, {EVENTS} events): injected {injected[-1] if injected else 0}, "
                 f"app crashes {len(app_crash)}, app ANRs {len(app_anr)} ==")
    lines += app_crash + app_anr
    lines += re.findall(r"// NOT RESPONDING: (?!" + re.escape(PKG) + r").*", monkey)[:3]
    log_lines = log.splitlines()
    app_fail, warn = logcat_triage.classify_logcat(log_lines, APP_PIDS, PKG)
    lines.append(f"== app pids sampled: " + ", ".join(f"{p} {n}" for p, n in sorted(APP_PIDS.items())) + " ==")
    lines.append(f"== logcat scan (app): {len(app_fail)} failures ==")
    lines += ["  FAIL " + x for x in app_fail[:60]]
    lines.append(f"== logcat scan (other processes, WARNINGS only): {len(warn)} ==")
    lines += ["  WARNING " + x for x in warn[:60]]
    lines.append("== signal 6 senders ==")
    lines += ["  " + x for x in logcat_triage.signal6_senders(log_lines, APP_PIDS, PKG)] or ["  (none)"]
    lines.append(f"== retries ({len(RETRIES)}) ==")
    lines += RETRIES
    gl_errors = len(re.findall(r"GL_INVALID|glGetError|EGL_BAD", log))
    lines.append(f"GL errors (all processes): {gl_errors}")
    lines += anr_stacks[:3]
    for m in re.finditer(r"backtrace:\n((?:.*DEBUG.*#\d\d.*\n){1,12})", log):
        lines.append("  backtrace:\n" + m.group(1))
    summary = "\n".join(lines)
    open("sweep_summary.txt", "w").write(summary + "\n")
    print(summary)

    ok = (planned and not failed and len(passed) + len(skipped) == len(planned) and injected and int(injected[-1]) == EVENTS
          and not app_crash and not app_anr and not app_fail)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
