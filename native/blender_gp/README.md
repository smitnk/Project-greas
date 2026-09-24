# Project Grease — Native Blender Grease Pencil Backend

This directory is the native drawing backend boundary for Project Grease.

## Source baseline

- Blender source: upstream Blender
- Pinned release: v3.6.23
- Pinned commit: e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca
- Legacy Grease Pencil architecture: bGPdata / bGPDlayer / bGPDframe / bGPDstroke
- Rendering path investigated: GP draw cache -> GP draw engine -> Blender GPU/DRW -> EGL/GLES on Android

The previously investigated Blender Android port is **not imported** into this project. Its Android platform layer is only a historical reference for EGL/GLES/input behavior.

## Current rule

Do not replace Blender Grease Pencil with Android Canvas, GL4ES, or a custom stroke renderer.

The first backend milestone is a real Blender GP data object and a real GP renderer path. UI work is intentionally deferred.

## Import boundary

The source import script is responsible for obtaining the pinned upstream Blender tree. The Project Grease backend should consume only the minimum source required by the GP target; it must not turn Project Grease into the full Blender application.

See:
- BLENDER_GP_SOURCE_MANIFEST.md
- tools/import_blender_gp.sh
