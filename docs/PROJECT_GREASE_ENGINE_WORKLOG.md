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


### CI #404 failure — duplicate Simplify switch case; corrected before next validation
- Run #404 failed in the Android CMake build at project_grease_gp_backend.cpp:3323 with clang error duplicate case value eGpencilModifierType_Simplify; the earlier case was at line 3301.
- This was a local integration error introduced while adding real Legacy GP Length configuration, not a Blender API/dependency failure.
- Commit 874550e5e8c36880f34dc5c5166076b53e9a0d1d removes the duplicate case. The current modifier-stack switch has one Smooth, one Simplify, one Length, one Thick, one Subdiv case, then default.
- Run #407 is validating this correction; #408 is the newer Length conformance validation.


### Next real Legacy modifier expansion — Opacity and Color
- Pinned Blender 3.6.23 `MOD_gpencil_legacy_opacity.c` uses the real `OpacityGpencilModifierData` callback to modify stroke strength/fill opacity/hardness. Its depsgraph parameter is unused for the focused deform path.
- Pinned `MOD_gpencil_legacy_color.c` uses real HSV conversion on Legacy GP vertex colors, with material-style fallback and optional curve weighting.
- Commit 1b9c2bf356bedf1d80cdc6f40d201abb991b2d9c configures Blender-owned Opacity and Color modifier data in both single and stack execution paths; commit ceb804eea12e92263a635ff4fa545aefc0e0c7ff wires the single-modifier path explicitly.
- Commit 59fd1a8db475f87d1b2946ea4c22d7c58e3fa520 adds native conformance tests for real Opacity strength modification and real Color vertex-color modification.
- These are source-derived Blender callbacks; no custom Project Grease opacity/color algorithm is substituted.


### Bulk Legacy GP modifier expansion — Tint / Offset / Texture Mapping — 2026-09-30

The next modifier bundle is source-traced against pinned Blender 3.6.23 Legacy GP code:

- **Tint**: uses Blender's real `TintGpencilModifierData` + `deformStroke()` path for vertex/fill color influence.
- **Offset**: uses Blender's real `OffsetGpencilModifierData` + `deformStroke()` path for location/rotation/scale deformation. Randomized paths remain outside this focused deterministic conformance case.
- **Texture Mapping**: uses Blender's real `TextureGpencilModifierData` + `deformStroke()` path for stroke UV factor/rotation and fill mapping parameters.

Project Grease adds no replacement geometry algorithm. The focused adapter supplies only the Blender-owned modifier settings and the minimum Object context needed by these callbacks. Native conformance verifies actual point/color/UV changes and render/cache validity.

The next CI loop must compile, link, run conformance, verify the APK, and upload the artifact. If any dependency or runtime error appears, fix the exact Blender 3.6.23 closure before adding the next modifier bundle.


### Bulk Legacy GP modifier expansion — Weight Angle / Weight Proximity / Hook — 2026-09-30

Source-traced against pinned Blender 3.6.23 Legacy GP modifier implementations.

- **Weight Angle** uses the real `WeightAngleGpencilModifierData::deformStroke()` callback and Blender deform-vertex APIs. Project Grease creates only the required target deform group in the real GP datablock.
- **Weight Proximity** uses the real `WeightProxGpencilModifierData::deformStroke()` callback. The focused adapter supplies a deterministic target Object transform; it does not port Blender's scene/depsgraph evaluation.
- **Hook** uses the real `HookGpencilModifierData::deformStroke()` callback with a deterministic target Object transform, no armature/bone subtarget, and no desktop depsgraph evaluation.
- Native conformance checks real deform-group weights and Hook geometry movement, followed by Blender GP render/cache validation.

These are not reimplemented Project Grease deformation algorithms. The adapter supplies only the minimum Blender-owned Object/deform-group context required by the upstream 3.6.23 callbacks.


### Generator closure — Legacy GP Build — 2026-09-30

Run #415 for commit `7bb2130e238ef86cd3984585925da30545307fc3` completed successfully through APK verification and artifact upload.

The generator sequence started with **Build**. Blender 3.6.23's desktop `generateStrokes()` requires Depsgraph/Scene evaluation, which Project Grease intentionally does not port. The focused implementation isolates Blender's deterministic `build_concurrent()`, `reduce_stroke_points()`, and `fade_stroke_points()` algorithms over the real `bGPdata/bGPDframe`, while retaining Blender's real `BuildGpencilModifierData`. The Android adapter supplies only deterministic percentage/concurrent context.

