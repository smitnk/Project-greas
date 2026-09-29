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

This batch is deliberately Blender-backed: the Project Grease layer supplies operation parameters and lifecycle/cache invalidation, while the actual geometry algorithms are Blender's Legacy GP functions. Blender's Legacy GP API documents these operations as part of the GP geometry layer, including simplify, subdivide, trim, merge-distance, resample, dissolve, and stretch. citeturn4search0turn5search1turn9search0

The next loop is build/CI inspection. Any compiler, linker, or runtime/conformance error from this batch is treated as a dependency-closure problem and fixed at its smallest concrete boundary before expanding the next bulk feature group.
