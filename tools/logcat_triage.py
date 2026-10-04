"""Pure logcat triage for the emulator sweep: app findings fail, other processes' findings warn.

Input lines are `adb logcat -v threadtime` lines:
    MM-DD HH:MM:SS.mmm  PID  TID PRI TAG: message
Pids belong to the app when they were sampled with `pidof` during the run or named by a crash /
tombstone line (see app_pids_from_log). Everything here is side-effect free so it can be unit tested.
"""
import re

THREADTIME = re.compile(r"^\d\d-\d\d\s+\d\d:\d\d:\d\d\.\d+\s+(\d+)\s+(\d+)\s+([VDIWEFA])\s+(.*?)\s*:\s?(.*)$")
# tombstone / debuggerd: "pid 123, tid 456, name RenderThread  >>> com.x <<<" (and "pid: 123, tid: 456, name: X  >>> com.x <<<")
TOMBSTONE = re.compile(r"pid:?\s*(\d+),\s*tid:?\s*(\d+),\s*name:?\s*(.*?)\s+>>>\s*(\S+)\s*<<<")
FATAL_SIGNAL = re.compile(r"Fatal signal (\d+).*?\(([^)]*)\),.*?in tid (\d+) \(([^)]*)\),\s*pid (\d+) \(([^)]*)\)")
PROCESS_PID = re.compile(r"Process: (\S+), PID: (\d+)")
NOT_RESPONDING = re.compile(r"(?:// )?NOT RESPONDING: (\S+) \(pid (\d+)\)")
ANR_IN = re.compile(r"ANR in (\S+)")

PATTERNS = [
    ("Fatal signal", re.compile(r"Fatal signal")),
    ("Abort message", re.compile(r"Abort message")),
    ("JNI DETECTED ERROR", re.compile(r"JNI DETECTED ERROR")),
    ("AddressSanitizer", re.compile(r"AddressSanitizer")),
    ("FATAL EXCEPTION", re.compile(r"FATAL EXCEPTION")),
    ("ANR", re.compile(r"ANR in \S+")),
]


def parse_line(line):
    """(pid, tag, message) of a threadtime line, or (None, None, line) for anything else."""
    m = THREADTIME.match(line)
    if not m:
        return None, None, line
    return int(m.group(1)), m.group(4), m.group(5)


def is_app(name, pkg):
    if not name:
        return False
    # Fatal signal lines carry the kernel comm, truncated to its last 15 chars ("smitnk.projectgrease")
    return (name == pkg or name.startswith(pkg + ".") or name.startswith(pkg + ":")
            or (len(name) >= 15 and (pkg.endswith(name) or (pkg + ".test").endswith(name))))


def app_pids_from_log(lines, pkg):
    """{pid: process name} for app pids named by tombstone / Fatal signal / FATAL EXCEPTION lines."""
    pids = {}
    for line in lines:
        _, _, msg = parse_line(line)
        for m in TOMBSTONE.finditer(msg):
            if is_app(m.group(4), pkg):
                pids[int(m.group(1))] = m.group(4)
        m = FATAL_SIGNAL.search(msg)
        if m and is_app(m.group(6), pkg):
            pids[int(m.group(5))] = m.group(6)
        m = PROCESS_PID.search(msg)
        if m and is_app(m.group(1), pkg):
            pids[int(m.group(2))] = m.group(1)
    return pids


def process_names(lines):
    """{pid: name} for every process a log line names (tombstones, Fatal signal, Process:, NOT RESPONDING)."""
    names = {}
    for line in lines:
        _, _, msg = parse_line(line)
        for m in TOMBSTONE.finditer(msg):
            names[int(m.group(1))] = m.group(4)
        m = FATAL_SIGNAL.search(msg)
        if m:
            names[int(m.group(5))] = m.group(6)
        m = PROCESS_PID.search(msg)
        if m:
            names[int(m.group(2))] = m.group(1)
        m = NOT_RESPONDING.search(msg)
        if m:
            names[int(m.group(2))] = m.group(1)
    return names


