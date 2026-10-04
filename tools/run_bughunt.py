#!/usr/bin/env python3
"""Bug-hunt driver for one emulator cell (.github/workflows/bughunt.yml).

Per cell (API level x device x GPU): configure the screen, install the hunting APK (ASan + GL error
checks + StrictMode where the API level allows wrap.sh, the plain debug APK otherwise), then
  1. every Sweep* test (same driver as the PR sweep: crashed classes are re-run test by test),
  2. the on-device fuzzers (edit commands, project JSON, SVG import, image trace),
  3. the frame-time budget with a Perfetto trace (API 28+),
  4. the soak (SOAK_MINUTES) with LeakCanary's fail-on-leak listener and memory sampling,
  5. the monkey: MONKEY_SEEDS x MONKEY_EVENTS events,
  6. process death: draw, background, `am kill`, relaunch, the drawing must be there,
then compare the screenshots with the cell's goldens and scan logcat for crashes, ANRs, ASan
reports, GL errors (PG_GL_ERROR), StrictMode violations and leaks. Everything goes to out/<cell>/
and a summary to the job log; the exit code is non-zero when anything failed.

Env: CELL, API, DEVICE (phone|tablet), ASAN (1|0), SOAK_MINUTES, FUZZ_ITERATIONS, MONKEY_SEEDS
(comma list), MONKEY_EVENTS, MONKEY_THROTTLE.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run_emulator_sweep as sweep  # noqa: E402

PKG, RUNNER, sh = sweep.PKG, sweep.RUNNER, sweep.sh
CELL = os.environ.get("CELL", "local")
API = int(os.environ.get("API", "30"))
DEVICE = os.environ.get("DEVICE", "phone")
ASAN = os.environ.get("ASAN", "0") == "1"
SOAK_MINUTES = os.environ.get("SOAK_MINUTES", "30")
FUZZ_ITERATIONS = os.environ.get("FUZZ_ITERATIONS", "3000")
MONKEY_SEEDS = [s.strip() for s in os.environ.get("MONKEY_SEEDS", "20240501,7,1234").split(",") if s.strip()]
MONKEY_EVENTS = int(os.environ.get("MONKEY_EVENTS", "50000"))
MONKEY_THROTTLE = os.environ.get("MONKEY_THROTTLE", "20")
OUT = os.path.join("out", CELL)
HUNT = PKG + ".bughunt."
SCREENS = {"phone": ("1080x2400", "420"), "tablet": ("2560x1600", "320")}


def class_tests(cls):
    out = sh(f"adb shell am instrument -w -r -e log true -e class {cls} {RUNNER}")
    tests = []
    for b, code in sweep.blocks(out):
        k = (b.get("class"), b.get("test"))
        if code == 1 and k[0] and k[1] and k not in tests:
            tests.append(k)
    return tests


def run_class(cls, extra=""):
    """Runs one test class; tests a crash left without a result are run one by one."""
    planned = class_tests(cls)
    results = sweep.parse(sweep.instrument(f"{extra} -e class {cls}"))
    for c, t in planned:
        if (c, t) not in results:
            r = sweep.parse(sweep.instrument(f"{extra} -e class {c}#{t}"))
            results.update(r)
            if (c, t) not in r:
                results[(c, t)] = (-99, "no result (process crashed before the test reported)")
    return results


def monkey(seed):
    sh(f"adb shell am force-stop {PKG}")
    out = sh(f"adb shell monkey -p {PKG} -s {seed} --throttle {MONKEY_THROTTLE} --pct-syskeys 0 --pct-appswitch 0 "
             f"--ignore-timeouts -v {MONKEY_EVENTS}", out=os.path.join(OUT, f"monkey_{seed}.txt"))
    injected = re.findall(r"Events injected: (\d+)", out)
    crash = re.findall(r"// CRASH: " + re.escape(PKG) + r".*", out)
    anr = re.findall(r"// NOT RESPONDING: " + re.escape(PKG) + r".*", out)
    return f"monkey seed {seed}: injected {injected[-1] if injected else 0}/{MONKEY_EVENTS}, app crashes {len(crash)}, app ANRs {len(anr)}", bool(crash or anr)


def main():
    os.makedirs(OUT, exist_ok=True)
    for f in ["instrument.txt"]:
        open(f, "w").close()
    size, density = SCREENS.get(DEVICE, SCREENS["phone"])
    sh(f"adb shell wm size {size}")
    sh(f"adb shell wm density {density}")
    sh("adb install -r -g apks/app.apk", check=True)
    sh("adb install -r -g apks/app-test.apk", check=True)
    sh("adb logcat -c")
    sh("adb logcat -G 16M")
    logcat = subprocess.Popen("adb logcat -v threadtime", shell=True,
                              stdout=open(os.path.join(OUT, "logcat_full.txt"), "w"), stderr=subprocess.STDOUT)
    summary = [f"== cell {CELL}: API {API}, {DEVICE} {size}@{density}dpi, {'ASan + GL debug + StrictMode' if ASAN else 'debug (no wrap.sh below API 27)'} =="]
    failed = []

    # 1. the sweep
    results = {}
    classes = []
    for c, _ in sweep.listing():
        if c not in classes:
            classes.append(c)
    for cls in classes:
        results.update(run_class(cls))
    # 2-4. the hunts
    results.update(run_class(HUNT + "FuzzDeviceTest", f"-e fuzzIterations {FUZZ_ITERATIONS}"))
    perfetto = None
    if API >= 28:
        cfg = ('buffers { size_kb: 65536 } data_sources { config { name: "linux.ftrace" ftrace_config { '
               'ftrace_events: "sched/sched_switch" atrace_categories: "gfx" atrace_categories: "view" '
               f'atrace_apps: "{PKG}" }} }} }} data_sources {{ config {{ name: "android.surfaceflinger.frametimeline" }} }} duration_ms: 600000')
        open(os.path.join(OUT, "perfetto.cfg"), "w").write(cfg)
        sh(f"adb push {OUT}/perfetto.cfg /data/local/tmp/perfetto.cfg")
        perfetto = subprocess.Popen("adb shell perfetto --txt -c /data/local/tmp/perfetto.cfg -o /data/misc/perfetto-traces/perf.pftrace",
                                    shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        time.sleep(3)
    results.update(run_class(HUNT + "PerfTest"))
    if perfetto:
        sh("adb shell pkill -INT perfetto")
        time.sleep(3)
        sh(f"adb pull /data/misc/perfetto-traces/perf.pftrace {OUT}/perf_{CELL}.pftrace")
    # LeakCanary's listener ships in the bug-hunt test APK only (the API 26 cell runs the plain one).
    leak = " -e listener leakcanary.FailTestOnLeakRunListener" if ASAN else ""
    results.update(run_class(HUNT + "SoakTest", f"-e soakMinutes {SOAK_MINUTES}{leak}"))

    # 5. monkey
    for seed in MONKEY_SEEDS:
        line, bad = monkey(seed)
        summary.append(line)
        if bad:
            failed.append(line)

    # 6. process death: draw + background, kill, relaunch
    results.update(run_class(HUNT + "ProcessDeathSetupTest"))
    sh(f"adb shell am kill {PKG}")
    sh(f"adb shell am force-stop {PKG}")
    results.update(run_class(HUNT + "ProcessDeathVerifyTest"))

    os.makedirs(os.path.join(OUT, "screenshots"), exist_ok=True)
    sh(f"adb pull /data/local/tmp/pg_screenshots/. {OUT}/screenshots/")
    if ASAN or API >= 27:
        sh(f"adb root && sleep 2 && mkdir -p {OUT}/anr {OUT}/tombstones && adb pull /data/anr/. {OUT}/anr/ && adb pull /data/tombstones/. {OUT}/tombstones/")
    time.sleep(2)
    logcat.terminate()

    # results
    passed = sorted(k for k, v in results.items() if v[0] == 0)
    bad = sorted((k, v) for k, v in results.items() if v[0] not in (0, -3))
    summary.append(f"== instrumentation: {len(results)} tests, {len(passed)} passed, {len(bad)} failed ==")
    for (c, t), (code, stack) in bad:
        first = "\n      ".join(stack.splitlines()[:4])
        line = f"FAIL {c.split('.')[-1]}.{t} (code {code}): {first}"
        summary.append(line)
        failed.append(line)

    log = open(os.path.join(OUT, "logcat_full.txt"), errors="replace").read()
    summary.append("== frame time / perf ==")
    summary += re.findall(r"(?:FRAMETIME|PERF) .*", log)
    summary += [m for m in re.findall(r"toolstats .*", log) if int(re.search(r"batches=(\d+)", m).group(1)) >= 300]
    summary.append("== soak ==")
    summary += re.findall(r"SOAK result .*", log)
    sh(f"python3 tools/memory_graph.py {OUT}/logcat_full.txt {OUT}/memory_{CELL}.svg 'Soak memory, {CELL}'")

    # logcat scan: every hit is a finding
    checks = [
        ("AddressSanitizer", r"AddressSanitizer|==\d+==ERROR"),
        ("GL error (PG_GL_ERROR)", r"PG_GL_ERROR"),
        ("Fatal signal", r"Fatal signal \d+"),
        ("FATAL EXCEPTION", r"FATAL EXCEPTION"),
        ("JNI DETECTED ERROR", r"JNI DETECTED ERROR"),
        ("ANR in app", r"ANR in " + re.escape(PKG)),
        ("StrictMode", r"StrictMode policy violation"),
        ("LeakCanary leak", r"HEAP ANALYSIS RESULT|LEAK FOUND|Application Leaks"),
    ]
    summary.append("== logcat scan ==")
    for name, pattern in checks:
        lines = [l for l in log.splitlines() if re.search(pattern, l)]
        summary.append(f"{name}: {len(lines)}")
        if lines:
            # the first hits with their context (stack / report) for the job log
            idx = log.find(lines[0])
            summary.append("  " + log[idx:idx + 2500].replace("\n", "\n  "))
            if name != "StrictMode" or any(PKG.replace(".", "/") in l or PKG in l for l in lines):
                failed.append(f"{name}: {len(lines)} in logcat")
    # StrictMode: the distinct violation sites of the app's own code
    sites = sorted(set(re.findall(r"at (com\.smitnk\.projectgrease\.[\w.$]+\([\w.]+:\d+\))", "\n".join(
        log[m.start():m.start() + 3000] for m in re.finditer("StrictMode policy violation", log)))))
    summary += [f"  StrictMode site: {s}" for s in sites[:30]]

    # goldens
    g = subprocess.run(f"python3 tools/golden_compare.py {OUT}/screenshots android/app/src/androidTest/goldens {OUT} {CELL}",
                       shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    summary.append(g.stdout.strip())
    if g.returncode != 0:
        failed.append(f"goldens differ in {CELL}")

    summary.append(f"== cell {CELL}: {'FAILED (' + str(len(failed)) + ')' if failed else 'clean'} ==")
    text = "\n".join(summary)
    print(text)
    open(os.path.join(OUT, "bughunt_summary.txt"), "w").write(text + "\n")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
