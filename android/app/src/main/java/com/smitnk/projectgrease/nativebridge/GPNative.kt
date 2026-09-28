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
    external fun nativeBeginStrokeEglRenderer(handle: Long, materialIndex: Int, thickness: Float): Boolean
    external fun nativeAddPointEglRenderer(handle: Long, x: Float, y: Float, z: Float, pressure: Float, strength: Float, time: Float): Boolean
    external fun nativeEndStrokeEglRenderer(handle: Long): Boolean
    external fun nativeSetPreviewStrokeEglRenderer(handle: Long, points: FloatArray, thickness: Float): Boolean
    external fun nativeClearPreviewStrokeEglRenderer(handle: Long): Boolean

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
    external fun nativeCreateLayer(handle: Long, name: String): Boolean
    external fun nativeSelectLayer(handle: Long, index: Int): Boolean
    external fun nativeLayerCount(handle: Long): Int
    external fun nativeCreateFrame(handle: Long, frameNumber: Int): Boolean
    external fun nativeSelectFrame(handle: Long, frameNumber: Int): Boolean
    external fun nativeFrameCount(handle: Long): Int
    external fun nativeDuplicateFrame(handle: Long, sourceFrame: Int, targetFrame: Int): Boolean
    external fun nativeDeleteFrame(handle: Long, frameNumber: Int): Boolean
    external fun nativeSelectStroke(handle: Long, index: Int): Boolean
    external fun nativeHitTestStroke(handle: Long, x: Float, y: Float, radius: Float): Int
    external fun nativeDeleteStroke(handle: Long, index: Int): Boolean
    external fun nativeDeleteLastStroke(handle: Long): Boolean
    external fun nativeDuplicateStroke(handle: Long, index: Int): Boolean
    external fun nativeTranslateStroke(handle: Long, index: Int, dx: Float, dy: Float, dz: Float): Boolean
    external fun nativeFlipStroke(handle: Long, index: Int): Boolean
    external fun nativeSubdivideStroke(handle: Long, index: Int, level: Int): Boolean
    external fun nativeCloseStroke(handle: Long, index: Int): Boolean
    external fun nativeTrimStroke(handle: Long, index: Int, from: Int, to: Int, keepSinglePoint: Boolean): Boolean
    external fun nativeSplitStroke(handle: Long, index: Int, beforeIndex: Int): Boolean
    external fun nativeGetPoint(handle: Long, strokeIndex: Int, pointIndex: Int): FloatArray?
}
