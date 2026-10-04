# Project Grease status

Last updated: 2026-10-04 (branch `fix/ci-emulator-gate`, base `71fae48`).

## What works

Source: `android/app/src/main/java/com/smitnk/projectgrease/editor/FeatureRegistry.kt`.

All 166 features in the registry are `wired`: state `IN_PROGRESS`, audit `IN_PROGRESS`,
`deviceVerified = false`. The code path from Android input to the native Blender GP data exists
for every one, but **none is marked verified on a physical device**, and none is
`NOT_IMPLEMENTED` or `BLOCKED`. The emulator sweep (below) exercises them; it does not change
their registry state.

| Area | Features | State |
|---|---|---|
| Drawing | 14 | in progress (wired, not device-verified) |
| Selection | 15 | in progress |
| Editing | 13 | in progress |
| Layers and animation | 27 | in progress |
| Paint, materials, sculpt | 29 | in progress |
| View | 7 | in progress |
| Modifiers and effects | 49 | in progress |
| Persistence and export | 12 | in progress |

The per-feature table (area | feature | state | evidence) is in
`docs/PROJECT_GREASE_AUDIT_2026-10-02.md`, section "Update 2026-10-04".

## Test and CI gates

Workflow `.github/workflows/android-shell.yml`:

- **android-shell** job: imports pinned Blender source, checks verbatim regions, runs the native
  host tests (`tools/run_native_*_tests.sh`: primitives, selection, eraser, edit, annotation,
  Scene-lite, Line Art vs. Blender 3.6.23, software-GLES render, modifiers, shader FX, ...), then
  `gradle :app:testDebugUnitTest` and `gradle :app:assembleDebug :app:assembleDebugAndroidTest`.
- **emulator-tests** job (API 30, x86_64, 120 min): `tools/run_emulator_sweep.py` runs every
  instrumented test class, a fixed-seed monkey (`-s 20240501 --throttle 50 -v 20000`), frame-time
  measurements and a logcat scan.

## Latest results (PR #44, merged as `71fae48`)

- Instrumented tests: **210/210 passed**.
- Monkey, 20,000 events: **0 app crashes, 0 app ANRs**.
- Frame time: `doc_200_strokes` median 28.79 ms, p95 38.15 ms; `long_stroke_5000` p95 33.92 ms.
  Target is 32 ms.

Bugs fixed in PR #44 (from the commit log):

- Annotations: the open-stroke marker was bit 30 of `bGPDstroke.flag`, which is a `short`, so it
  was lost and annotation strokes drew nothing. Now bit 5, with a compile-time check.
- Presenter: `present_reset()` releases the open-stroke cache's GL objects; a stale texture from
  an old EGL context forced a full redraw per input sample (104 ms/sample).
- Presenter: GL sticky error flags cleared before cache store/reuse and the annotation pass.
- Fill: a fill that creates nothing no longer leaves the material's fill switched on without an
  undo step.
- Fill: strokes were placed in view pixels instead of canvas units.
- Timeline: the frame strip is a `LazyRow` (it composed up to 100,000 cells; monkey ANR).
- Deleting an empty keyframe was reported as a failure.
- Drawing over a full frame no longer rebuilds Blender's batch cache per sample; committed
  strokes are cached in a texture while a stroke is open (200-stroke median was 123 ms).
- Undo: material colour/fill/name/texture edits and the first Weight Paint vertex group are now
  undo steps; selection is restored after undo/redo.
- Rotation no longer recreates the activity and loses the open document.
- Sweep harness hides system ANR dialogs that stole input focus.

## Known issues / in flight

1. **CI emulator health.** A later run (37216872004, on `71fae48`) failed with about 60
   `INJECT_EVENTS` SecurityExceptions while `com.android.systemui` was not responding. This is an
   emulator problem, not an app failure; a health gate is being added on `fix/ci-emulator-gate`.
2. **200-stroke drawing p95 is 38 ms**, above the 32 ms target. Fix in flight on
   `test/bughunt-env` (PR #45, smitnk/Project-greas#45).
3. **Multiply and Extrude are slow on long self-crossing strokes** (re-triangulation). Not fixed.
4. **Bug-hunt workflow** (ASan / GL-debug matrix) is being brought up on PR #45 and has not
   produced emulator results yet.
5. No feature is verified on a physical device yet (see the device checklist in the audit doc).

## Run tests locally

```sh
bash tools/import_blender_gp.sh            # once: pinned Blender source
bash tools/run_native_edit_tests.sh        # any tools/run_native_*_tests.sh (host, no device)
(cd android && gradle :app:testDebugUnitTest)   # JVM unit tests
(cd android && gradle :app:assembleDebug :app:assembleDebugAndroidTest)
python3 tools/run_emulator_sweep.py        # needs a running emulator/device on adb (API 30)
```

The native render tests need software GLES (Mesa EGL); see the workflow for the packages.
