# Project Grease — Bundled Blender Legacy GP Engine Worklog

## Working rule

Project Grease must become a focused Android 2D drawing application backed by real Blender 3.6.23 Legacy Grease Pencil functionality.

Do NOT port the full Blender application.
Do NOT introduce OpenToonz or GL4ES.
Do NOT replace Blender GP algorithms with unrelated custom approximations when the Blender 3.6.23 implementation can be integrated.
Keep Android responsible for Activity/UI/input/EGL/GLES lifecycle.
Keep the existing Project Grease UI architecture.

## Repository baseline

Branch: feature/project-grease-ui-real-integration
Successful functional baseline: ac11bd459f81422a054708e12222cd8ea973d4c8
Last verified CI baseline before the current paint-buffer work: workflow run #375 (2026-09-29), Android GP build/verification and APK artifact succeeded.

## Current bundled-engine work

1. 81c65980bba385007e198e3b3d5db25da3927ad9
   Added this persistent worklog so decisions, errors, fixes, successes and next actions are not lost.

2. cb8004bd0226d0d8a349380533a81c0a5930e1d9
   Added `project_grease_gp_engine.h`, a single host-facing engine boundary over the real Legacy GP Backend. It is an adapter, not a second drawing engine.

3. 8954a56a50b2ae1fa450622339e1a7f526072414
   Added `project_grease_gp_engine.cpp` with an explicit capability map. A capability is marked integrated only when it is actually backed and exercised; unsupported entries remain false instead of being advertised as complete.

4. d0089a8b28de07710f39cf423af1071fb161dbaa
   Expanded the focused native CMake target to compile the pinned Blender Legacy GP editor sources under the Legacy GP editor tree and GP modifier sources, including Line Art sources, while retaining the minimal Android boundary.

## Why this is the correct direction

The previous adapter already used real Blender Legacy GP data and cache code but exposed only a subset of operations. The new bundled target now points directly at Blender's actual 3.6-era GP editor/modifier implementation instead of continuing to grow a collection of isolated Project Grease replacements.

The Blender source layout confirms the Legacy GP editor subsystem contains drawing/editing/fill/interpolation/armature and related operators, while GP modifiers contain the non-destructive modifier implementations. Newer Blender Grease Pencil 3 code must not be substituted for this pinned Legacy GP target.

## Error-solving procedure

For every CI/compiler/link/runtime failure:
1. Read the exact failure from the failing job/log.
2. Identify the owning Blender 3.6.23 source/header/symbol.
3. Determine whether it belongs to the GP closure or is an unrelated desktop dependency.
4. If it belongs to GP, integrate the smallest missing closure.
5. If it is desktop-only, keep it outside Android and introduce only a narrow adapter if the real GP algorithm requires one.
6. Never import the full Blender application graph just to silence an error.
7. Rebuild and verify the same bundled change.
8. Record the failure and fix here.

## Success procedure

When the bundle builds:
- verify the native GP tests;
- verify Android APK creation;
- verify the APK artifact;
- verify that the new source closure is actually linked/used;
- then continue expanding the same engine boundary with the next real Blender subsystem, rather than creating a new micro-phase.

## Permanent architecture rule

Android:
UI + input + lifecycle + EGL/GLES

Project Grease:
controller/JNI/engine boundary

Blender 3.6.23:
real Legacy GP data + drawing/editing/sculpt/paint/modifier/Line Art algorithms as their minimal dependency closure

Not included:
full Blender application, desktop UI, Python, full scene/3D viewport, GHOST-on-Android, OpenToonz, GL4ES, unrelated Blender subsystems.

## Modifier execution bundles already verified

The real Blender 3.6.23 Legacy GP modifier registry/kernel and modifier implementations are in the Android closure. The backend can create real `GpencilModifierData` objects in Blender's `Object::greasepencil_modifiers` ListBase and invoke real `deformStroke()` callbacks for the focused context. CI #375 succeeded through Android build, APK verification and artifact upload with Smooth -> Thickness -> Subdivide conformance.

These modifier paths remain a focused adapter around real Blender callbacks, not a claim that all desktop depsgraph/scene-dependent modifier evaluation is complete.

## Next bundle — real Legacy GP paint stroke buffer

### Starting verification

- Android CI run **#375 / 36604200725** for commit `2ce7c88d8585bfd736746e9fe90436f5c69b1fd1` completed successfully through Blender import, minimal DNA generation, Android GP build, APK verification and artifact upload.
- The modifier-stack conformance reached the Android build gate without introducing a new dependency failure.

### Upstream reference

The pinned Blender 3.6.23 `source/blender/editors/gpencil_legacy/gpencil_paint.c` uses the real `tGPspoint` transient stroke representation and the public `ED_gpencil_sbuffer_ensure()` editor utility. Its paint path accumulates screen-space points in `bGPdata.runtime.sbuffer` and later transfers them into a real `bGPDstroke`.

### Implementation

- `868d5fdc29142281d95ade28c61e88302219cda9`
  - Exposes a conformance accessor for the current real Blender Legacy GP stroke-buffer point count.
