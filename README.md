# Project Grease

Project Grease is being rebuilt around a native Blender Legacy Grease Pencil backend.

## Current baseline

- Upstream Blender v3.6.23
- Commit: e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca
- Native GP data: bGPdata / bGPDlayer / bGPDframe / bGPDstroke
- Target renderer: Blender GP draw engine + DRW/GPU
- Android graphics target: EGL/GLES
- No GL4ES
- No Android Blender application port is embedded

## Development

The active native backend work is isolated on:

native/blender-gp-backend

The old Android-port research/import manifest has been removed from the main branch so it is not used as the implementation baseline.

The first real milestone is a native one-stroke Blender GP render. UI design is intentionally deferred until the backend is proven.
