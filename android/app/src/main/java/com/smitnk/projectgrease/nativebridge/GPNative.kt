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
    external fun nativeCancelStrokeEglRenderer(handle: Long): Boolean
    external fun nativeFillAtEglRenderer(handle: Long, seedX: Int, seedY: Int, materialIndex: Int, thickness: Float): Boolean
    external fun nativeSetPreviewStrokeEglRenderer(handle: Long, points: FloatArray, thickness: Float): Boolean
    external fun nativeClearPreviewStrokeEglRenderer(handle: Long): Boolean
    external fun nativeSetStrokeColorEglRenderer(handle: Long, r: Float, g: Float, b: Float, a: Float): Boolean
    external fun nativeSetCanvasSize(handle: Long, width: Int, height: Int): Boolean
    external fun nativeSetViewTransform(handle: Long, zoom: Float, panX: Float, panY: Float): Boolean

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
    external fun nativeSetLayerVisibility(handle: Long, index: Int, visible: Boolean): Boolean
    external fun nativeSetLayerLocked(handle: Long, index: Int, locked: Boolean): Boolean
    external fun nativeMoveLayer(handle: Long, fromIndex: Int, toIndex: Int): Boolean
    external fun nativeDuplicateLayer(handle: Long, index: Int): Boolean
    external fun nativeDeleteLayer(handle: Long, index: Int): Boolean
    external fun nativeRenameLayer(handle: Long, index: Int, name: String): Boolean
    external fun nativeResetDocument(handle: Long): Boolean
    external fun nativeCreateFrame(handle: Long, frameNumber: Int): Boolean
    external fun nativeSelectFrame(handle: Long, frameNumber: Int): Boolean
    external fun nativeFrameCount(handle: Long): Int
    external fun nativeFrameEnd(handle: Long): Int
    external fun nativeFrameNumbers(handle: Long): IntArray
    external fun nativeInterpolateFrame(handle: Long, sourceFrame: Int, targetFrame: Int, resultFrame: Int, factor: Float): Boolean
    external fun nativeSelectFrameOrHold(handle: Long, frameNumber: Int): Boolean
    external fun nativeDuplicateFrame(handle: Long, sourceFrame: Int, targetFrame: Int): Boolean
    external fun nativeDeleteFrame(handle: Long, frameNumber: Int): Boolean
    external fun nativeSelectStroke(handle: Long, index: Int): Boolean
    external fun nativeStrokeCenter(handle: Long, index: Int): FloatArray?
    external fun nativeHitTestStroke(handle: Long, x: Float, y: Float, radius: Float): Int
    external fun nativeDeleteStroke(handle: Long, index: Int): Boolean
    external fun nativeDeleteLastStroke(handle: Long): Boolean
    external fun nativeDuplicateStroke(handle: Long, index: Int): Boolean
    external fun nativeTranslateStroke(handle: Long, index: Int, dx: Float, dy: Float, dz: Float): Boolean
    external fun nativeFlipStroke(handle: Long, index: Int): Boolean
    external fun nativeRotateStroke(handle: Long, index: Int, radians: Float): Boolean
    external fun nativeRotateStrokeAbout(handle: Long, index: Int, radians: Float, centerX: Float, centerY: Float): Boolean
    external fun nativeScaleStroke(handle: Long, index: Int, scaleX: Float, scaleY: Float): Boolean
    external fun nativeScaleStrokeAbout(handle: Long, index: Int, scaleX: Float, scaleY: Float, centerX: Float, centerY: Float): Boolean
    external fun nativeMirrorStroke(handle: Long, index: Int, mirrorX: Boolean, mirrorY: Boolean): Boolean
    external fun nativeMirrorStrokeAbout(handle: Long, index: Int, mirrorX: Boolean, mirrorY: Boolean, centerX: Float, centerY: Float): Boolean
    external fun nativeSubdivideStroke(handle: Long, index: Int, level: Int): Boolean
    external fun nativeCloseStroke(handle: Long, index: Int): Boolean
    external fun nativeTrimStroke(handle: Long, index: Int, from: Int, to: Int, keepSinglePoint: Boolean): Boolean
    external fun nativeTrimStrokeToIntersection(handle: Long, index: Int): Boolean
    external fun nativeSplitStroke(handle: Long, index: Int, beforeIndex: Int): Boolean
    external fun nativeCreatePrimitive(handle: Long, type: Int, x0: Float, y0: Float, x1: Float, y1: Float, startAngle: Float, endAngle: Float, segments: Int, materialIndex: Int, thickness: Float): Boolean
    external fun nativeGeneratePrimitivePreview(type: Int, x0: Float, y0: Float, x1: Float, y1: Float, startAngle: Float, endAngle: Float, segments: Int): FloatArray?
    external fun nativeCreatePolyline(handle: Long, pointsXY: FloatArray, count: Int, materialIndex: Int, thickness: Float, cyclic: Boolean): Boolean
    external fun nativeEraseAt(handle: Long, x: Float, y: Float, radius: Float): Boolean
    external fun nativeClearSelection(handle: Long)
    external fun nativeApplyEditCommand(handle: Long, command: Int, args: FloatArray = floatArrayOf()): Boolean
    external fun nativeLassoSelect(handle: Long, pointsXY: FloatArray, count: Int, additive: Boolean): Int
    external fun nativeGetPoint(handle: Long, strokeIndex: Int, pointIndex: Int): FloatArray?
    external fun nativeMaterialCount(handle: Long): Int
    external fun nativeCreateMaterial(handle: Long): Boolean
    external fun nativeSetMaterialColors(handle: Long, index: Int, stroke: FloatArray, fill: FloatArray): Boolean
    external fun nativeSetMaterialVisibility(handle: Long, index: Int, visible: Boolean): Boolean
    external fun nativeSetMaterialFillEnabled(handle: Long, index: Int, enabled: Boolean): Boolean
    external fun nativeSmoothStroke(handle: Long, index: Int, influence: Float, iterations: Int): Boolean
    external fun nativeSetOnionSkin(handle: Long, enabled: Boolean, before: Int, after: Int, opacity: Float): Boolean
    external fun nativeSetMultiframeEditing(handle: Long, enabled: Boolean): Boolean
    external fun nativeApplyBlenderModifier(handle: Long, strokeIndex: Int, name: String, factor: Float, iterations: Int): Boolean
    external fun nativeFillStroke(handle: Long, index: Int): Boolean
    external fun nativeHistoryReset(handle: Long): Boolean
    external fun nativeHistoryRecord(handle: Long): Boolean
    external fun nativeHistoryUndo(handle: Long): Boolean
    external fun nativeHistoryRedo(handle: Long): Boolean
    external fun nativeHistoryCanUndo(handle: Long): Boolean
    external fun nativeHistoryCanRedo(handle: Long): Boolean
}
