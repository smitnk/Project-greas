"""Unit tests for tools/logcat_triage.py (stdlib unittest, no device needed)."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.dont_write_bytecode = True
import logcat_triage as lt  # noqa: E402

PKG = "com.smitnk.projectgrease"

APP_ABORT = [
    "10-04 12:00:01.100  4321  4350 F libc    : Fatal signal 6 (SIGABRT), code -1 (SI_QUEUE) in tid 4350 (RenderThread), pid 4321 (smitnk.projectgrease)",
    "10-04 12:00:01.300  5000  5000 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***",
    "10-04 12:00:01.301  5000  5000 F DEBUG   : pid: 4321, tid: 4350, name: RenderThread  >>> com.smitnk.projectgrease <<<",
    "10-04 12:00:01.302  5000  5000 F DEBUG   : Abort message: 'stroke buffer overflow'",
]
SYSUI_ABORT = [
    "10-04 12:00:02.100   698   720 F libc    : Fatal signal 6 (SIGABRT), code -1 (SI_QUEUE) in tid 720 (hwuiTask1), pid 698 (com.android.systemui)",
    "10-04 12:00:02.300  5100  5100 F DEBUG   : pid: 698, tid: 720, name: hwuiTask1  >>> com.android.systemui <<<",
    "10-04 12:00:02.301  5100  5100 F DEBUG   : Abort message: 'systemui gave up'",
]


def fatal_exception(pkg, pid):
    return [
        f"10-04 12:00:03.000  {pid}  {pid} E AndroidRuntime: FATAL EXCEPTION: main",
        f"10-04 12:00:03.001  {pid}  {pid} E AndroidRuntime: Process: {pkg}, PID: {pid}",
        f"10-04 12:00:03.002  {pid}  {pid} E AndroidRuntime: java.lang.IllegalStateException: boom",
    ]


class ClassifyTest(unittest.TestCase):
    def test_app_native_abort_fails_naming_app(self):
        failures, warnings = lt.classify_logcat(APP_ABORT, {}, PKG)
        self.assertTrue(failures)
        self.assertTrue(any("Fatal signal" in f and "pid 4321" in f for f in failures))
        self.assertTrue(any(f.startswith("Abort message") and PKG in f for f in failures), failures)
        self.assertEqual(warnings, [])

    def test_systemui_abort_only_warns(self):
        failures, warnings = lt.classify_logcat(SYSUI_ABORT, {}, PKG)
        self.assertEqual(failures, [])
        self.assertTrue(any("com.android.systemui" in w for w in warnings))
        self.assertTrue(all("pid 698" in w for w in warnings), warnings)

    def test_fatal_exception_app_fails(self):
        failures, warnings = lt.classify_logcat(fatal_exception(PKG, 4321), [], PKG)
        self.assertEqual(len(failures), 1)
        self.assertIn(PKG, failures[0])
        self.assertEqual(warnings, [])

    def test_fatal_exception_other_package_warns(self):
        failures, warnings = lt.classify_logcat(fatal_exception("com.google.android.gms", 2222), [], PKG)
        self.assertEqual(failures, [])
        self.assertEqual(len(warnings), 1)
        self.assertIn("com.google.android.gms", warnings[0])

    def test_jni_and_asan_from_app_pid_fail(self):
        lines = [
            "10-04 12:00:04.000  4321  4321 F nkprojectgrease: java_vm_ext.cc:591] JNI DETECTED ERROR IN APPLICATION: use of deleted local reference",
            "10-04 12:00:04.100  4321  4330 I wrap.sh : ==4321==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x0060",
        ]
        failures, warnings = lt.classify_logcat(lines, [4321], PKG)
        kinds = [f.split(" [")[0] for f in failures]
        self.assertEqual(kinds, ["JNI DETECTED ERROR", "AddressSanitizer"])
        self.assertEqual(warnings, [])

    def test_anr_systemui_warns(self):
        lines = ["10-04 12:00:05.000   540   560 E ActivityManager: ANR in com.android.systemui"]
        failures, warnings = lt.classify_logcat(lines, [], PKG)
        self.assertEqual(failures, [])
        self.assertEqual(len(warnings), 1)
        self.assertIn("com.android.systemui", warnings[0])

    def test_anr_app_fails(self):
        lines = ["10-04 12:00:05.000   540   560 E ActivityManager: ANR in com.smitnk.projectgrease (com.smitnk.projectgrease/.MainActivity)"]
        failures, warnings = lt.classify_logcat(lines, [], PKG)
        self.assertEqual(len(failures), 1)
        self.assertEqual(warnings, [])


class Signal6SendersTest(unittest.TestCase):
    def test_lists_app_and_systemui(self):
        out = lt.signal6_senders(APP_ABORT + SYSUI_ABORT, {}, PKG)
        self.assertTrue(any(s.startswith("pid 4321 smitnk.projectgrease") for s in out), out)
        self.assertTrue(any(s.startswith("pid 4321 com.smitnk.projectgrease") and "Abort message" in s for s in out), out)
        self.assertTrue(any(s.startswith("pid 698 com.android.systemui") for s in out), out)
        self.assertEqual(len(out), 4)


class InjectEventsTest(unittest.TestCase):
    def test_positive(self):
        self.assertTrue(lt.is_inject_events_failure(
            "java.lang.SecurityException: Injecting to another application requires INJECT_EVENTS permission\n\tat ..."))

    def test_negative(self):
        self.assertFalse(lt.is_inject_events_failure("java.lang.AssertionError: expected 1"))
        self.assertFalse(lt.is_inject_events_failure(None))


if __name__ == "__main__":
    unittest.main()
