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
Last verified CI baseline: workflow run #356 (2026-09-29), Android GP build/verification and APK artifact succeeded.

## Current bundled-engine work

1. 81c65980bba385007e198e3b3d5db25da3927ad9
   Added this persistent worklog so decisions, errors, fixes, successes and next actions are not lost.

2. cb8004bd0226d0d8a349380533a81c0a5930e1d9
   Added `project_grease_gp_engine.h`, a single host-facing engine boundary over the real Legacy GP Backend. It is an adapter, not a second drawing engine.

3. 8954a56a50b2ae1fa450622339e1a7f526072414
   Added `project_grease_gp_engine.cpp` with an explicit capability map. A capability is marked integrated only when it is actually backed and exercised; unsupported entries remain false instead of being advertised as complete.

4. d0089a8b28de07710f39cf423af1071fb161dbaa
   Expanded the focused native CMake target to compile the pinned Blender Legacy GP editor sources under `source/blender/editors/gpencil/` and GP modifier sources under `source/blender/gpencil_modifiers/`, including Line Art sources, while retaining the minimal Android boundary.

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

## Current state

The bundled editor/modifier source integration was corrected to Blender 3.6.23's actual gpencil_legacy and gpencil_modifiers_legacy directories and was CI-verified by Android Shell run #361, including APK verification.

The next action is to obtain a CI build of d0089a8, inspect the first real compiler/linker failure, and solve the dependency closure from that concrete evidence. If the build succeeds, the next action is runtime/native conformance verification of the integrated Blender GP editor/modifier path.

## Permanent architecture rule

Android:
UI + input + lifecycle + EGL/GLES

Project Grease:
controller/JNI/engine boundary

Blender 3.6.23:
real Legacy GP data + drawing/editing/sculpt/paint/modifier/Line Art algorithms as their minimal dependency closure

Not included:
full Blender application, desktop UI, Python, full scene/3D viewport, GHOST-on-Android, OpenToonz, GL4ES, unrelated Blender subsystems.

## Next real-engine bundle — Legacy GP modifier execution

- CI run #361 proved the actual Blender 3.6.23 Legacy GP editor/modifier source directories compile in the Android target.
- The next step is no longer source bundling alone: Project Grease now invokes Blender's real GpencilModifierTypeInfo::deformStroke callback through BKE_gpencil_modifier_new() / BKE_gpencil_modifier_get_info().
- Added Backend::apply_blender_modifier(...) and the C bridge entry point.
- Initial conformance test invokes Blender's actual Legacy GP Smooth modifier on an existing bGPDstroke and verifies geometry changes plus GP cache rendering.
- No Project Grease smoothing algorithm is used by this path; Blender 3.6.23 owns the modifier implementation.
- Next CI result determines the next minimal dependency fix. After this callback path passes, expand the same mechanism to additional Legacy GP modifier types and then build the modifier-stack/depsgraph closure only where the actual Blender implementation requires it.


## Exact CI error recovery — modifier API closure

### Runs inspected

- **#364 / 36599437309** — failed compiling `project_grease_gp_backend.cpp`.
- **#365 / 36599452874** — same native Legacy GP modifier API/type failure at the bridge exposure stage.
- **#366 / 36599462494** — same native failure at JNI wiring.
- **#367 / 36599485267** — succeeded after adding the real Blender Legacy GP modifier API/DNA includes.
- **#368 / 36599507228** — succeeded through Blender import, DNA generation, Android build, APK verification and artifact upload.

### Exact root failure

The failed Android compiler reported that `BKE_gpencil_modifier_init`, `BKE_gpencil_modifier_new`, `BKE_gpencil_modifier_get_info`, `GpencilModifierData`, `GpencilModifierType`, `GpencilModifierTypeInfo`, `SmoothGpencilModifierData`, `ThickGpencilModifierData`, `SubdivGpencilModifierData`, and the corresponding modifier enums/flags were unavailable while compiling the backend.

