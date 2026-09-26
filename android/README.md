# Project Grease Android shell

This directory is the Android/Gradle application layer for Project Grease.

Current scope:
- Android application module
- Kotlin + Jetpack Compose
- Existing Project Grease Grease-Pencil-oriented UI
- Reproducible compile configuration
- Android JNI bridge
- Android EGL/GLES viewport surface
- No Blender UI or drawing engine is imported here

## Native rendering boundary

The Android viewport now proves the transport layer:

Android SurfaceView
-> Surface
-> ANativeWindow
-> EGL display/context/window surface
-> OpenGL ES 2
-> eglSwapBuffers()

The native EGL renderer only clears and presents the surface. It does not implement Grease Pencil drawing.

The Blender GP backend remains a separate native layer. The next connection step is to make the Blender GPU/DRW path use this Android EGL/GLES context without importing Blender's desktop GHOST application layer.

The current desktop Blender GHOST rendering path is intentionally not reused as the Android renderer.