The conformance test creates a real GP stroke, configures a real Blender Legacy GP Build modifier, runs the pinned 3.6.23 algorithm closure, verifies point reduction, and renders the resulting GP data.

### Primitive closure bundle — 2026-09-30

Source reference: pinned Blender 3.6.23 `source/blender/editors/gpencil_legacy/gpencil_primitive.c`.

- `8d48ae8` extends the focused primitive API with Bézier and polyline operations.
- `b6a74c3` implements the deterministic point generation using the same cubic Bézier interpolation and segmented line construction used by the pinned Legacy GP primitive implementation.
- `5f01e72` adds conformance coverage for Bézier and polyline generation and their `generate()` dispatch.

Review result: the mathematical generation path is source-aligned, but this does **not** yet mean the full Blender primitive operator is complete. The desktop operator still owns modal context, brush/material state, depth projection, control points, frame transfer and auto-merge. Those dependencies must be selectively closed before the Android tool can be marked complete.

Next bundled work remains the real operator lifecycle and fill/editor dependency closure, not another custom shape implementation.

### Fill geometry closure bundle — 2026-09-30

Pinned Blender 3.6.23 `gpencil_fill.c` source review identified a concrete mismatch in the existing focused fill extraction: its dilation was cardinal-only, while Blender's Legacy GP `dilate_shape()` stages cardinal and diagonal candidates before applying them.

- `0848899` aligns the focused dilation and contraction routines with the pinned 3.6.23 algorithms, including staged updates and diagonal dilation candidates.
- `caaa160` adds regression coverage for the fill outline path with dilation enabled.

This remains an extraction of the Blender Legacy GP fill image algorithm, not a claim that the complete desktop fill operator is ported. The operator still has a larger dependency closure around offscreen rendering, stroke extension/collision, depth projection, layer selection and final stroke transfer.


## Live modifier stack (non-destructive, per layer)

- A layer owns an ordered list of `PGModEntry {type, enabled, params[]}` (`native/blender_gp/project_grease_modifier_stack.{h,c}`), held by `Backend` by layer index, carried by layer move/duplicate/delete and by undo/redo snapshots, and saved per layer in the project file (format version 4, key `modifiers`; older files load with an empty stack).
- Evaluation (`pg_mod_eval_frame`) duplicates the layer's current frame strokes with `BKE_gpencil_stroke_duplicate`, runs each enabled modifier's deform function in order on the copies and hands the copies to the presenter. Original strokes are never modified; editing, hit testing, saving and the fill-tool mask use the originals. The evaluated frame is cached and invalidated by any document edit (`project_grease_gp_tag`), a frame change or a stack change.
- Apply (`pg_mod_apply`) bakes one modifier into every frame of the layer and removes it from the stack.
- Thickness, Opacity, Tint, Hue/Saturation and Length reuse the per-stroke functions of `project_grease_blender_edit.c` (the code that mirrors `deformStroke()`); Smooth, Simplify, Subdivide, Offset and Noise carry the pinned Blender `deformStroke()` bodies as `BEGIN/END VERBATIM` regions (checked by `tools/verify_blender_verbatim.py`) with documented adapted glue. Offset and Noise run in an object-space view of the canvas (y flipped; Noise in units of 100 px) and use Blender's `BLI_hash_*` / `BLI_halton_*` (`rand.cc`, `noise.c`).
- Not supported: vertex-group weights, layer/material/pass filters, custom intensity curves; onion-skin ghosts are drawn unmodified.
- Tests: `tools/run_native_modifier_stack_tests.sh` (links the real pinned BKE code; compares Offset and Noise results with an independent Python reference), `ModifierStackTest`, `ProjectDocumentRoundTripTest`.


## Line Art batch 1 — Scene-lite (approved architecture change) — 2026-10-02

The user approved SPEC_LINE_ART_ARCHITECTURE: an opt-in "Scene-lite" module beside the 2D GP
backend, because Blender's Line Art reads evaluated meshes, objects and a camera that Project Grease
otherwise does not have. Batch 1 adds only the data Line Art's loaders read; the 2D path does not use it.

