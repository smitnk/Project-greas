package com.smitnk.projectgrease.nativebridge

/**
 * Android-facing JNI contract for the Project Grease native engine.
 *
 * The EGL renderer owns the Android surface/context. When the Android
 * compatible Blender GP native library is linked into this target, the
 * renderer attaches the current EGL/GLES context to the GP backend.
 */
object GPNative {
    init {
        System.loadLibrary("projectgrease_jni")
    }

    external fun nativePing(): Boolean

    external fun nativeCreateEglRenderer(): Long
    external fun nativeDestroyEglRenderer(handle: Long)
    external fun nativeAttachSurface(handle: Long, surface: android.view.Surface): Boolean
    external fun nativeDetachSurface(handle: Long)
    external fun nativeRenderEgl(handle: Long): Boolean
    external fun nativeEglReady(handle: Long): Boolean
    external fun nativeGlesVersion(handle: Long): Int
    external fun nativeBlenderGpConnected(handle: Long): Boolean

    external fun nativeCreate(): Long
    external fun nativeDestroy(handle: Long)
    external fun nativeBeginStroke(handle: Long, materialIndex: Int, thickness: Float): Boolean
    external fun nativeAddPoint(
        handle: Long,
        x: Float,
        y: Float,
        z: Float,
        pressure: Float,
        strength: Float,
        time: Float
    ): Boolean
    external fun nativeEndStroke(handle: Long): Boolean
    external fun nativeRender(handle: Long): Boolean
    external fun nativeStrokeCount(handle: Long): Int
    external fun nativePointCount(handle: Long): Int
}
