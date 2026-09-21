# Project-greas

Experimental MotionCanvas-compatible native drawing backend research project.

## Blender source

This repository uses the Android Blender port as the reference/build base:
- https://github.com/Wanderson-Magalhaes/blender_for_android

The project does **not** embed Blender's desktop UI. The intended direction is a native drawing backend exposed to MotionCanvas through a small API, with Grease Pencil as the eventual drawing representation.

## Import policy

Do not copy the entire Blender tree blindly. Grease Pencil depends on Blender's data, geometry, GPU and runtime systems. The import is therefore staged and source-driven.

See docs/BLENDER_GP_IMPORT_MANIFEST.md.
