# Project Grease — Final Blender 3.6.23 Legacy GP Engine Plan

## Goal

Build a focused Android 2D drawing/animation engine from Blender 3.6.23 Legacy Grease Pencil source behavior. This is not a full Blender Android port.

## Authority

- Blender v3.6.23 source commit: e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca
- Legacy GP data, editor operators, geometry algorithms, fill, brushes/material state, modifiers and GP draw-engine behavior are taken from Blender source where technically required.
- Old Blender Android/GP ports are implementation references only; they do not override Blender 3.6.23 behavior.

## Engineering rule

Do not replace a Blender Legacy GP behavior with a Project Grease approximation when the corresponding Blender 3.6.23 implementation can be brought into the focused dependency closure.

## Target dependency closure

1. Real Legacy GP data and runtime state.
2. Legacy GP stroke creation/input buffer and drawing lifecycle.
3. Legacy GP drawing/brush behavior and pressure/material handling.
4. Legacy GP primitives/operators.
5. Legacy GP edit operators and geometry algorithms.
6. Legacy GP fill/gap closing and triangulation.
7. Legacy GP sculpt/paint operations where supported by the 3.6.23 source closure.
8. Legacy GP animation/frame/layer/onion workflows.
9. Legacy GP modifier behavior required by the 2D engine.
10. Legacy GP draw/cache/shader behavior, adapted only at the Android GPU/EGL boundary.
11. Advanced GP subsystems such as Line Art, curve conversion, SVG/PDF and rigging only when their minimal, verifiable dependency closure is established.

## Explicit boundary

Keep outside the Android product:
- Blender desktop UI/GHOST
- Blender Python
- full Blender application executable
- full scene/database/editor stack unless a specific Legacy GP feature proves a minimal dependency is required
- unrelated Blender render engines

Android remains the owner of Activity, Surface, lifecycle, input, EGL and the final GLES presentation boundary.

## Current remediation

The existing Project Grease branch contains substantial real Legacy GP data and BKE operations, but also custom replacements for primitives, fill, stabilization, eraser hit testing, brush presets, interpolation and parts of modifiers. These are not the final engine.

The next implementation work must systematically replace those approximations with the corresponding 3.6.23 Legacy GP source paths, using dependency-minimal extraction and regression tests.

## Definition of done

The engine is considered final only when:
- the implemented feature is backed by the corresponding Blender 3.6.23 Legacy GP source path or an explicitly necessary Android adapter;
- custom approximations are removed or isolated to Android platform concerns;
- host/native regression tests cover each subsystem;
- Android CI builds a debug APK;
- runtime-critical JNI symbols are verified;
- feature behavior is manually device-validated before being called complete;
- independent QA review finds no exposed feature falsely advertised as complete.