- `c658de1c0fb674ddcf502c073675d92b52a6bfd5`
  - Routes Project Grease stroke input through Blender's real `tGPspoint`/sbuffer representation using `ED_gpencil_sbuffer_ensure()`.
  - Android input is now stored in the same transient point representation used by the Legacy GP editor rather than using the custom pending vector as the authoritative buffer.
  - The existing pending vector is retained only as a temporary Android preview bridge until the presentation path consumes Blender's sbuffer directly.
  - Stroke commit still uses the real `BKE_gpencil_stroke_add()` and transfers Blender sbuffer point data into the real `bGPDstroke`.
  - The Blender sbuffer is explicitly cleared with `ED_gpencil_sbuffer_ensure(..., clear=true)` after commit.
- `8a5407a8824c0046fc271f7d12373915ce8b1e31`
  - Adds conformance that four input points reach Blender's sbuffer and that the buffer is empty after commit.

### Why this bundle is important

This is the first explicit connection of the Android stroke session to Blender 3.6.23's actual Legacy GP editor paint-buffer abstraction. It is still deliberately narrower than the complete desktop operator because the desktop operator's `tGPsdata` context requires Blender window/area/region/depsgraph/brush infrastructure that Project Grease intentionally does not port wholesale.

### Next continuation

Do not mark the paint/stroke subsystem complete yet. The next step is to move more of the pinned Blender 3.6.23 paint processing into this same focused path: brush settings, pressure/strength processing, active smoothing, stroke subdivision/simplification and final stroke commit, adding only the concrete BKE/ED dependencies required by those upstream functions. The eventual target remains the real Legacy GP drawing pipeline, followed by editing/sculpt/paint, animation, fill/material/onion, rendering/cache and Android GLES presentation.

## Paint-buffer linker correction — 2026-09-29

- Runs **#377, #378 and #379** reached the native link stage but failed on the same concrete symbol: `undefined symbol: ED_gpencil_sbuffer_ensure`.
- Commit `3ae3ff6eb5db763c3e06b48a9256ea8e1e7ad860` temporarily added the full pinned `source/blender/editors/gpencil_legacy/gpencil_utils.c` to obtain that symbol.
- Run **#380** then failed while compiling that whole desktop editor utility because it requires generated `RNA_prototypes.h`. This confirmed that importing the whole file would drag unrelated RNA/window-manager/editor infrastructure into the focused Android target.
- Commit `27cc0d85b7e9e8f3c7226b315a8dcfa24c15f425` therefore introduced `native/blender_gp/project_grease_legacy_sbuffer.c`, containing the exact pinned Blender 3.6.23 `ED_gpencil_sbuffer_ensure()` algorithm.
- Commit `ca3bbc78c2def0afe084d4e4b7284310861aff37` replaced the whole editor utility source in CMake with that focused extraction.

## #381/#382 correction — missing upstream constant in focused extraction

### Exact CI evidence

- **#381 / run ID 36607667861 / commit `27cc0d85b7e9e8f3c7226b315a8dcfa24c15f425`** failed during Android C compilation.
- **#382 / run ID 36607692992 / commit `ca3bbc78c2def0afe084d4e4b7284310861aff37`** failed at the same point.
- Blender import and minimal DNA generation both succeeded. The failure was isolated to the focused sbuffer source during the Android native build.
- Exact compiler error:
  `use of undeclared identifier 'GP_STROKE_BUFFER_CHUNK'`
  at the three uses inside `project_grease_legacy_sbuffer.c`.

### Root cause

The extracted function was copied from Blender 3.6.23 `gpencil_utils.c`, but the constant was originally provided by the private editor header `source/blender/editors/gpencil_legacy/gpencil_intern.h`:

```
#define GP_STROKE_BUFFER_CHUNK 2048
```

The focused extraction intentionally did not include `gpencil_intern.h` because that header pulls `ED_numinput.h` and belongs to the broader desktop editor closure. The function therefore lost one exact compile-time dependency even though its algorithm was otherwise correct.

### Fix

Commit **`0211e68076f96863dc1e9bfe29a261bd2764bb0f`** updates `native/blender_gp/project_grease_legacy_sbuffer.c` with the exact upstream constant:

```
#define GP_STROKE_BUFFER_CHUNK 2048
```

It is documented as an exact 3.6.23 `gpencil_intern.h` constant, kept local specifically to avoid importing the unrelated desktop header dependency closure.

No drawing algorithm was changed. No custom stroke algorithm was introduced.

### Next verification

Run the Android Shell workflow from the new commit. If it fails, continue from the first concrete compiler/linker/runtime error only. If it passes, verify APK + conformance and then continue the real Legacy GP paint path: brush settings, pressure/strength processing, smoothing/subdivision/simplification, and final commit behavior. Do not mark the paint subsystem complete until those real Blender paths are exercised.


### Bulk Legacy GP geometry integration — 2026-09-29

Implemented a single batched engine entry point for Blender 3.6.23 Legacy GP geometry operations instead of adding each operation as a separate Project Grease geometry algorithm.

