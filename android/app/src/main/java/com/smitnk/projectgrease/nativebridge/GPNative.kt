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
    external fun nativeGetGpHandle(rendererHandle: Long): Long
    external fun nativeResetDocumentEgl(rendererHandle: Long): Boolean
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
    external fun nativePickColorEglRenderer(handle: Long, x: Int, y: Int): Int
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
    // easingType/easingMode are PG_EASE_* (see ProjectGreaseSelect.EASE_*)
    external fun nativeInterpolateFrameEased(handle: Long, sourceFrame: Int, targetFrame: Int, resultFrame: Int, factor: Float, easingType: Int, easingMode: Int): Boolean
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
    /**
     * Blender 3.6.23 gpencil_primitive.c geometry (project_grease_blender_primitive.h).
     * type: ProjectGreasePrimitive id. anchorsXY: start,end[,cp1,cp2] for shapes, or
     * polyline vertices. edges <= 0 uses Blender's defaults. Returns x,y pairs.
     */
    external fun nativeGenerateBlenderPrimitive(type: Int, anchorsXY: FloatArray, edges: Int, flip: Boolean): FloatArray?
    // Scene-lite (project_grease_scene_lite.h): 3D reference scene for Line Art; its own handle.
    external fun nativeSceneLiteCreate(): Long
    external fun nativeSceneLiteFree(handle: Long)
    external fun nativeSceneLiteClear(handle: Long)
    external fun nativeSceneLiteLoadObj(handle: Long, text: ByteArray): Int
    /** objects, vertices, triangles, edges, loose edges */
    external fun nativeSceneLiteStats(handle: Long): IntArray?
    external fun nativeSceneLiteSetCamera(handle: Long, params: FloatArray): Boolean
    /** x0, y0, x1, y1 per visible mesh edge, Line Art frame-buffer coordinates (-1..1). */
    external fun nativeSceneLiteProjectEdges(handle: Long): FloatArray?
    /** Line Art (default settings, occlusion levels 0..levelEnd): x0, y0, x1, y1, occlusion, edge type per segment. */
    external fun nativeSceneLiteLineArt(handle: Long, levelEnd: Int): FloatArray?
    // Annotations (project_grease_annotations.h): command ids PG_ANNOT_CMD_* of project_grease_gp_bridge.h.
    external fun nativeAnnotationCommand(handle: Long, command: Int, args: FloatArray?): Int
    /** r, g, b, a, thickness (px), visible (1/0); null without a document. */
    external fun nativeAnnotationStyle(handle: Long): FloatArray?
    external fun nativeAnnotationDump(handle: Long): FloatArray?
    external fun nativeAnnotationLoad(handle: Long, data: FloatArray): Boolean
    external fun nativeCreatePolyline(handle: Long, pointsXY: FloatArray, count: Int, materialIndex: Int, thickness: Float, cyclic: Boolean): Boolean
    external fun nativeEraseAt(handle: Long, x: Float, y: Float, radius: Float): Boolean
    external fun nativeSoftEraseAt(handle: Long, x: Float, y: Float, radius: Float, strength: Float): Boolean
    external fun nativeClearSelection(handle: Long)
    external fun nativeApplyEditCommand(handle: Long, command: Int, args: FloatArray = floatArrayOf()): Boolean
    external fun nativeLassoSelect(handle: Long, pointsXY: FloatArray, count: Int, additive: Boolean): Int
    external fun nativeGetPoint(handle: Long, strokeIndex: Int, pointIndex: Int): FloatArray?
    external fun nativeSetPoint(
        handle: Long, strokeIndex: Int, pointIndex: Int,
        x: Float, y: Float, z: Float,
        pressure: Float, strength: Float, time: Float
    ): Boolean
    // Save/load state. Float layouts (see project_grease_gp_jni.cpp):
    //   stroke info   [material, thickness, cyclic, fillOpacity, fillR, fillG, fillB, fillA]
    //   layer info    [visible, locked, opacity]
    //   material info [strokeRGBA(4), fillRGBA(4), visible, fillEnabled]
    external fun nativeGetStrokeInfo(handle: Long, strokeIndex: Int): FloatArray?
    //   point color   [r, g, b, a]  (bGPDspoint.vert_color; alpha 0 = no vertex color)
    // nativeAddStroke: points = 6 floats per point, pointColors = 4 floats per point.
    external fun nativeAddStroke(handle: Long, points: FloatArray, count: Int, info: FloatArray, pointColors: FloatArray): Boolean
    external fun nativeGetPointColor(handle: Long, strokeIndex: Int, pointIndex: Int): FloatArray?
    external fun nativeGetLayerInfo(handle: Long, index: Int): FloatArray?
    external fun nativeGetLayerName(handle: Long, index: Int): String?
    external fun nativeSetLayerOpacity(handle: Long, index: Int, opacity: Float): Boolean
    // Vertex groups and point weights. nativeGetPointWeights returns [group, weight, ...] pairs.
    external fun nativeVertexGroupCount(handle: Long): Int
    external fun nativeVertexGroupName(handle: Long, group: Int): String?
    external fun nativeVertexGroupAdd(handle: Long, name: String): Int
    external fun nativeVertexGroupRemove(handle: Long, group: Int): Boolean
    external fun nativeVertexGroupRename(handle: Long, group: Int, name: String): Boolean
    external fun nativeVertexGroupActive(handle: Long): Int
    external fun nativeSetVertexGroupActive(handle: Long, group: Int): Boolean
    external fun nativeGetPointWeights(handle: Long, stroke: Int, point: Int): FloatArray?
    external fun nativeSetPointWeight(handle: Long, stroke: Int, point: Int, group: Int, weight: Float): Boolean
    // Weight Paint view of the renderer: group >= 0 tints strokes by that group's weights, -1 = normal.
    external fun nativeSetWeightView(handle: Long, group: Int): Boolean
    // Layer masks: names refer to layers; flags bit 0 = hidden, bit 1 = inverted (-1 = no such entry).
    external fun nativeLayerUseMask(handle: Long, layer: Int): Boolean
    external fun nativeSetLayerUseMask(handle: Long, layer: Int, enabled: Boolean): Boolean
    external fun nativeMaskCount(handle: Long, layer: Int): Int
    external fun nativeMaskAdd(handle: Long, layer: Int, maskLayer: Int): Boolean
    external fun nativeMaskRemove(handle: Long, layer: Int, index: Int): Boolean
    external fun nativeMaskName(handle: Long, layer: Int, index: Int): String?
    external fun nativeMaskFlags(handle: Long, layer: Int, index: Int): Int
    external fun nativeMaskSetFlags(handle: Long, layer: Int, index: Int, flags: Int): Boolean
    // Live modifier stack of a layer (project_grease_modifier_stack.h). nativeModifierGet returns
    // [type, enabled, params...]; nativeModifierSetParams takes the bare params array.
    external fun nativeModifierCount(handle: Long, layer: Int): Int
    external fun nativeModifierAdd(handle: Long, layer: Int, type: Int): Int
    external fun nativeModifierRemove(handle: Long, layer: Int, index: Int): Boolean
    external fun nativeModifierMove(handle: Long, layer: Int, from: Int, to: Int): Boolean
    external fun nativeModifierSetEnabled(handle: Long, layer: Int, index: Int, enabled: Boolean): Boolean
    external fun nativeModifierSetParams(handle: Long, layer: Int, index: Int, params: FloatArray): Boolean
    external fun nativeModifierGet(handle: Long, layer: Int, index: Int): FloatArray?
    external fun nativeModifierApply(handle: Long, layer: Int, index: Int): Boolean
    // Per-layer shader effects (project_grease_shader_fx.h). nativeFxGet returns [type, enabled, params...].
    external fun nativeFxCount(handle: Long, layer: Int): Int
    external fun nativeFxAdd(handle: Long, layer: Int, type: Int): Int
    external fun nativeFxRemove(handle: Long, layer: Int, index: Int): Boolean
    external fun nativeFxMove(handle: Long, layer: Int, from: Int, to: Int): Boolean
    external fun nativeFxSetEnabled(handle: Long, layer: Int, index: Int, enabled: Boolean): Boolean
    external fun nativeFxSetParams(handle: Long, layer: Int, index: Int, params: FloatArray): Boolean
    external fun nativeFxGet(handle: Long, layer: Int, index: Int): FloatArray?
    external fun nativeGetMaterialInfo(handle: Long, index: Int): FloatArray?
    external fun nativeMaterialCount(handle: Long): Int
    external fun nativeCreateMaterial(handle: Long): Boolean
    external fun nativeSetMaterialColors(handle: Long, index: Int, stroke: FloatArray, fill: FloatArray): Boolean
    external fun nativeSetMaterialVisibility(handle: Long, index: Int, visible: Boolean): Boolean
    external fun nativeSetMaterialFillEnabled(handle: Long, index: Int, enabled: Boolean): Boolean
    external fun nativeSmoothStroke(handle: Long, index: Int, influence: Float, iterations: Int): Boolean
    external fun nativeSculptAt(handle: Long, tool: Int, x: Float, y: Float, radius: Float, influence: Float): Boolean
    external fun nativeSetOnionSkin(handle: Long, enabled: Boolean, before: Int, after: Int, opacity: Float): Boolean
    external fun nativeSetMultiframeEditing(handle: Long, enabled: Boolean): Boolean
    external fun nativeApplyBlenderModifier(handle: Long, strokeIndex: Int, name: String, factor: Float, iterations: Int): Boolean
    external fun nativeApplyLegacyGeometry(handle: Long, strokeIndex: Int, type: Int,
                                            value0: Float, value1: Float, value2: Float,
                                            int0: Int, int1: Int, flag0: Boolean, flag1: Boolean): Boolean
    external fun nativeFillStroke(handle: Long, index: Int): Boolean
    external fun nativeHistoryReset(handle: Long): Boolean
    external fun nativeHistoryRecord(handle: Long): Boolean
    external fun nativeHistoryUndo(handle: Long): Boolean
    external fun nativeHistoryRedo(handle: Long): Boolean
    external fun nativeHistoryCanUndo(handle: Long): Boolean
    external fun nativeHistoryCanRedo(handle: Long): Boolean
}