This was not a problem in the JNI logic itself. The Android target had not yet established the complete real Blender 3.6.23 Legacy GP modifier kernel closure.

### Upstream source verification

The pinned Blender commit is `e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca`, which is Blender 3.6.23 release commit. The exact upstream tree contains:

- `source/blender/blenkernel/BKE_gpencil_modifier_legacy.h`
- `source/blender/blenkernel/intern/gpencil_modifier_legacy.c`
- `source/blender/gpencil_modifiers_legacy/`
- `source/blender/makesdna/DNA_gpencil_modifier_types.h`

The header defines the real `GpencilModifierTypeInfo::deformStroke` callback and the `BKE_gpencil_modifier_init/new/get_info/free` API. The kernel implementation initializes the actual modifier registry and dispatches to the real modifier-type implementations.

### Fix applied

- `29ebc357b46ef4b5749a401cbf5dcf55149ac17a`
  - Explicitly includes the pinned Blender Legacy GP modifier DNA before the modifier API header in the backend.
- `5001d4175d710e16694647d4bff8335232e8feed`
  - Adds Blender 3.6.23's real `gpencil_modifier_legacy.c` to the Android native source closure.
  - Adds the real `gpencil_modifiers_legacy` include directories.

### Important interpretation

Run #368 proves the previous compiler/linkage state is recoverable, but it does **not** by itself prove that Android runtime execution has exercised the real modifier callback. The next verification must therefore test the actual Android-native modifier path, not merely APK creation.

## Current next target

Use the exact Blender 3.6.23 modifier registry and real modifier implementations already imported into the Android closure. Continue resolving only concrete compiler/linker/runtime dependencies. Then add conformance for multiple real Legacy GP modifier callbacks and a real modifier-stack execution path where the Blender implementation requires it.

Never replace these callbacks with Project Grease geometry algorithms.


## Next bundle — real Legacy GP modifier-list execution

### Verified starting point

- Android CI **#370** succeeded for commit `5001d417`.
- The real Blender 3.6.23 `gpencil_modifier_legacy.c` registry/kernel is now included in the Android native closure.
- The real `gpencil_modifiers_legacy` implementation sources are already compiled.

### Implementation

- `be1cc84d83dad2ca145ca38f9e800c8d60d05d3e`
  - Adds `Backend::apply_blender_modifier_stack()`.
- `83a29118aa2cf2d6d47e43bbcfcacbe13a7231ef`
  - Creates a real Blender 3.6.23 `Object` adapter with `greasepencil_modifiers`.
  - Creates each modifier using `BKE_gpencil_modifier_new()`.
  - Resolves its real `GpencilModifierTypeInfo`.
  - Adds it to Blender's actual `Object::greasepencil_modifiers` ListBase.
  - Executes the real Blender `deformStroke()` callback in list order.
  - Frees the real Blender modifier objects afterward.
- `6aeffc659b427c252de576995ffd49d33ca66c32` and `525a0281d44be9b1688a3b974e245832868e608e`
  - Expose the stack through the native C bridge.
- `2ce7c88d8585bfd736746e9fe90436f5c69b1fd1`
  - Adds conformance for Smooth -> Thickness -> Subdivide.

### Why this is a real Blender path

Blender 3.6.23's DNA defines `Object::greasepencil_modifiers` as a ListBase of `GpencilModifierData`. The Project Grease stack now uses that actual Blender structure for evaluation instead of inventing a parallel modifier representation.

### Constraint

This stack currently targets modifiers whose real `deformStroke` callback can execute with the focused Android object/data context. Modifiers requiring a real depsgraph, scene, armature, texture, or 3D object context are not falsely marked complete. Their exact dependency closure will be added only when their upstream implementation is brought into the focused engine.

### Next

Run CI. Fix only the exact compiler/linker/runtime failure. After stack conformance succeeds, continue with the next real Legacy GP subsystem rather than declaring completion.