- f650466 — added Backend::LegacyGeometryOp and apply_legacy_geometry_batch() to the native engine API.
- e4d6858 — dispatches the batch directly to Blender Legacy GP BKE algorithms: adaptive/fixed simplify, subdivide, intersection trim, point trim, merge-distance, resample/sample, strength/thickness/UV smoothing, stretch, close, dissolve, and fill triangulation.
- 2f66152 / 14171ce — exposed the batch through the Android-safe C bridge.
- 923545b — added native conformance coverage for the batched callbacks, including a separate self-intersection trim case.

This batch is deliberately Blender-backed: the Project Grease layer supplies operation parameters and lifecycle/cache invalidation, while the actual geometry algorithms are Blender's Legacy GP functions. Blender's Legacy GP API documents these operations as part of the GP geometry layer, including simplify, subdivide, trim, merge-distance, resample, dissolve, and stretch.

The next loop is build/CI inspection. Any compiler, linker, or runtime/conformance error from this batch is treated as a dependency-closure problem and fixed at its smallest concrete boundary before expanding the next bulk feature group.


### Bulk Legacy GP paint-stage integration — 2026-09-29

Added Blender-backed paint-stage settings to the real `tGPspoint` stroke-buffer commit path.

- 34975fe — added `LegacyPaintSettings` to the backend API.
- 13b5c80 — stores those settings and applies Blender Legacy GP smoothing/input-sample processing when the sbuffer becomes a `bGPDstroke`.
- faf9fb4 / 1753993 — exposed the settings through the Android C bridge.
- 48405fa — native conformance test exercises brush smoothing/input-sample processing and verifies the real Blender sbuffer is cleared after commit.
- f68723a — corrected the paint-stage position smoothing call to the actual 3.6.23 native API signature used by the existing backend closure.

The upstream Legacy GP paint implementation performs smoothing and input-sample processing during stroke creation, using the same `BKE_gpencil_stroke_smooth*` family that this batch now calls. The Blender 3.6 API also exposes the Legacy GP drawing/interpolation/smoothing controls used by this path. 

CI verification branches were created only to trigger/inspect the Android build without modifying the production branch with CI marker files. They are not production feature commits.


### CI #399 failure correction — exact Blender 3.6.23 smoothing signatures and paint semantics
- Run #399 (36612069140) reached the Android native compile and failed only in project_grease_gp_backend.cpp; setup, pinned Blender import and minimal DNA generation all passed.
- The compiler exposed the real pinned 3.6.23 API in BKE_gpencil_geom_legacy.h: BKE_gpencil_stroke_smooth takes 9 arguments; BKE_gpencil_stroke_smooth_point, ..._smooth_strength, ..._smooth_thickness, and ..._smooth_uv also require their full Legacy signatures.
- Corrected production commit 7f1b961329afecdd6548a851db14dbb97b45ac21 restored the full signatures for the new paint-stage and bulk geometry calls.
- Follow-up production commit 96bc129f0bdec37c02efbfa95f4b2b7c2f8e26d7 matched the pinned Blender 3.6.23 gpencil_stroke_newfrombuffer() smoothing semantics: one stroke-wide BKE_gpencil_stroke_smooth() call using draw_smoothfac/draw_smoothlvl, then per-point input-sample smoothing with the full smooth_point/smooth_strength signatures.
- This correction is based on the actual pinned 3.6.23 source, not newer Blender/GP3 APIs.
- Next step is CI compile -> APK verification -> native conformance. A successful compile is not treated as feature completion.


### Proactive conformance correction after #399 — sbuffer lifecycle
- The modifier-stack test had an invalid expectation: it checked for 4 points in `stroke_buffer_count()` after `end_stroke()`. The backend intentionally commits the real Blender `tGPspoint` buffer to `bGPDstroke` and clears the temporary sbuffer, matching the paint lifecycle.
- Commit `77d27217a19198f3d43417ad0cde21d74e2f4fcc` changes the test to require sbuffer count 0 after commit and validate the committed stroke through the normal stroke APIs.
- This prevents a false runtime failure after the compile-stage smoothing fix.


### Next Legacy modifier expansion — Simplify
- Official Blender 3.6.23 `MOD_gpencil_legacy_simplify.c` confirms the real `deformStroke` callback dispatches Fixed, Adaptive/RDP, Sample, and Merge modes through BKE Legacy GP geometry APIs; its depsgraph parameter is unused.
- Production commit `94ab9f080fae32071b6f7938f6d65164b95c4329` configures the real `SimplifyGpencilModifierData` in the single-modifier path, and `a11563c6f87b56dcf786febffb2156efcfafbb46` adds the same real configuration to the modifier-stack path.
- Conformance commit `8ca5a4dfff1e2ca468b2bbf8363f4cfe370f89d7` adds a real Simplify callback test that requires the point count to decrease and the Legacy GP render/cache path to remain valid.
- Noise and other context/depsgraph-dependent modifiers are not being falsely marked complete; each must be source-traced and only added when its focused Android dependency closure is proven.
