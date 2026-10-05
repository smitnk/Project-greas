# Summary since the last checkpoint (PR #45 bug-hunt follow-up)

## CI runs read
| Run / head | Cell | Result |
|---|---|---|
| 37229710509 / 97770bd | API 26 tablet | 28–29 failures, mostly "stroke not visible" |
| 37229710509 / 97770bd | API 34 phone (ASan) | Report crashed: `MemoryError` on a ~450 MB logcat after the emulator died |
| 37229710509 / 97770bd | API 30 phone/tablet, API 34 tablet | Cancelled at the 340-min job limit |
| 37254377799 / 5251910 | API 26 phone | 6 failures |
| 37254377799 / 5251910 | API 26 tablet | 28 failures (same pattern) |
| 37254377799 / 5251910 | API 34 phone (ASan) | 213/217 passed; soak crashed; monkey injected 0 events |
| 37270873638 / 695a962 | android-shell, build, emulator-tests | **Green** (first CI compile of the Kotlin changes) |
| 37270873638 / 695a962 | API 34 phone | 218/220 passed |
| 37270873638 / 695a962 | API 30 phone | 218/220 passed |
| 37270873638 / 695a962 | API 34 tablet | 216/220 passed (ink failures gone at native resolution) |
| 37270873638 / 695a962 | API 26 phone | **218/219 passed**; frame p95 29.6 ms; 0 monkey crashes or ANRs; clean logcat |
| 37270873638 / 695a962 | API 26 and API 30 tablet | Still running when stopped |

## Bugs fixed (app)
- **Lost edits after the app was killed in the background** (`ProjectGreaseUI.kt`, commit 71b8ee0, pushed). The app saved only on a 5 s timer and on Back, so a kill in the background lost recent edits. It now also saves at `ON_STOP`.
- **Save wrote no file once the surface was gone** (`EditorControllers.kt`, `ProjectGreaseUI.kt`, commit 6a37797, **unpushed**).
  - Cause: `saveDocumentJson()` returns null after the surface detaches, yet the project record was still written and marked saved. Reopening gave an empty default document.
  - Fix: a new `persistableDocumentJson()` falls back to the snapshot taken at detach. If there is nothing to save, the record is skipped.
- **Trim missed self-crossings that land exactly on a sample point** (commit d6cefda, pushed).
  - Cause: Blender's `BKE_gpencil_stroke_trim` only accepts crossings strictly inside both segments (0 < λ < 1).
  - Fix: a new `project_grease_stroke_trim.c/.h` (`pg_gpencil_stroke_trim`) keeps Blender's trim first. Only if that finds nothing does it search again with segment ends included.
  - Wiring: added to `tools/android_gp_source_manifest.txt`, `tools/native_host_closure.sh`, `native/blender_gp/CMakeLists.txt` and the backend.
  - Tests: `tests/test_stroke_trim.c` and `tools/run_native_stroke_trim_tests.sh`, plain and ASan/UBSan, passed locally. Also added as an android-shell CI step, plus a case in `gp_backend_link_test.cpp`.

## Test and harness fixes
- **`topBarIsBelowTheStatusBar`:** below API 30 it hard-coded the inset to 0. It now reads `systemWindowInsetTop`.
- **MP4 export tests:** they used API 28+ calls. A new `Mp4Checks.kt` counts frames with `MediaExtractor` and grabs a frame with `getFrameAtTime` below API 28.
- **`frameTime200Strokes`:** rows went down to y = 932 on a 720-high canvas, so 44 strokes landed off-canvas on tablets. Rows are now 12 px apart (commit 53624a6, **unpushed**).
- **`run_bughunt.py`:**
  - Logcat is read line by line, which fixes the `MemoryError`. `memory_graph.py` streams too.
  - Tablet resolution is 2560x1800, the pixel_c's native size. The 2560x1600 override changed the aspect ratio and caused the tablet ink failures.
  - The soak reruns without the LeakCanary listener if the listener run gives no result.
  - A monkey run that injects fewer events than requested now counts as a failure.
  - ASan cells reinstall the plain APKs for the performance budgets (commit b0d8ce1, **unpushed**).
- **`bughunt.yml`:**
  - Timeout raised from 340 to 355 min.
  - ASan cells run 1000 fuzz iterations; API 26 runs 3000.
  - Plain APKs are copied to `apks-plain/` (commit b0d8ce1, **unpushed**).
- **`androidTest/AndroidManifest.xml`:** removes LeakCanary's providers from the test APK. They crashed 218 times with a missing Kotlin `Intrinsics` class.

## Decisions and why
- I held each push until running hunt cells finished, because a push cancels the in-progress run and I wanted their results.
- I didn't patch the vendored Blender checkout. I wrote a fallback module so the change lives in project code and runs in the existing host tests.
- Performance budgets are measured on the plain build because ASan timings don't reflect the real app. The budget assertions themselves are unchanged.
- No test thresholds were loosened. The test fixes only make the tests measure what they claim to on every API level and device.

## Open findings (not fixed)
- **Playback median is 70–116 ms** against a 16 ms budget. This shows on the plain API 26 build as well, so it isn't just ASan overhead.
- **LeakCanary listener won't load:** `FailTestOnLeakRunListener` gives "Could not find extra class". The soak fallback works, but runs without leak detection.
- **StrictMode flags main-thread disk I/O** in `ProjectFiles`, the save/load path, startup and `attachRenderer`.
- **API 30 ASan monkey runs hit 3 input-dispatch ANRs**, likely from slow redraws (~370 ms on the 200-stroke document).
- **ASan soak memory grows 10–24 MB** while native heap stays flat. It may be the sanitizer's freed-memory quarantine; not confirmed.
- **`primitiveRectangle` failed once on API 26 phone** after passing on the previous run there. Possibly flaky; not investigated.

## State when stopped
- At your request I cancelled the scheduled check-in and unsubscribed from PR #45.
- Three local commits are **not pushed**: b0d8ce1, 53624a6 and 6a37797.

## Next steps
1. Push the 3 commits. This cancels the two tablet cells still running.
2. Read the next hunt run, especially the tablets and process death after 6a37797.
3. Update the PR #45 report table.
4. Open issues for the open findings above.
5. Commit golden images from the generated candidates, which needs your approval.
