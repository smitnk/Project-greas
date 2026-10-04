# 01 — Full emulator bug hunt (branch `test/full-emulator-sweep`, PR #44)

## Goal
Drive every wired FeatureRegistry feature through the real UI on the API 30 x86_64 emulator,
assert both native document state (JNI read APIs) and the screen (pixel checks), add robustness
runs (monkey, rotation, background/foreground, trim-memory, undo/redo, 5,000-point stroke,
200-stroke frame time), fix every failure at its root cause and keep the tests as regressions.
No new features.

## What we built
- **Instrumented sweep (209 tests)** in `android/app/src/androidTest/java/com/smitnk/projectgrease/`:
  - `SweepBase.kt` — drag/tap with stylus pressure, `timedStroke` + `median`, `signature()`
    (document JSON minus frame), `undoable` / `undoableGesture`, screenshot + pixel helpers
    (`shot`, `inkNear`, `assertInk`, `countPixels`, `colorsNear`), a RESULT watcher saving a
    screenshot per test, and closing system dialogs before each test.
  - `SweepDrawTest`, `SweepEditTest`, `SweepPaintTest`, `SweepLayersFramesTest`,
    `SweepModsMaterialsTest`, `SweepIoTest`, `SweepRobustnessTest` (+ existing `DeviceBugfixTest`).
- **CI driver** `tools/run_emulator_sweep.py` (emulator-tests job, 120 min): runs each class,
  isolates tests a crash left unreported, fixed-seed monkey (`-s 20240501 --throttle 50 -v 20000`),
  pulls the app's ANR traces, captures full logcat, and prints a summary (failures, FRAMETIME,
  toolstats, annotpass, monkey, logcat scan). Artifacts: screenshots/, instrument.txt,
  logcat*.txt, monkey.txt, sweep_summary.txt, anr/.

## Bugs fixed (root cause → fix)
| Area | Bug | Root cause | Fix |
|---|---|---|---|
| Materials | Native crash on material delete after undo | Undo snapshots shared material pointers | Deep-copy/free materials in history snapshots |
| Layers | Wrong active layer after undo | `select_layer` did not set BKE active layer | Call `BKE_gpencil_layer_active_set` |
| Stroke ops | Random-colour etc. not legacy-geometry ops | Bridge op range too short | Range extended to RandomColor |
| Transforms | One gesture = many undo steps | No batching | `beginBatch/endBatch` around move/rotate/scale/mirror |
| Locked layers | Draw/fill/primitives edited locked layers | No editability gate | `activeLayerEditable()` gates them |
| Frames | Duplicate/delete/interpolate skipped undo | UI bypassed controller | Controller wrappers used by UI; `delete_frame` ignores BKE return on empty frame |
| Export | PNG export ignored background | No export background | `present_set_export_background` + `exportCanvasPixels` |
| Persistence | Stroke caps lost on save/load | Not serialized | caps in PGStrokeInfo/JNI/StrokeRecord/codec |
| Materials | Colour/fill/name/texture changes not undoable | No `markEdit` | Undo steps + texture extras hooks |
| Weight paint | First vertex group not undoable | Missing history record | Record step |
| Edit | Selection lost after undo | Not restored | `PG_DOC_Q_SELECTED_STROKES` + `restoreSelectedStroke()` |
| Robustness | Activity recreated on rotation | No configChanges | `android:configChanges` in manifest |
| Fill | Fill polygon offset | Outline not mapped back through canvas map | `set_fill_screen_map` from the fill JNI |
| Performance | 164 ms/sample drawing over 200 strokes | DRW batch cache rebuilt per sample + full redraw | Skip DRW cache while sbuffer open; open-stroke texture cache |
| Performance/annotations | Cache invalid / annotation draw failing (suspected) | Sticky `glGetError` from earlier calls | `clear_gl_errors()` before cache store/reuse and annotation pass |
| CI harness | Most UI tests: INJECT_EVENTS SecurityException | Emulator's System UI ANR dialog took focus | `hide_error_dialogs 1` + `CLOSE_SYSTEM_DIALOGS` per run and per test |

## Decisions and why
- **Tests follow Blender semantics.** Where a test disagreed with Blender 3.6 behaviour, the
  test was corrected (with a stronger assertion), not the engine: weight blur/average/smear only
  redistribute weight; `is_leak_narrow` closes gaps in straight walls, not corners (added a
  leak=1 control that must leak); texture `mix 1` = material colour (added mix 0 and mix 1 checks);
  finger pressure is 1 by design (stylus events used for pressure tests).
- **Evidence via job log.** Artifact downloads are blocked by the proxy here, so the sweep prints
  everything needed (failures, frame times, native timing, annotation pass) into the log.
- **Logging guarded with `__has_include(<android/log.h>)`** because host native tests define
  `__ANDROID__` without NDK headers.

## Results so far
- Run 3: 195/209. Run 4: blocked by system dialogs. Run 5: **202/209**, monkey 0 app crashes /
  0 app ANRs, logcat 0 fatal/JNI/abort/ASan/GL errors.
- Frame time: 5,000-point stroke median 0.06 ms (p95 27.5 ms); 200-stroke document median
  87.8 ms (was 164 ms) — still above the 32 ms limit at run 5.

## Files changed (main ones)
- Tests: `android/app/src/androidTest/.../Sweep*.kt`, `ProjectDocumentRoundTripTest`.
- CI: `tools/run_emulator_sweep.py`, emulator-tests workflow.
- Kotlin: `editor/EditorControllers.kt`, `nativebridge/ProjectGreaseEglSurface.kt`, `AndroidManifest.xml`.
- Native: `native/blender_gp/project_grease_gp_backend.cpp`, `android_gp_presentation.cpp`,
  `project_grease_gp_bridge.cpp`, `project_grease_gp_jni.cpp`, `android/app/src/main/cpp/project_grease_android_egl_renderer.cpp`.
- Native tests: `test_backend_modifier_stack.cc`, `test_edit8.cc`, `test_render.cc`.

## Next steps
1. Read run 6 (`ebb9d40`): check `toolstats` (cache_reuses vs. stores, render_ms) and
   `annotpass` lines; fix frame time to ≤ 32 ms median and the annotation draw at root cause.
2. Confirm weight/fill-leak/texture tests pass with the corrected expectations.
3. When android-shell and emulator-tests are both green: write the final report table into the
   PR description (area | feature | test | result | bug | root cause | fix | screenshot), then merge.
