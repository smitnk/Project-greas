# Project Grease — Bundled Blender Legacy GP Engine Worklog

## Working rule

Project Grease must become a focused Android 2D drawing application backed by real Blender 3.6.23 Legacy Grease Pencil functionality.

Do NOT port the full Blender application.
Do NOT introduce OpenToonz or GL4ES.
Do NOT replace Blender GP algorithms with unrelated custom approximations when the Blender 3.6.23 implementation can be integrated.
Keep Android responsible for Activity/UI/input/EGL/GLES lifecycle.
Keep the existing Project Grease UI architecture.

## Current repository truth

Branch: feature/project-grease-ui-real-integration
Known successful baseline commit: ac11bd459f81422a054708e12222cd8ea973d4c8
Last verified CI: Android GP build/verification succeeded at workflow run #356 (2026-09-29), APK artifact project-grease-debug-apk.

The recent work added individual Legacy GP operations such as join/grouped selection. Those are real GP data operations, but they do NOT constitute the complete Blender 2D drawing engine.

## New working strategy

Stop treating each feature as a separate micro-phase.

Work in bundled engineering changes:
1. Map the actual Blender 3.6.23 Legacy GP implementation and its dependency closure.
2. Integrate coherent groups of real Blender GP functionality behind one Project Grease engine boundary.
3. Keep desktop-only context/UI/WindowManager/GHOST dependencies outside Android unless a concrete dependency proves otherwise.
4. Compile the bundle.
5. If compilation fails, inspect the exact missing symbol/header/type and add only the smallest Blender dependency closure that is genuinely required.
6. Re-run CI.
7. If CI succeeds, verify the resulting APK and then continue expanding the same engine boundary rather than stopping for a ceremonial phase.
8. Record every failure, fix, success, and next action in this file.

## Source direction

Blender 3.6.23 Legacy GP functionality is distributed across GP data/kernel, drawing/editor operations, sculpt/paint/editing, fill, modifiers, line art, animation and rendering/cache systems. The official 3.6 API exposes real GP drawing, eraser, stabilizer and selection operations. The repository's pinned Blender source is the authority for exact 3.6.23 implementation.

The target is not to copy all of Blender. The target is to extract the smallest complete closure needed for the real GP drawing/editor engine.

## Dependency policy

When a compiler/linker error occurs:
- classify it as header/type, symbol, generated DNA/RNA, allocator/math/container, data/kernel, editor operation, GPU/cache, or desktop-context dependency;
- locate the exact Blender 3.6.23 owner of the missing item;
- add that source/dependency only when it belongs to the GP closure;
- never solve a dependency error by importing the full Blender CMake graph;
- if a dependency is desktop-only, replace the context boundary with an Android-owned adapter only where necessary.

When runtime behavior fails:
- identify whether the failure is in GP data creation, GP operation, cache generation, Android GPU upload, coordinate conversion, or presentation;
- preserve the Blender GP data/algorithm and repair the adapter boundary instead of rewriting the GP operation.

## Success definition

A successful bundle means more than a compile:
- real Blender Legacy GP data is created and mutated;
- Android input reaches the real GP drawing path;
- the resulting GP data can be edited/animated using integrated Blender functionality;
- the Android renderer presents the result;
- CI builds and verifies the APK;
- tests exercise the actual native engine path.

## Current next action

Continue from the successful ac11bd4 baseline by integrating the real Blender Legacy GP drawing/editor dependency closure as a coherent engine, while keeping full Blender application/UI/scene/GHOST/desktop rendering outside the Android product.
