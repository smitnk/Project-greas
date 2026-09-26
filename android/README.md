# Project Grease Android shell

This directory is the Android/Gradle application layer for Project Grease.

Current scope:
- Android application module
- Kotlin + Jetpack Compose
- Existing Project Grease Grease-Pencil-oriented UI
- Reproducible compile configuration
- No Blender UI or drawing engine is imported here

The native Blender GP bridge remains in the repository's existing native layer.

The next native Android layer will connect this UI to the JNI adapter and then to an Android EGL/GLES rendering surface. The current desktop Blender GHOST rendering path is intentionally not reused as the Android renderer.
