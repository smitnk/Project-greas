# Project Grease

Project Grease is a focused Android 2D animation/drawing application using a selected subset of Blender 3.6.23 Legacy Grease Pencil as its native drawing/data foundation.

It is **not** a full Blender Android port.

## Architecture

Android UI
-> Project Grease controllers
-> Project Grease JNI/native adapter
-> Blender Legacy Grease Pencil subset
-> GP geometry/cache
-> Project Grease Android EGL/GLES presentation

The application owns its UI, document model, animation workflow, editing controllers, materials, history and presentation layer.

## UI direction

The mobile editor is canvas-first and uses the supplied FlipaClip screenshots only as interaction/ergonomics references:

- compact top actions
- collapsible tool rail
- collapsible properties panel
- collapsible timeline
- full-screen canvas focus
- frame-oriented timeline interaction
- project/settings bottom sheets
- touch-sized controls

No branding, logo, proprietary asset or exact proprietary implementation is copied.

## Native scope

The selected native foundation is Blender 3.6.23 Legacy Grease Pencil:

- bGPdata
- bGPDlayer
- bGPDframe
- bGPDstroke
- bGPDspoint

Do not add Blender desktop UI, GHOST-on-Android, Blender Python, the full 3D viewport, full Blender rendering, OpenToonz, GL4ES, or the full Blender CMake graph.

See:
- PROJECT_GREASE_GP_SCOPE.md
- docs/PROJECT_GREASE_UI_ENGINE_MAPPING.md
- native/blender_gp/BLENDER_GP_SOURCE_MANIFEST.md