def _owner(line, msg, line_pid, block_owner, app_pids, names, pkg):
    """(pid, name, is_app) of the process a finding line is about."""
    m = FATAL_SIGNAL.search(msg)
    if m:
        pid, name = int(m.group(5)), m.group(6)
        return pid, name, is_app(name, pkg) or pid in app_pids
    m = ANR_IN.search(msg)
    if m:
        name = m.group(1)
        pid_m = re.search(r"PID: (\d+)", msg)
        pid = int(pid_m.group(1)) if pid_m else None
        return pid, name, is_app(name, pkg)
    if block_owner is not None:
        pid, name = block_owner
        return pid, name, is_app(name, pkg) or pid in app_pids
    if line_pid is not None:
        name = app_pids.get(line_pid) or names.get(line_pid) or "?"
        return line_pid, name, line_pid in app_pids or is_app(name, pkg)
    return None, "?", False


def classify_logcat(lines, app_pids, pkg):
    """Split crash/abort/ANR findings into (failures, warnings).

    failures: findings from the app (pid in app_pids or process name pkg / pkg.*).
    warnings: the same kinds of findings from any other process (systemui, ...).
    Each entry is "<kind> [pid N <name>]: <line>". app_pids is {pid: name} or an iterable of pids.
    """
    lines = [l.rstrip("\n") for l in lines]
    if not isinstance(app_pids, dict):
        app_pids = {int(p): pkg for p in app_pids}
    app_pids = dict(app_pids)
    app_pids.update(app_pids_from_log(lines, pkg))
    names = process_names(lines)
    failures, warnings = [], []
    # A FATAL EXCEPTION block is logged by AndroidRuntime from the crashing process; its
    # "Process: X, PID: N" line comes right after, so resolve the owner from the next few lines.
    # debuggerd ("DEBUG" tag) lines belong to the process named in the tombstone header above them.
    tomb_owner = None
    for i, line in enumerate(lines):
        line_pid, tag, msg = parse_line(line)
        t = TOMBSTONE.search(msg)
        if t:
            tomb_owner = (int(t.group(1)), t.group(4))
        elif tag is not None and tag != "DEBUG" and "Fatal signal" not in msg:
            tomb_owner = None
        for kind, pat in PATTERNS:
            if not pat.search(msg):
                continue
            block_owner = tomb_owner if tag == "DEBUG" or line_pid is None else None
            if kind == "FATAL EXCEPTION":
                for nxt in lines[i + 1:i + 4]:
                    m = PROCESS_PID.search(nxt)
                    if m:
                        block_owner = (int(m.group(2)), m.group(1))
                        break
            pid, name, app = _owner(line, msg, line_pid, block_owner, app_pids, names, pkg)
            entry = f"{kind} [pid {pid if pid is not None else '?'} {name}]: {line.strip()[:400]}"
            (failures if app else warnings).append(entry)
            break
    return failures, warnings


def signal6_senders(lines, app_pids, pkg):
    """Every Fatal signal 6 / Abort message line with the pid and name of its process."""
    lines = [l.rstrip("\n") for l in lines]
    if not isinstance(app_pids, dict):
        app_pids = {int(p): pkg for p in app_pids}
    names = process_names(lines)
    names.update(app_pids)
    out, tomb_owner = [], None
    for line in lines:
        line_pid, tag, msg = parse_line(line)
        t = TOMBSTONE.search(msg)
        if t:
            tomb_owner = (int(t.group(1)), t.group(4))
        m = FATAL_SIGNAL.search(msg)
        if m and m.group(1) == "6":
            out.append(f"pid {m.group(5)} {m.group(6)}: {line.strip()[:400]}")
        elif "Fatal signal 6" in msg or "Abort message" in msg:
            pid, name = tomb_owner if (tomb_owner and (tag == "DEBUG" or line_pid is None)) else \
                (line_pid, names.get(line_pid, "?"))
            out.append(f"pid {pid if pid is not None else '?'} {name}: {line.strip()[:400]}")
    return out


SYSTEMUI_ANR = re.compile(r"NOT RESPONDING: com\.android\.systemui|ANR in com\.android\.systemui|"
                          r"Application Not Responding: com\.android\.systemui")
INJECT_EVENTS = "Injecting to another application requires INJECT_EVENTS permission"


def is_inject_events_failure(stack):
    return INJECT_EVENTS in (stack or "")
