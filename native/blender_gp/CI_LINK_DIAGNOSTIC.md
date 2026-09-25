# CI Link Diagnostic — Generated Link Recipes

## Purpose

This note records the CI diagnostic correction for the native Blender GP backend.

The next linker investigation should inspect CMake's generated link recipes after configure/generate and before compilation. This avoids paying the cost of compiling full Blender just to discover the linker command.

## Why link.txt is the correct first diagnostic

With the Unix Makefiles generator, CMake generates per-target link command files during the generate phase. The expected reference target is:

    build/blender-core/source/creator/CMakeFiles/blender.dir/link.txt

The Project Grease link-test target should have a corresponding generated file under its CMake build-tree directory, for example:

    build/blender-core/project_grease_gp/CMakeFiles/project_grease_gp_link_test.dir/link.txt

The exact Project Grease path must be discovered from the generated tree rather than assumed.

Reading these files does not invoke compilation or linking.

If the configured generator is Ninja instead of Unix Makefiles, inspect the generated Ninja command database (for example with ninja -t commands) rather than assuming link.txt exists.

## Important condition

The real Blender link.txt exists only if the configure pass actually defines the stock blender target through Blender's creator CMake files.

Run #28 already configures Blender's CMake project, but the diagnostic must verify that the stock blender target is present before attempting to compare its link recipe.

If it is absent, perform a configure-only reference pass against the same pinned Blender v3.6.23 source, toolchain, and relevant CMake options. Do not build the full Blender executable merely to obtain its link command.

## Required comparison

Compare:

1. Blender's generated blender target link recipe.
2. Project Grease's generated project_grease_gp_link_test link recipe.

Do not assume the cause in advance.

The comparison should establish whether the Project Grease executable is missing:

- Blender's accumulated target/link dependency closure.
- Required static libraries.
- Required library ordering.
- Linker grouping/options.
- Required link flags or platform libraries.
- Another target-level usage requirement present on Blender's executable but absent from the Project Grease target.

## Current Run #28 evidence

Run #28 successfully reached:

- bf_gpu
- project_grease_blender_gp
- project_grease_gp_link_test

The failure occurred at the final executable link.

The failure contained 271 undefined-reference occurrences across 113 unique symbols, including BKE legacy GP APIs, MEM allocation, GPU batch/vertex/index buffers, DRW context/render functions, ED GP helpers, BLI helpers, GPENCIL shader/antialiasing functions, and GP image rendering.

Therefore the current problem is a link dependency/closure problem, not a GP source compilation problem.

## Current constraint

Do not respond to this failure by manually adding dozens or hundreds of Blender libraries.

First inspect the generated link recipes and identify the exact difference between Blender's working target and the Project Grease test target. Then make the smallest CMake change that reproduces the required dependency mechanism.

## Project architecture constraint

This diagnostic does not change the Project Grease architecture.

The target remains:

Project Grease -> Blender Legacy Grease Pencil data -> Blender GP draw cache -> Blender GP draw engine -> Blender GPU/DRW -> Android EGL/GLES.

It does not authorize importing the old Blender Android application port, full Blender UI, Android Canvas, GL4ES, or a custom stroke renderer.
