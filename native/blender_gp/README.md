# Project Grease — Native Legacy Grease Pencil Backend

This directory is the native drawing/data boundary for Project Grease.

## Source baseline

- Blender v3.6.23
- Pinned commit: e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca
- Legacy GP data: bGPdata / bGPDlayer / bGPDframe / bGPDstroke / bGPDspoint

## Current production boundary

Android UI/input
-> Project Grease controllers
-> Project Grease JNI/bridge
-> real Legacy GP data
-> focused GP cache/geometry
-> Project Grease Android GLES presentation

The Android application owns EGL/GLES and lifecycle. The native backend does not create Android GHOST or a Blender desktop application.

## Verified native capabilities

The backend can create the real Legacy GP document/layer/frame structures, create strokes and points, capture pressure/strength/time, expose stroke editing primitives, and attach to the externally-owned Android EGL/GLES context. The latest Android shell CI also builds and verifies the APK.

The current Android presentation remains deliberately focused and does not claim the full Blender DRW/GPU stack.

## Rules

Do not replace Legacy Grease Pencil with Android Canvas, OpenToonz, GL4ES, or a separate custom stroke engine.

Do not import the full Blender application, desktop UI, Python, GHOST, scene system, or unrelated render systems merely to implement a Project Grease feature.

Add Blender code only when dependency evidence proves it is required by the selected GP path.