- `native/blender_gp/project_grease_scene_lite.{h,c}`: objects (name, world matrix, line art usage)
  with MeshLite (vertices; triangles keeping their polygon and material; unique edges with their two
  adjacent triangles and loose / material-boundary / non-manifold / polygon-diagonal flags, plus
  slots for sharp / seam / freestyle marks), a Wavefront OBJ loader (o/g, v, f with any index form,
  l, usemtl; Y-up -> Z-up like Blender's importer) and a camera.
- Camera math is Blender's: `BKE_camera_sensor_size/fit`, `focallength_to_fov`,
  `lineart_matrix_perspective_44d` / `lineart_matrix_ortho_44d` are verbatim file-local copies
  (verify_blender_verbatim.py, 5 new regions); the glue reproduces the camera part of
  `lineart_main_load_geometries()` / `lineart_main_init()` (normalized camera axes, sensor fit,
  shift adjusted for the fit, fb = ndc - 2 * shift).
- Host test `tools/run_native_scene_lite_tests.sh` (ASan/UBSan): OBJ parsing and edges of a
  two-material cube and a loose polyline; frame edges at d * tan(fov / 2) for landscape/portrait
  fits, orthographic corners, shift, camera scale ignored.
- Android: JNI `nativeSceneLite*` (own handle), `ReferenceScene` / `ReferenceCamera`,
  More > "3D reference (Line Art)" (Import OBJ, clear, perspective/orthographic, lens, ortho scale,
  orbit, elevation, distance, shift) and a wireframe of the mesh edges over the canvas.
- LINE_ART is IN_PROGRESS with "BATCH 1 OF 3 ONLY": no lines are generated, the reference is not
  saved. Next: batch 2 (lineart_cpu triangle/edge buffer and occlusion, compared with reference
  output exported from desktop Blender 3.6.23), then batch 3 (chaining, strokes, UI).


## Line Art batch 2 — Blender 3.6.23 Line Art core on Scene-lite — 2026-10-02

- `tools/gen_lineart_lite.py` generates `native/blender_gp/lineart/project_grease_lineart_cpu.cc` from the
  pinned `lineart_cpu.cc`: the file is emitted in its original order as BEGIN/END VERBATIM regions
  (10 regions, ~4,000 lines: culling, near/far clipping, perspective division, bounding areas,
  triangle intersections, edge linking, occlusion), and only the 16 items that read Mesh / Object /
  Depsgraph / Collection / Material data are swapped for the Scene-lite versions in
  `lineart_scene_lite_replacements.cc` (written to follow the originals line by line). GP output
  (`lineart_gpencil_generate`) is left for batch 3. CI checks the output is up to date (`--check`) and
  the regions byte for byte (`verify_blender_verbatim.py`). The pinned `lineart_util.c` is compiled as is.
- `lineart_lite_runtime.cc`: serial BLI_task pool / parallel range (one thread, deterministic), spin
  locks, `G`, PIL time and the shadow entry points (shadows / light contour unsupported: off).
- Scene-lite gained per-polygon smooth shading (OBJ `s`, flat by default like Blender's importer):
  Line Art only looks for creases on flat (`sharp_face`) polygons unless forced.
- `project_grease_lineart_lite.h`: `pg_lineart_compute()` returns every feature edge's segments with
  occlusion level and type in Line Art frame-buffer coordinates.
- Tests (`tools/run_native_lineart_tests.sh`, plain + ASan/UBSan): cube from a corner (9 visible / 3
  hidden at level 1), orthographic front (4 edges, exact perimeter), an occluding plane cutting edges,
  intersection lines of overlapping cubes, loose edges, empty / behind-camera scenes.
- Reference: CI downloads desktop Blender 3.6.23 (cached), runs `tools/lineart_reference/blender_reference.py`
  (OBJ import, same camera, default Line Art modifier with overscan 0 / depth offset 0) on
  `tools/lineart_reference/scenes.txt`, and the test requires >= 99 % two-way coverage between
  Blender's strokes and our visible segments in frame-buffer space (tolerance 0.002).
- Android: JNI `nativeSceneLiteLineArt`; the 3D reference sheet has a "Line Art preview" switch that
  draws Line Art's visible lines instead of the mesh wireframe. LINE_ART stays IN_PROGRESS ("BATCH 2 OF 3").


## Line Art batch 3 — chaining and strokes — 2026-10-02

- The pinned `lineart_chain.c` is compiled unmodified (Android manifest + host test). The Scene-lite
  `MOD_lineart_compute_feature_lines` continues after occlusion exactly as Blender's: enclosed shapes
  (no shadows: no-op), unused-line removal, `MOD_lineart_chain_feature_lines`,
  `..._split_for_fixed_occlusion`, `..._connect`, smoothing / border trimming / angle splitting when
  enabled, the depth offset towards the camera, cache transfer, `clear_picked_flag`, `finalize_chains`.
- `lineart_gpencil_generate` is replaced (generator section) by a version that keeps the original's
  picked / type / level / two-point filters and writes `PGLineartStrokes` (world `gpos` and image
  `pos` per point) instead of GP strokes; the remaining filters (object / collection source,
  material & intersection masks, shadow selection, silhouettes, vertex groups) have no Scene-lite
  data. `pg_lineart_compute_strokes()` is the API; settings gained level_start, chaining threshold,
  smoothing, angle split, depth offset and stroke types (Blender defaults).
- Tests: cube -> 3 chained strokes on its edges, hidden-line strokes at level 1, type filter, depth
  offset. CI: every Blender 3.6.23 stroke of every reference scene must equal one of ours point for
  point (world space, 1e-3, forward or reversed) and the counts must match.
- Batch 2 follow-ups found by the Blender comparison: edges the chainer skips (no type left /
  CHAIN_PICKED after culling or `lineart_main_discard_out_of_frame_edges`) are no longer reported;
  the axis-aligned reference scene was made non-degenerate.
- Android: JNI `nativeSceneLiteLineArtStrokes`; 3D reference sheet: line thickness, "include hidden
  lines", "Generate Line Art strokes" -> strokes on a new "Line Art" layer at the current frame,
  in the active color (image-space points mapped to the canvas). LINE_ART stays IN_PROGRESS:
  unverified on device, no shadows / material settings / collections / per-frame re-bake.

## Line Art — bake to frames — 2026-10-02

- Spec item "bake to frames for animation": `EditorController.bakeLineArt(from, to, yawFrom, yawTo, ...)`
  orbits the reference camera (`ReferenceCamera.orbitAt`, linear yaw) over the frame range and writes
  each frame's Line Art strokes to its own keyframe on a new "Line Art bake" layer; the camera is
  restored afterwards. Sheet: frame count, orbit sweep, "Bake Line Art to frames". Unit test for
  `orbitAt`; CI checks the button string in the APK.

## Next batch (one go) — 2026-10-02, branch feature/batch-next

- 0. BUG thick-stroke slashes: the GLES presenter drew each segment as its own quad (wedge gaps
  on the outer side of bends, visible at Size 100). Strokes are now one band built by
  `project_grease_stroke_outline.c` after Blender's `gpencil_vertex()` (common_gpencil_lib.glsl):
  miter offsets with miter_limit cos 60 deg, bevel + round join when it breaks, round caps
  (caps[] FLAT respected), per-point thickness x pressure. Used for strokes, the live sbuffer,
  the pending preview, annotations and the fill mask. Unit tests (90/150/45/179/-120 deg turns
  without gaps, plain strip, miter limit, cyclic, dot) and a GLES pixel test of a 40 px bend
  that fails on the old presenter.
- 1. Segment select: `project_grease_blender_edit4.c` carries gpencil_utils.c collision helpers
  and `ED_gpencil_select_stroke_segment` verbatim; flat_ref is swapped for the XY projection our
  strokes lie in (flat_ref degenerates on straight strokes). Edit-mode Point/Stroke/Segment
  switch; area selection treats Segment as Point.
- 2-4. Fill Leak / Dilate (negative contracts) / boundary All, Strokes, Edit Lines (1 px center
  lines) reach the fill mask and the Legacy fill port.
- 5. Delete material: strokes on the slot removed, higher slots shifted
  (BKE_gpencil_material_index_reassign), palette shrinks; confirm dialog; undoable.
- 6. Fit canvas button (zoom 1 / no pan = whole canvas with 4% margins).
- 7-8. Onion: GP_DATA_SHOW_ONIONSKINS overlay switch + per-layer GP_LAYER_ONIONSKIN, gpd gstep
  keyframes, ghost alpha verbatim from gpencil_layer_final_tint_and_alpha_get (GP_ONION_FADE).
- 9. Save as (SAF create-document, continues under the file's name).
- 10. PNG export: offscreen canvas-size render of the current frame by the same presenter,
  transparent background option (premultiplied blending undone on readback).
- BLI_ghash / mempool / hash_mm2a added to the Android and host closures.
- Not validated locally: Android NDK link probe and Gradle (no Google Maven / NDK in the
  session); editor Kotlin compiled and its 149 unit tests run offline with kotlinc.
