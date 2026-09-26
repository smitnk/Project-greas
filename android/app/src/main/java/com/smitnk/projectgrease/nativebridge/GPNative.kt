package com.smitnk.projectgrease.nativebridge

/**
 * Android-facing JNI contract for the Project Grease native engine.
 *
 * The current Android build exposes nativePing() first. The GP operations
 * already defined by the native adapter are kept as the next connection step.
 */
object GPNative {
    init {
        System.loadLibrary("projectgrease_jni")
    }

    external fun nativePing(): Boolean

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
