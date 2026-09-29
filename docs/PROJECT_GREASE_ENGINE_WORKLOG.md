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

The bundled editor/modifier source integration has been committed, but **it has not yet been CI-verified after d0089a8**. Therefore it must not be called successful yet.

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
