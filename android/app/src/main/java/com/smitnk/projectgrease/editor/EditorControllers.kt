package com.smitnk.projectgrease.editor

import kotlin.math.pow
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue

import com.smitnk.projectgrease.nativebridge.GPNative

// Fill boundary source (Blender brush fill_draw_mode): All, Strokes, Edit Lines.
const val FILL_BOUNDARY_ALL = 0
const val FILL_BOUNDARY_STROKES = 1
const val FILL_BOUNDARY_EDIT_LINES = 2

class NativeEditorBridge : ModifierNative, FxNative {
    var handle: Long = 0L
        private set
    fun attach(value: Long) { handle = value }
    fun historyReset() = handle != 0L && GPNative.nativeHistoryReset(handle)
    fun applyEditCommand(command: Int, args: FloatArray = floatArrayOf()) =
        handle != 0L && GPNative.nativeApplyEditCommand(handle, command, args)
    fun historyRecord() = handle != 0L && GPNative.nativeHistoryRecord(handle)
    fun historyUndo() = handle != 0L && GPNative.nativeHistoryUndo(handle)
    fun historyRedo() = handle != 0L && GPNative.nativeHistoryRedo(handle)
    fun historyCanUndo() = handle != 0L && GPNative.nativeHistoryCanUndo(handle)
    fun historyCanRedo() = handle != 0L && GPNative.nativeHistoryCanRedo(handle)
    fun detach() { handle = 0L }
    fun layerCount() = if (handle != 0L) GPNative.nativeLayerCount(handle) else 0
    fun setLayerVisibility(index: Int, visible: Boolean) = handle != 0L && GPNative.nativeSetLayerVisibility(handle, index, visible)
    fun setLayerLocked(index: Int, locked: Boolean) = handle != 0L && GPNative.nativeSetLayerLocked(handle, index, locked)
    fun moveLayer(from: Int, to: Int) = handle != 0L && GPNative.nativeMoveLayer(handle, from, to)
    fun duplicateLayer(index: Int) = handle != 0L && GPNative.nativeDuplicateLayer(handle, index)
    fun deleteLayer(index: Int) = handle != 0L && GPNative.nativeDeleteLayer(handle, index)
    fun renameLayer(index: Int, name: String) = handle != 0L && GPNative.nativeRenameLayer(handle, index, name)
    fun createLayer(name: String) = handle != 0L && GPNative.nativeCreateLayer(handle, name)
    fun selectLayer(index: Int) = handle != 0L && GPNative.nativeSelectLayer(handle, index)
    fun frameCount() = if (handle != 0L) GPNative.nativeFrameCount(handle) else 0
    fun frameEnd() = if (handle != 0L) GPNative.nativeFrameEnd(handle) else 1
    fun frameNumbers() = if (handle != 0L) GPNative.nativeFrameNumbers(handle) else intArrayOf()
    fun interpolateFrame(sourceFrame:Int,targetFrame:Int,resultFrame:Int,factor:Float,easingType:Int=0,easingMode:Int=0) =
        handle != 0L && GPNative.nativeInterpolateFrameEased(handle,sourceFrame,targetFrame,resultFrame,factor,easingType,easingMode)
    fun selectFrameOrHold(frame:Int) = handle != 0L && GPNative.nativeSelectFrameOrHold(handle,frame)
    fun render() = handle != 0L && GPNative.nativeRender(handle)
    fun duplicateFrame(sourceFrame:Int,targetFrame:Int)=handle != 0L && GPNative.nativeDuplicateFrame(handle,sourceFrame,targetFrame)
    fun deleteFrame(frameNumber:Int)=handle != 0L && GPNative.nativeDeleteFrame(handle,frameNumber)
    fun resetDocument() = handle != 0L && GPNative.nativeResetDocument(handle)
    fun createFrame(frame: Int) = handle != 0L && GPNative.nativeCreateFrame(handle, frame)
    fun beginStroke(material:Int, thickness:Float) = handle != 0L && GPNative.nativeBeginStroke(handle, material, thickness)
    fun addPoint(point:FloatArray) = handle != 0L && GPNative.nativeAddPoint(handle, point[0], point[1], point[2], point[3], point[4], point[5])
    fun endStroke() = handle != 0L && GPNative.nativeEndStroke(handle)
    fun selectFrame(frame: Int) = handle != 0L && GPNative.nativeSelectFrame(handle, frame)
    fun strokeCount() = if (handle != 0L) GPNative.nativeStrokeCount(handle) else 0
    fun pointCount() = if (handle != 0L) GPNative.nativePointCount(handle) else 0
    fun selectedPointCount() = if (handle != 0L) GPNative.nativeSelectedPointCount(handle) else 0
    fun selectStroke(index: Int) = handle != 0L && GPNative.nativeSelectStroke(handle, index)
    fun strokeCenter(index: Int) = if (handle != 0L) GPNative.nativeStrokeCenter(handle, index) else null
    fun hitTestStroke(x: Float, y: Float, radius: Float) =
        if (handle != 0L) GPNative.nativeHitTestStroke(handle, x, y, radius) else -1
    fun eraseAt(x: Float, y: Float, radius: Float) =
        handle != 0L && GPNative.nativeEraseAt(handle, x, y, radius)
    fun deleteStroke(index: Int) = handle != 0L && GPNative.nativeDeleteStroke(handle, index)
    fun deleteLastStroke() = handle != 0L && GPNative.nativeDeleteLastStroke(handle)
    fun duplicateStroke(index: Int) = handle != 0L && GPNative.nativeDuplicateStroke(handle, index)
    fun translateStroke(index: Int, dx: Float, dy: Float, dz: Float) = handle != 0L && GPNative.nativeTranslateStroke(handle, index, dx, dy, dz)
    fun flipStroke(index: Int) = handle != 0L && GPNative.nativeFlipStroke(handle, index)
    fun rotateStroke(index: Int, radians: Float) = handle != 0L && GPNative.nativeRotateStroke(handle, index, radians)
    fun rotateStrokeAbout(index: Int, radians: Float, centerX: Float, centerY: Float) =
        handle != 0L && GPNative.nativeRotateStrokeAbout(handle, index, radians, centerX, centerY)
    fun scaleStroke(index: Int, scaleX: Float, scaleY: Float) = handle != 0L && GPNative.nativeScaleStroke(handle, index, scaleX, scaleY)
    fun scaleStrokeAbout(index: Int, scaleX: Float, scaleY: Float, centerX: Float, centerY: Float) =
        handle != 0L && GPNative.nativeScaleStrokeAbout(handle, index, scaleX, scaleY, centerX, centerY)
    fun mirrorStroke(index: Int, mirrorX: Boolean, mirrorY: Boolean) = handle != 0L && GPNative.nativeMirrorStroke(handle, index, mirrorX, mirrorY)
    fun mirrorStrokeAbout(index: Int, mirrorX: Boolean, mirrorY: Boolean, centerX: Float, centerY: Float) =
        handle != 0L && GPNative.nativeMirrorStrokeAbout(handle, index, mirrorX, mirrorY, centerX, centerY)
    fun subdivideStroke(index: Int, level: Int) = handle != 0L && GPNative.nativeSubdivideStroke(handle, index, level)
    fun closeStroke(index: Int) = handle != 0L && GPNative.nativeCloseStroke(handle, index)
    fun trimStroke(index: Int, from: Int, to: Int, keepSinglePoint: Boolean) = handle != 0L && GPNative.nativeTrimStroke(handle, index, from, to, keepSinglePoint)
    fun trimStrokeToIntersection(index: Int) = handle != 0L && GPNative.nativeTrimStrokeToIntersection(handle, index)
    fun splitStroke(index: Int, beforeIndex: Int) = handle != 0L && GPNative.nativeSplitStroke(handle, index, beforeIndex)
    fun getPoint(stroke: Int, point: Int) = if (handle != 0L) GPNative.nativeGetPoint(handle, stroke, point) else null
    fun strokeInfo(stroke: Int) = if (handle != 0L) GPNative.nativeGetStrokeInfo(handle, stroke) else null
    fun addStroke(points: FloatArray, count: Int, info: FloatArray, pointColors: FloatArray) =
        handle != 0L && GPNative.nativeAddStroke(handle, points, count, info, pointColors)
    fun pointColor(stroke: Int, point: Int) = if (handle != 0L) GPNative.nativeGetPointColor(handle, stroke, point) else null
    fun layerInfo(index: Int) = if (handle != 0L) GPNative.nativeGetLayerInfo(handle, index) else null
    fun layerName(index: Int) = if (handle != 0L) GPNative.nativeGetLayerName(handle, index) else null
    fun setLayerOpacity(index: Int, opacity: Float) = handle != 0L && GPNative.nativeSetLayerOpacity(handle, index, opacity)
    fun vertexGroupCount() = if (handle != 0L) GPNative.nativeVertexGroupCount(handle) else 0
    fun vertexGroupName(group: Int) = if (handle != 0L) GPNative.nativeVertexGroupName(handle, group) else null
    fun vertexGroupAdd(name: String) = if (handle != 0L) GPNative.nativeVertexGroupAdd(handle, name) else -1
    fun vertexGroupRemove(group: Int) = handle != 0L && GPNative.nativeVertexGroupRemove(handle, group)
    fun vertexGroupRename(group: Int, name: String) = handle != 0L && GPNative.nativeVertexGroupRename(handle, group, name)
    fun vertexGroupActive() = if (handle != 0L) GPNative.nativeVertexGroupActive(handle) else -1
    fun setVertexGroupActive(group: Int) = handle != 0L && GPNative.nativeSetVertexGroupActive(handle, group)
    fun pointWeights(stroke: Int, point: Int) = if (handle != 0L) GPNative.nativeGetPointWeights(handle, stroke, point) else null
    fun setPointWeight(stroke: Int, point: Int, group: Int, weight: Float) =
        handle != 0L && GPNative.nativeSetPointWeight(handle, stroke, point, group, weight)
    fun layerUseMask(layer: Int) = handle != 0L && GPNative.nativeLayerUseMask(handle, layer)
    fun setLayerUseMask(layer: Int, enabled: Boolean) = handle != 0L && GPNative.nativeSetLayerUseMask(handle, layer, enabled)
    fun maskCount(layer: Int) = if (handle != 0L) GPNative.nativeMaskCount(handle, layer) else 0
    fun maskAdd(layer: Int, maskLayer: Int) = handle != 0L && GPNative.nativeMaskAdd(handle, layer, maskLayer)
    fun maskRemove(layer: Int, index: Int) = handle != 0L && GPNative.nativeMaskRemove(handle, layer, index)
    fun maskName(layer: Int, index: Int) = if (handle != 0L) GPNative.nativeMaskName(handle, layer, index) else null
    fun maskFlags(layer: Int, index: Int) = if (handle != 0L) GPNative.nativeMaskFlags(handle, layer, index) else -1
    fun maskSetFlags(layer: Int, index: Int, flags: Int) = handle != 0L && GPNative.nativeMaskSetFlags(handle, layer, index, flags)
    override fun modifierCount(layer: Int) = if (handle != 0L) GPNative.nativeModifierCount(handle, layer) else 0
    override fun modifierAdd(layer: Int, type: Int) = if (handle != 0L) GPNative.nativeModifierAdd(handle, layer, type) else -1
    override fun modifierRemove(layer: Int, index: Int) = handle != 0L && GPNative.nativeModifierRemove(handle, layer, index)
    override fun modifierMove(layer: Int, from: Int, to: Int) = handle != 0L && GPNative.nativeModifierMove(handle, layer, from, to)
    override fun modifierSetEnabled(layer: Int, index: Int, enabled: Boolean) =
        handle != 0L && GPNative.nativeModifierSetEnabled(handle, layer, index, enabled)
    override fun modifierSetParams(layer: Int, index: Int, params: FloatArray) =
        handle != 0L && GPNative.nativeModifierSetParams(handle, layer, index, params)
    override fun modifierGet(layer: Int, index: Int) = if (handle != 0L) GPNative.nativeModifierGet(handle, layer, index) else null
    override fun modifierApply(layer: Int, index: Int) = handle != 0L && GPNative.nativeModifierApply(handle, layer, index)
    override fun fxCount(layer: Int) = if (handle != 0L) GPNative.nativeFxCount(handle, layer) else 0
    override fun fxAdd(layer: Int, type: Int) = if (handle != 0L) GPNative.nativeFxAdd(handle, layer, type) else -1
    override fun fxRemove(layer: Int, index: Int) = handle != 0L && GPNative.nativeFxRemove(handle, layer, index)
    override fun fxMove(layer: Int, from: Int, to: Int) = handle != 0L && GPNative.nativeFxMove(handle, layer, from, to)
    override fun fxSetEnabled(layer: Int, index: Int, enabled: Boolean) =
        handle != 0L && GPNative.nativeFxSetEnabled(handle, layer, index, enabled)
    override fun fxSetParams(layer: Int, index: Int, params: FloatArray) =
        handle != 0L && GPNative.nativeFxSetParams(handle, layer, index, params)
    override fun fxGet(layer: Int, index: Int) = if (handle != 0L) GPNative.nativeFxGet(handle, layer, index) else null
    override fun fxSetTarget(layer: Int, index: Int, target: Int) = handle != 0L && GPNative.nativeFxSetTarget(handle, layer, index, target)
    override fun fxTarget(layer: Int, index: Int) = if (handle != 0L) GPNative.nativeFxGetTarget(handle, layer, index) else FxTarget.LAYER
    fun materialInfo(index: Int) = if (handle != 0L) GPNative.nativeGetMaterialInfo(handle, index) else null
    /** Batch 21 document query (PG_DOC_Q_*: 0 frames, 1 layer, 2 material, 3 onion). */
    fun docQuery(what: Int, vararg args: Float) = if (handle != 0L) GPNative.nativeDocQuery(handle, what, args) else null
    fun materialName(slot: Int) = if (handle != 0L) GPNative.nativeMaterialName(handle, slot) else null
    fun setMaterialName(slot: Int, name: String) = handle != 0L && GPNative.nativeSetMaterialName(handle, slot, name)
    fun fillStroke(index: Int) = handle != 0L && GPNative.nativeFillStroke(handle, index)
    fun materialCount() = if (handle != 0L) GPNative.nativeMaterialCount(handle) else 0
    fun createMaterial() = handle != 0L && GPNative.nativeCreateMaterial(handle)
    fun setMaterialColors(index:Int, stroke:FloatArray, fill:FloatArray) = handle != 0L && GPNative.nativeSetMaterialColors(handle,index,stroke,fill)
    fun setMaterialVisibility(index:Int, visible:Boolean) = handle != 0L && GPNative.nativeSetMaterialVisibility(handle,index,visible)
    fun setMaterialFillEnabled(index:Int, enabled:Boolean) = handle != 0L && GPNative.nativeSetMaterialFillEnabled(handle,index,enabled)
    fun smoothStroke(index:Int,influence:Float=0.5f,iterations:Int=2) = handle != 0L && GPNative.nativeSmoothStroke(handle,index,influence,iterations)
    fun setOnionSkin(enabled:Boolean,before:Int,after:Int,opacity:Float) = handle != 0L && GPNative.nativeSetOnionSkin(handle,enabled,before,after,opacity)
    fun setMultiframeEditing(enabled:Boolean) = handle != 0L && GPNative.nativeSetMultiframeEditing(handle,enabled)
    fun applyModifier(strokeIndex:Int,name:String,factor:Float=0.5f,iterations:Int=2):Boolean =
        handle != 0L && GPNative.nativeApplyBlenderModifier(handle,strokeIndex,name,factor,iterations)
    fun legacyGeometry(strokeIndex:Int, type:Int, value0:Float=0f, value1:Float=0f, value2:Float=0f,
                       int0:Int=0, int1:Int=0, flag0:Boolean=false, flag1:Boolean=false):Boolean =
        handle != 0L && GPNative.nativeApplyLegacyGeometry(
            handle, strokeIndex, type, value0, value1, value2, int0, int1, flag0, flag1)
    fun reverseStroke(index:Int) = legacyGeometry(index, 14)
    fun uniformSubdivideStroke(index:Int, targetPoints:Int) =
        legacyGeometry(index, 15, int0=targetPoints, flag0=true)
    fun shrinkStroke(index:Int, distance:Float, mode:Int) =
        legacyGeometry(index, 16, value0=distance, int0=mode)
    fun randomizeStrokeColor(index:Int) = legacyGeometry(index, 17)
}

class HistoryController(private val native: NativeEditorBridge) {
    val canUndo: Boolean get() = native.historyCanUndo()
    val canRedo: Boolean get() = native.historyCanRedo()

    /**
     * Document state kept on the Kotlin side (material texture settings and images) is snapshotted
     * with every native step and restored with it, so undo/redo cover it like the Blender data.
     */
    var captureExtras: () -> Any? = { null }
    var restoreExtras: (Any?) -> Unit = {}
    private val undoExtras = ArrayList<Any?>()
    private val redoExtras = ArrayList<Any?>()

    // History is the actual Blender Legacy GP datablock state, not a UI flag.
    fun markEdit(): Boolean {
        if (batching) { batchChanged = true; return true }
        return record()
    }
    private fun record(): Boolean {
        val ok = native.historyRecord()
        if (ok) {
            undoExtras += captureExtras(); redoExtras.clear()
            while (undoExtras.size > MAX_STEPS) undoExtras.removeAt(0)
        }
        return ok
    }
    private var batching = false
    private var batchChanged = false
    /**
     * One undo step for a whole gesture (Blender's modal transform pushes a single undo on confirm):
     * edits between beginBatch and endBatch are recorded once, at the end.
     */
    fun beginBatch() { batching = true; batchChanged = false }
    fun endBatch(): Boolean {
        if (!batching) return false
        batching = false
        val changed = batchChanged
        batchChanged = false
        return if (changed) record() else false
    }
    fun reset(): Boolean {
        undoExtras.clear(); redoExtras.clear()
        return native.historyReset().also { if (it) undoExtras += captureExtras() }
    }
    fun undo(): Boolean {
        if (!native.historyUndo()) return false
        if (undoExtras.size >= 2) { redoExtras += undoExtras.removeAt(undoExtras.size - 1); restoreExtras(undoExtras.last()) }
        return true
    }
    fun redo(): Boolean {
        if (!native.historyRedo()) return false
        if (redoExtras.isNotEmpty()) { val e = redoExtras.removeAt(redoExtras.size - 1); undoExtras += e; restoreExtras(e) }
        return true
    }
    companion object { const val MAX_STEPS = 64 } // kMaxHistory of the native history
}

class DocumentController {
    var projectName = "Project Grease"
    var canvasWidth = 1280
    var canvasHeight = 720
    var dirty = false; private set
    fun markDirty() { dirty = true }
    fun markSaved() { dirty = false }
}

class AnimationController(private val native: NativeEditorBridge, private val renderFrame: () -> Unit) {
    // Snapshot state: the timeline composable is skipped under strong skipping unless the values it
    // reads are observable, so "+ Frame"/playback would otherwise leave the counter at "Frame 1 / 1".
    var currentFrame by androidx.compose.runtime.mutableIntStateOf(1); private set
    private val fpsState = androidx.compose.runtime.mutableIntStateOf(12)
    val fps: Int get() = fpsState.intValue
    var playing by androidx.compose.runtime.mutableStateOf(false); private set
    var loop by androidx.compose.runtime.mutableStateOf(true); private set
    var frameCount by androidx.compose.runtime.mutableIntStateOf(1); private set
    var timelineEnd by androidx.compose.runtime.mutableIntStateOf(1); private set
    /** Native keyframe numbers, re-read after every frame operation. */
    var keyframes by androidx.compose.runtime.mutableStateOf(IntArray(0)); private set
    /** bGPDframe.key_type and GP_FRAME_SELECT of the active layer's keyframes, by frame number. */
    var keyTypes by androidx.compose.runtime.mutableStateOf(emptyMap<Int, Int>()); private set
    var selectedFrames by androidx.compose.runtime.mutableStateOf(emptySet<Int>()); private set
    fun refreshKeyInfo() {
        val q = native.docQuery(0, -1f) ?: FloatArray(0)
        val types = LinkedHashMap<Int, Int>(); val sel = HashSet<Int>()
        var i = 0
        while (i + 2 < q.size) { val f = q[i].toInt(); types[f] = q[i + 1].toInt(); if (q[i + 2] != 0f) sel += f; i += 3 }
        keyTypes = types; selectedFrames = sel
    }
    /** Scene end frame (a template's frame_end); the timeline shows at least this many frames. 0 = none. */
    private val sceneEndState = androidx.compose.runtime.mutableIntStateOf(0)
    val sceneEnd: Int get() = sceneEndState.intValue
    fun setSceneEnd(value:Int) { sceneEndState.intValue = value.coerceIn(0, 100000); timelineEnd = endFrame() }
    private fun endFrame() = maxOf(native.frameEnd(), sceneEnd).coerceAtLeast(1)
    private val handler = android.os.Handler(android.os.Looper.getMainLooper())
    private val tick = object : Runnable {
        override fun run() {
            if (!playing || native.handle == 0L) return
            val end = endFrame()
            timelineEnd = end
            // screen_animation_step: wraps within the preview range when it is on (PRVRANGEON).
            val next = TimelineRules.nextPlaybackFrame(currentFrame, timeline.preview, end, loop)
            if (next == null) {
                playing = false
                return
            }
            if (native.selectFrameOrHold(next)) {
                currentFrame = next
                frameCount = native.frameCount().coerceAtLeast(1)
                renderFrame()
            }
            if (playing) handler.postDelayed(this, (1000L / fps.coerceIn(1,120)).coerceAtLeast(1L))
        }
    }

    fun initialize() {
        if (native.handle != 0L) {
            native.selectFrameOrHold(1)
            currentFrame = 1
            frameCount = native.frameCount().coerceAtLeast(1)
            timelineEnd = endFrame(); keyframes = native.frameNumbers(); refreshKeyInfo()
        }
    }
    fun setFrame(value: Int): Boolean {
        val target = value.coerceAtLeast(1)
        if (native.handle == 0L) return false
        if (!native.selectFrameOrHold(target)) return false
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        timelineEnd = endFrame(); keyframes = native.frameNumbers(); refreshKeyInfo()
        return true
    }
    fun ensureFrame(frameNumber: Int): Boolean {
        val target = frameNumber.coerceAtLeast(1)
        if (native.handle == 0L) return false
        if (native.selectFrame(target)) {
            currentFrame = target
            frameCount = native.frameCount().coerceAtLeast(1)
            timelineEnd = endFrame(); keyframes = native.frameNumbers(); refreshKeyInfo()
            return true
        }
        if (!native.createFrame(target)) return false
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        timelineEnd = endFrame(); keyframes = native.frameNumbers(); refreshKeyInfo()
        return true
    }
    fun duplicateFrame(sourceFrame:Int,targetFrame:Int):Boolean {
        if (native.handle == 0L || targetFrame < 1) return false
        if (!native.duplicateFrame(sourceFrame,targetFrame)) return false
        currentFrame=targetFrame; frameCount=native.frameCount().coerceAtLeast(1); timelineEnd = endFrame(); keyframes = native.frameNumbers(); refreshKeyInfo(); return true
    }
    fun frameNumbers(): IntArray = native.frameNumbers().also { keyframes = it; refreshKeyInfo() }
    /** Re-reads frame count/end and re-selects the current frame (or its hold) after native frame edits. */
    fun refreshFromNative(): Boolean = setFrame(currentFrame)

    /** Interpolation easing (Blender's gpencil_interpolate easing): type Linear..Bounce, mode In/Out/In-Out. */
    var easingType = ProjectGreaseSelect.EASE_LINEAR
        private set
    var easingMode = ProjectGreaseSelect.EASE_IN
        private set
    fun setEasing(type:Int, mode:Int) {
        if (type in ProjectGreaseSelect.EASE_LINEAR..ProjectGreaseSelect.EASE_ELASTIC) easingType = type
        if (mode in ProjectGreaseSelect.EASE_IN..ProjectGreaseSelect.EASE_IN_OUT) easingMode = mode
    }
    /** Elastic easing amplitude / period (GPENCIL_OT_interpolate defaults 0.15 / 0.15). */
    var elasticAmplitude = 0.15f; private set
    var elasticPeriod = 0.15f; private set
    fun setElastic(amplitude:Float, period:Float) {
        elasticAmplitude = amplitude.coerceIn(0f, 10f); elasticPeriod = period.coerceIn(0f, 10f)
        ProjectGreaseSelect.easingParams(elasticAmplitude, elasticPeriod).let { native.applyEditCommand(it.id, it.args) }
    }
    /**
     * GPENCIL_OT_interpolate_sequence: every frame strictly between the keyframe before [frame] and the
     * one after gets an in-between (step 1), each eased with the current easing; existing frames in the
     * gap are replaced as Blender does. Returns the number of frames created (one undo step).
     */
    /** GPENCIL_OT_interpolate_sequence options: flip (0 none, 1 always, 2 auto), step, smoothing,
     *  only selected (Edit mode), exclude breakdowns. */
    var interpolateFlip = ProjectGreaseSelect.INTERP_FLIP_AUTO
    var interpolateStep = 1
    var interpolateSmoothFactor = 0f
    var interpolateSmoothSteps = 1
    var interpolateOnlySelected = false
    var interpolateExcludeBreakdowns = false
    /** Blender interpolates between the keys before and after the current frame; on a key the gap
     *  after it is filled (the frame just after the key is used as the current frame). */
    private fun interpolationFrame(frame:Int):Int = if (frame in native.frameNumbers()) frame + 1 else frame
    private fun runInterpolation(frame:Int, single:Boolean):Int {
        if (native.handle == 0L) return 0
        ProjectGreaseSelect.easingParams(elasticAmplitude, elasticPeriod).let { native.applyEditCommand(it.id, it.args) }
        val before = native.frameNumbers().toSet()
        val c = ProjectGreaseSelect.interpolate(frame, interpolateStep, interpolateFlip, interpolateOnlySelected,
            interpolateExcludeBreakdowns, easingType, easingMode, interpolateSmoothFactor, interpolateSmoothSteps, single)
        if (!native.applyEditCommand(c.id, c.args)) return 0
        val made = native.frameNumbers().count { it !in before }.coerceAtLeast(if (single) 1 else 0)
        frameCount = native.frameCount().coerceAtLeast(1)
        timelineEnd = endFrame(); keyframes = native.frameNumbers(); refreshKeyInfo()
        return made
    }
    fun interpolateSequence(frame:Int = currentFrame):Int {
        val made = runInterpolation(interpolationFrame(frame), single = false)
        currentFrame = frame.coerceAtLeast(1)
        native.selectFrameOrHold(currentFrame)
        return made
    }
    fun interpolateAt(frame:Int):Boolean {
        if (frame in native.frameNumbers()) return false
        if (runInterpolation(frame, single = true) <= 0) return false
        currentFrame = frame
        native.selectFrameOrHold(currentFrame)
        return true
    }
    fun deleteFrame(frameNumber:Int):Boolean {
        if (native.handle == 0L) return false
        if (native.frameCount() <= 1) return false
        if (!native.deleteFrame(frameNumber)) return false
        val remaining = native.frameNumbers().sorted()
        val target = remaining.lastOrNull { it <= frameNumber }
            ?: remaining.firstOrNull()
            ?: 1
        native.selectFrameOrHold(target)
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        timelineEnd = endFrame(); keyframes = native.frameNumbers(); refreshKeyInfo()
        return true
    }

    fun setFps(value:Int){
        fpsState.intValue=value.coerceIn(1,120)
        if(playing){handler.removeCallbacks(tick);handler.postDelayed(tick,(1000L/fps).coerceAtLeast(1L))}
    }
    fun togglePlayback(){
        playing=!playing
        handler.removeCallbacks(tick)
        if(playing) handler.post(tick)
    }
    fun toggleLoop(){loop=!loop}

    /** Scene markers and preview range; part of every undo step and saved in the project file. */
    var timeline by androidx.compose.runtime.mutableStateOf(TimelineState()); private set
    fun restoreTimeline(state: TimelineState) { timeline = state }
    /** Scrubbing snaps to the nearest keyframe instead of the nearest frame when on (UI option). */
    var scrubSnapToKeys by androidx.compose.runtime.mutableStateOf(false)
    /** ANIM_OT_change_frame: [position] is a fractional frame; it is rounded (or key-snapped) first. */
    fun scrubTo(position: Float): Boolean {
        val frame = TimelineRules.scrubFrame(position, 1, maxOf(timelineEnd, endFrame()), keyframes, scrubSnapToKeys)
        return frame == currentFrame || setFrame(frame)
    }
    /** Applies a marker / preview-range rule result; false (nothing changed) when the rule refused. */
    fun applyTimeline(state: TimelineState?): Boolean {
        if (state == null || state == timeline) return false
        timeline = state
        return true
    }
    fun stop(){playing=false;handler.removeCallbacks(tick)}
}

class MaterialController {
    // Closed primitives are outline-only until Fill is explicitly enabled.
    var fillEnabled=false; private set
    var activeMaterial=0; private set
    var thickness=8f; private set
    var opacity=1f; private set
    var colorArgb:Int=0xFF202124.toInt(); private set
    fun select(index:Int){activeMaterial=index.coerceAtLeast(0)}
    fun setColor(value:Int){colorArgb=value}
    fun setFillEnabled(value:Boolean){fillEnabled=value}
    fun setThickness(value:Float){thickness=value.coerceIn(0.5f,500f)}
    fun setOpacity(value:Float){opacity=value.coerceIn(0f,1f)}
}

enum class GreaseMode { DRAW, EDIT, SCULPT, VERTEX_PAINT, WEIGHT_PAINT }

/**
 * Legacy GP default brushes (BKE_gpencil_brush_preset_set, brush.cc of the pinned tree): size,
 * draw_strength, pressure / strength-pressure switches, input samples, active smoothing, angle,
 * hardness and the curve_sensitivity / curve_strength CurveMapping presets (brush_gpencil_curvemap_reset).
 */
enum class BrushPreset(
    val label: String, val size: Float, val strength: Float, val usePressure: Boolean, val useStrengthPressure: Boolean,
    val inputSamples: Int = 10, val activeSmooth: Float = 0.35f, val angle: Float = 0f, val angleFactor: Float = 0f,
    val hardness: Float = 1f, val randomPressure: Float = 0f,
    val pressureCurve: List<Pair<Float, Float>> = listOf(0f to 0f, 1f to 1f), val strengthCurve: List<Pair<Float, Float>> = listOf(0f to 0f, 1f to 1f),
    val kind: Kind = Kind.DRAW, val eraser: EraserMode? = null
) {
    PENCIL("Pencil", 20f, 0.6f, true, true),
    PENCIL_SOFT("Pencil Soft", 80f, 0.4f, true, true, hardness = 0.8f),
    INK_PEN("Ink Pen", 60f, 1.0f, true, false, pressureCurve = listOf(0f to 0f, 0.63448f to 0.375f, 1f to 1f)),
    INK_PEN_ROUGH("Ink Pen Rough", 60f, 1.0f, true, false, randomPressure = 0.6f,
        pressureCurve = listOf(0f to 0f, 0.55f to 0.45f, 0.85f to 1f)),
    MARKER_BOLD("Marker Bold", 150f, 0.3f, false, false,
        pressureCurve = listOf(0f to 0f, 0.38f to 0.22f, 0.65f to 0.68f, 1f to 1f)),
    MARKER_CHISEL("Marker Chisel", 150f, 1.0f, true, false, activeSmooth = 0.3f,
        angle = (35.0 * Math.PI / 180.0).toFloat(), angleFactor = 0.5f,
        pressureCurve = listOf(0f to 0f, 0.25f to 0.40f, 1f to 1f),
        strengthCurve = listOf(0f to 0f, 0.31f to 0.22f, 0.61f to 0.88f, 1f to 1f)),
    PEN("Pen", 25f, 1.0f, false, false),
    AIRBRUSH("Airbrush", 300f, 0.4f, true, true, hardness = 0.9f),
    FILL_AREA("Fill Area", 5f, 1.0f, false, false, kind = Kind.FILL),
    ERASER_SOFT("Eraser Soft", 30f, 0.5f, true, true, kind = Kind.ERASE, eraser = EraserMode.SOFT),
    ERASER_HARD("Eraser Hard", 30f, 1.0f, false, false, kind = Kind.ERASE, eraser = EraserMode.SOFT),
    ERASER_POINT("Eraser Point", 30f, 1.0f, false, false, kind = Kind.ERASE, eraser = EraserMode.HARD),
    ERASER_STROKE("Eraser Stroke", 30f, 1.0f, false, false, kind = Kind.ERASE, eraser = EraserMode.STROKE);

    enum class Kind { DRAW, FILL, ERASE }
    companion object { val LINEAR_CURVE = listOf(0f to 0f, 1f to 1f) }
}

class BrushController(private val materials: MaterialController) {
    var preset = BrushPreset.PENCIL
        private set
    /** Brush Size is the stroke thickness (one value, as Blender's brush size): every thickness slider,
     *  preset and the Size slider read and write materials.thickness, so the label always matches. */
    val size: Float get() = materials.thickness
    var strength = 1f
        private set
    /** Former power-curve exponent; kept for old callers, the curves below are what Draw uses. */
    var pressureCurve = 1f
        private set
    var usePressure = true; private set
    var useStrengthPressure = false; private set
    var inputSamples = 10; private set
    var activeSmooth = 0.35f; private set // ACTIVE_SMOOTH of the default Pencil preset (BKE_gpencil_brush_preset_set)
    var angle = 0f; private set
    var angleFactor = 0f; private set
    var hardness = 1f; private set
    /** curve_sensitivity and curve_strength (CurveMapping points, evaluated natively). */
    var pressureCurvePoints: List<Pair<Float, Float>> = BrushPreset.LINEAR_CURVE; private set
    var strengthCurvePoints: List<Pair<Float, Float>> = BrushPreset.LINEAR_CURVE; private set

    fun select(value: BrushPreset) {
        preset = value
        strength = value.strength
        usePressure = value.usePressure
        useStrengthPressure = value.useStrengthPressure
        inputSamples = value.inputSamples
        activeSmooth = value.activeSmooth
        angle = value.angle
        angleFactor = value.angleFactor
        hardness = value.hardness
        pressureCurvePoints = value.pressureCurve
        strengthCurvePoints = value.strengthCurve
        pressureCurve = 1f
        materials.setThickness(value.size)
    }

    fun setSize(value: Float) {
        materials.setThickness(value.coerceIn(0.5f, 500f))
    }

    fun setStrength(value: Float) {
        strength = value.coerceIn(0f, 1f)
    }

    fun setPressureCurve(value: Float) {
        pressureCurve = value.coerceIn(0.25f, 3f)
    }
    fun setUsePressure(value: Boolean) { usePressure = value }
    fun setUseStrengthPressure(value: Boolean) { useStrengthPressure = value }
    /** Points sorted by x, 2..8 of them, inside 0..1 (CurveMapping clip rect). */
    fun setPressureCurvePoints(points: List<Pair<Float, Float>>) { pressureCurvePoints = CurvePoints.clean(points) }
    fun setStrengthCurvePoints(points: List<Pair<Float, Float>>) { strengthCurvePoints = CurvePoints.clean(points) }

    fun pressure(input: Float): Float =
        input.coerceIn(0f, 1f).let { it.toDouble().pow(pressureCurve.toDouble()).toFloat() }
}

/** CurveMapping point lists as edited in the UI (2..8 points, sorted, clamped to 0..1). */
object CurvePoints {
    const val MAX = 8
    fun clean(points: List<Pair<Float, Float>>): List<Pair<Float, Float>> {
        val p = points.filter { it.first.isFinite() && it.second.isFinite() }
            .map { it.first.coerceIn(0f, 1f) to it.second.coerceIn(0f, 1f) }.sortedBy { it.first }.take(MAX)
        return if (p.size >= 2) p else BrushPreset.LINEAR_CURVE
    }
    /** Index of the point nearest (x, y) within [radius], or -1. */
    fun hit(points: List<Pair<Float, Float>>, x: Float, y: Float, radius: Float): Int {
        var best = -1; var bd = radius
        points.forEachIndexed { i, (px, py) -> val d = kotlin.math.hypot(px - x, py - y); if (d <= bd) { bd = d; best = i } }
        return best
    }
    /** Adds a point at x on the line between its neighbours (Blender adds where you click). */
    fun insert(points: List<Pair<Float, Float>>, x: Float, y: Float): List<Pair<Float, Float>> =
        if (points.size >= MAX) points else clean(points + (x to y))
    fun remove(points: List<Pair<Float, Float>>, index: Int): List<Pair<Float, Float>> =
        if (points.size <= 2 || index !in points.indices) points else points.filterIndexed { i, _ -> i != index }
}


class ViewController {
    var zoom=1f; private set
    var panX=0f; private set
    var panY=0f; private set
    var showGrid=false; private set
    var showGuides=false; private set
    var snapEnabled=false; private set
    var gridSize=32f; private set
    var guideX=0.5f; private set
    var guideY=0.5f; private set
    fun zoomBy(delta:Float){zoom=(zoom+delta).coerceIn(0.1f,8f)}
    fun setZoom(value:Float){zoom=value.coerceIn(0.1f,8f)}
    fun panBy(dx:Float,dy:Float){panX+=dx;panY+=dy}
    fun reset(){zoom=1f;panX=0f;panY=0f}
    /**
     * Fit canvas: the presenter (update_canvas_map) shows the canvas at 92% of the fitting scale
     * times the zoom, centered plus the pan, so zoom 1 without pan is the whole canvas in view with
     * a 4% margin on each side of the limiting axis.
     */
    fun fitCanvas(){zoom=FIT_ZOOM;panX=0f;panY=0f}
    companion object {
        const val FIT_ZOOM = 1f
        const val FIT_MARGIN = 0.04f
        /** Canvas rectangle (left, top, width, height) in viewport pixels for a zoom / pan, as the presenter lays it out. */
        fun canvasRect(viewW:Float, viewH:Float, canvasW:Float, canvasH:Float, zoom:Float, panX:Float, panY:Float):FloatArray {
            val scale = minOf(viewW/canvasW.coerceAtLeast(1f), viewH/canvasH.coerceAtLeast(1f)) * (1f - 2f*FIT_MARGIN) * zoom
            val w = canvasW*scale; val h = canvasH*scale
            return floatArrayOf((viewW-w)*0.5f+panX, (viewH-h)*0.5f+panY, w, h)
        }
    }
    /** Drawing guide (GP_GUIDE_*: 0 circular, 1 radial, 2 parallel, 3 grid, 4 isometric; -1 off), canvas units. */
    var guideType=-1; private set
    var guideCenterX=640f; private set
    var guideCenterY=360f; private set
    var guideAngle=0f; private set
    var guideSpacing=40f; private set
    fun setDrawingGuide(type:Int, centerX:Float=guideCenterX, centerY:Float=guideCenterY, angle:Float=guideAngle, spacing:Float=guideSpacing){
        guideType=if(type in 0..4) type else -1
        if(centerX.isFinite()) guideCenterX=centerX
        if(centerY.isFinite()) guideCenterY=centerY
        if(angle.isFinite()) guideAngle=angle
        if(spacing.isFinite()) guideSpacing=spacing.coerceIn(4f,1000f)
    }
    fun toggleGrid(){showGrid=!showGrid}
    fun toggleGuides(){showGuides=!showGuides}
    fun toggleSnapping(){snapEnabled=!snapEnabled}
    fun setGridSize(value:Float){gridSize=value.coerceIn(8f,256f)}
    fun setGuide(x:Float,y:Float){guideX=x.coerceIn(0f,1f);guideY=y.coerceIn(0f,1f)}
    fun snap(value:Float):Float {
        if(!snapEnabled) return value
        val size=gridSize.coerceAtLeast(1f)
        return kotlin.math.round(value/size)*size
    }
    fun snapPoint(x:Float,y:Float):Pair<Float,Float> = snap(x) to snap(y)
}

class SelectionController(private val native: NativeEditorBridge) {
    var hasSelection=false; private set
    var selectedStroke=-1; private set
    fun clear(){hasSelection=false; selectedStroke=-1}
    /** Remember the stroke under the last pick without touching the native selection. */
    fun note(index:Int){hasSelection=true; selectedStroke=index}
    fun selectStroke(index:Int):Boolean {
        val ok=native.selectStroke(index)
        if(ok){selectedStroke=index;hasSelection=true}
        return ok
    }
}

class ModifierController { val modifiers=mutableListOf<String>(); fun add(name:String){modifiers+=name}; fun removeAt(index:Int){if(index in modifiers.indices)modifiers.removeAt(index)} }
enum class EraserMode { HARD, SOFT, STROKE }
enum class SculptBrush {
    SMOOTH, THICKNESS, STRENGTH, GRAB, PUSH, PINCH, TWIST, RANDOMIZE, CLONE
}

/** Sculpt brush selection; the brushes themselves run natively (project_grease_tool_sculpt.c). */
class SculptController {
    var brush = SculptBrush.SMOOTH
        private set
    var invert = false
        private set

    fun isAvailable() =
        FeatureRegistry.capability(FeatureId.SCULPT).state != FeatureState.NOT_IMPLEMENTED

    fun select(value: SculptBrush) { brush = value }

    fun setInvert(value: Boolean) { invert = value }
    /** ToolSettings gp_sculpt.flag auto-masking bits, gpencil_selectmode_sculpt, brush curve preset. */
    var automask = 0
        private set
    var selectMask = 0
        private set
    var curvePreset = 0
        private set
    fun toggleAutomask(bit: Int) { automask = automask xor bit }
    fun setSelectMask(mask: Int) { selectMask = mask and 7 }
    fun setCurvePreset(preset: Int) { if (ToolSession.CURVE_PRESETS.any { it.first == preset }) curvePreset = preset }
}

class OnionSkinController {
    var enabled=false; private set
    var beforeFrames=2; private set
    var afterFrames=2; private set
    var opacity=0.35f; private set
    var fade=true; private set
    var selectedLayerOnly=true; private set
    /** bGPdata.onion_keytype (-1 = all, else BEZT_KEYTYPE_*) and GP_ONION_LOOP. */
    var keyTypeFilter=-1; private set
    var loop=false; private set
    fun setFilter(keyType:Int, loop:Boolean){ keyTypeFilter=keyType.coerceIn(-1,4); this.loop=loop }
    fun toggle(){enabled=!enabled}
    fun setBefore(value:Int){beforeFrames=value.coerceIn(0,12)}
    fun setAfter(value:Int){afterFrames=value.coerceIn(0,12)}
    fun setOpacity(value:Float){opacity=value.coerceIn(0f,1f)}
    fun setFade(value:Boolean){fade=value}
    // bGPdata.onion_mode and the ghost colours (Blender defaults: Relative, custom colours on,
    // gcolor_prev green, gcolor_next blue; BKE_gpencil_data_addnew).
    var mode=ProjectGreaseSelect.ONION_MODE_RELATIVE; private set
    var usePrevColor=true; private set
    var useNextColor=true; private set
    var prevColor=0xFF256B23.toInt(); private set
    var nextColor=0xFF201587.toInt(); private set
    fun setStyle(mode:Int, usePrev:Boolean, useNext:Boolean, prev:Int, next:Int){
        if(mode in ProjectGreaseSelect.ONION_MODE_ABSOLUTE..ProjectGreaseSelect.ONION_MODE_SELECTED) this.mode=mode
        usePrevColor=usePrev; useNextColor=useNext; prevColor=prev; nextColor=next
    }
}

/** Stroke or fill texture of a material slot (MaterialGPencilStyle texture settings + the picked image). */
data class MaterialTexture(
    val uri: String? = null, val enabled: Boolean = false, val mix: Float = 0f,
    val scaleX: Float = 1f, val scaleY: Float = 1f, val offsetX: Float = 0f, val offsetY: Float = 0f,
    val angle: Float = 0f, val pixelSize: Float = 100f
)

/** Tools that select or act on the selection: the edit overlay is shown while one is active. */
val SELECTION_TOOLS = setOf(GreaseTool.SELECT, GreaseTool.LASSO, GreaseTool.MOVE, GreaseTool.ROTATE,
    GreaseTool.SCALE, GreaseTool.MIRROR, GreaseTool.BOX_SELECT, GreaseTool.CIRCLE_SELECT)

enum class GreaseTool { DRAW, ERASE, SELECT, LASSO, FILL, EYEDROPPER, LINE, RECTANGLE, CIRCLE, ARC, POLYLINE, CURVE, ANNOTATE, MOVE, ROTATE, SCALE, MIRROR, PAN, SCULPT, BOX_SELECT, CIRCLE_SELECT }

class ToolController {
    var activeTool=GreaseTool.DRAW; private set
    fun select(tool:GreaseTool):Boolean {
        val feature=when(tool){
            GreaseTool.DRAW->FeatureId.FREEHAND; GreaseTool.ERASE->FeatureId.ERASER
            GreaseTool.SELECT->FeatureId.SELECT; GreaseTool.LASSO->FeatureId.LASSO
            GreaseTool.FILL->FeatureId.FILL; GreaseTool.EYEDROPPER->FeatureId.STROKE_COLOR
            GreaseTool.LINE->FeatureId.LINE; GreaseTool.RECTANGLE->FeatureId.RECTANGLE
            GreaseTool.CIRCLE->FeatureId.CIRCLE; GreaseTool.ARC->FeatureId.ARC
            GreaseTool.POLYLINE->FeatureId.POLYLINE; GreaseTool.CURVE->FeatureId.CURVE; GreaseTool.ANNOTATE->FeatureId.ANNOTATIONS; GreaseTool.MOVE->FeatureId.MOVE; GreaseTool.ROTATE->FeatureId.ROTATE; GreaseTool.SCALE->FeatureId.SCALE; GreaseTool.MIRROR->FeatureId.MIRROR; GreaseTool.PAN->FeatureId.PAN
            GreaseTool.SCULPT->FeatureId.SCULPT
            GreaseTool.BOX_SELECT->FeatureId.SELECT_BOX; GreaseTool.CIRCLE_SELECT->FeatureId.SELECT_CIRCLE
        }
        if(FeatureRegistry.capability(feature).state==FeatureState.NOT_IMPLEMENTED)return false
        activeTool=tool; return true
    }
}

class EditorController {
    private val native = NativeEditorBridge()
    var selectedLayer = 0
        private set
    val tools=ToolController()
    /** 3D reference scene for Line Art (Scene-lite): OBJ meshes + camera, previewed as a wireframe. */
    val reference=ReferenceScene()
    val document=DocumentController()
    val history=HistoryController(native).also { h ->
        // Kotlin-side state of an undo step: material textures plus the timeline (markers, preview range).
        h.captureExtras = { Pair(captureTextures(), animation.timeline) }
        h.restoreExtras = { e -> (e as? Pair<*, *>)?.let { restoreTextures(it.first); (it.second as? TimelineState)?.let(animation::restoreTimeline) } }
    }
    val animation=AnimationController(native) { render() }
    val materials=MaterialController()
    val brushes=BrushController(materials)
    var mode=GreaseMode.DRAW
        private set
    var multiframeEditing=false
        private set
    val view=ViewController()
    val selection=SelectionController(native)
    val modifiers=ModifierController()
    val sculpt=SculptController()
    val onion=OnionSkinController()
    private var rendererHandle=0L
    fun attachRenderer(handle:Long) {
        rendererHandle = handle
        val gpHandle = if (handle != 0L) GPNative.nativeGetGpHandle(handle) else 0L
        native.attach(gpHandle)
        native.historyReset()
        setMaterialColor(materials.colorArgb)
        animation.initialize()
        selectedLayer = 0
        reuploadTextures()
        // Document setup requested before the surface existed (New Project / open from Home): the
        // native document only exists once the EGL renderer is attached, so it runs now.
        if (rendererReady) {
            val pending = pendingDocumentSetup
            pendingDocumentSetup = null
            val snapshot = detachedSnapshot
            detachedSnapshot = null
            // A destroyed surface takes its native renderer and document with it, and a new surface
            // starts from an empty "Layer 1" document: restore the document as it was at detach.
            setupTrace("attach pending=${pending != null} snapshot=${snapshot != null} layers=${native.layerCount()}")
            if (pending != null) pending() else if (snapshot != null) loadDocumentJson(snapshot)
        }
    }
    /** The document saved when the surface went away (app in background, surface recreated). */
    private var detachedSnapshot: String? = null
    private var pendingDocumentSetup: (() -> Unit)? = null
    /**
     * Runs a document setup (template, project load) on the native document. Before the editor's
     * surface is attached there is no native document, and the setup used to be dropped silently
     * (New Project kept "Layer 1"); it is now kept and run on attach. The latest request wins.
     */
    fun runWhenAttached(setup: () -> Unit) {
        setupTrace("runWhenAttached ready=$rendererReady")
        if (rendererReady) setup() else pendingDocumentSetup = setup
    }
    /** Document setup trace (attach / template / restore steps), logged and read by device tests. */
    val setupLog = ArrayList<String>()
    fun setupTrace(msg: String) {
        setupLog += msg
        runCatching { android.util.Log.i("ProjectGrease", "setup: $msg") }
    }
    fun detachRenderer(){
        animation.stop()
        setupTrace("detach ready=$rendererReady")
        if (rendererReady) saveDocumentJson()?.let { detachedSnapshot = it }
        rendererHandle=0L;native.detach()
    }
    fun resetDocument():Boolean {
        if (rendererHandle == 0L) return false
        val ok=GPNative.nativeResetDocumentEgl(rendererHandle)
        setupTrace("resetDocument ok=$ok")
        if(ok){
            clearTextures()
            projectSettings = ProjectSettings()
            reapplyOnion()
            selectedLayer=0
            animation.setSceneEnd(0)
            animation.restoreTimeline(TimelineState())
            animation.initialize()
            history.reset()
            document.markDirty()
            render()
        }
        return ok
    }
    fun selectTool(tool:GreaseTool):Boolean {
        // Leaving the polyline tool confirms the polyline, like Blender's confirm keys.
        if (tool != tools.activeTool && polyline.isActive) finishPolyline()
        // Likewise for the curve once its handles are shown; a curve still being dragged is dropped.
        if (tool != tools.activeTool && curve.isActive) {
            if (curve.phase == CurveSession.Phase.EDIT) confirmCurve() else cancelCurve()
        }
        if (tool != tools.activeTool && shapeEdit.isActive) confirmShape()
        // Re-render so the edit overlay follows the tool at once.
        return tools.select(tool).also { if (it) render() }
    }
    // ---- Native tool session (ToolSession.kt / project_grease_tool_session.h) ----
    /** The session tool the current gesture uses, or -1. */
    private var sessionTool = -1
    private var sessionGestureChanged = false
    private var sessionSeed = 1

    /** Which native session tool a touch on the canvas runs, or -1 (other tools keep their path). */
    fun sessionToolForTouch():Int = when {
        gizmoActive -> -1 // modifier handles take the touch (beginStroke path)
        mode == GreaseMode.VERTEX_PAINT && paintsInMode() -> ToolSession.TOOL_VERTEX_PAINT
        mode == GreaseMode.WEIGHT_PAINT && paintsInMode() -> ToolSession.TOOL_WEIGHT_PAINT
        tools.activeTool == GreaseTool.SCULPT -> ToolSession.TOOL_SCULPT
        // Draw mode's Tint tool (GPAINT_TOOL_TINT) runs the vertex paint session
        tools.activeTool == GreaseTool.DRAW && mode == GreaseMode.DRAW && drawTint -> ToolSession.TOOL_VERTEX_PAINT
        tools.activeTool == GreaseTool.DRAW -> ToolSession.TOOL_DRAW
        else -> -1
    }
    /** Draw keeps the grid snapping of the former per-point path; the brushes use raw samples. */
    fun snapForTool(tool:Int, p:Pair<Float,Float>):Pair<Float,Float> =
        if (tool == ToolSession.TOOL_DRAW) view.snapPoint(p.first, p.second) else p
    private fun paintsInMode() = tools.activeTool != GreaseTool.PAN && tools.activeTool != GreaseTool.EYEDROPPER

    private fun sessionParams(tool:Int, pxPerUnit:Float):FloatArray? {
        val argb = materials.colorArgb
        val r = ((argb shr 16) and 0xFF) / 255f
        val g = ((argb shr 8) and 0xFF) / 255f
        val b = (argb and 0xFF) / 255f
        return when (tool) {
            ToolSession.TOOL_SCULPT -> ToolSession.brushParams(
                ToolSession.sculptTool(sculpt.brush), brushes.size.coerceAtLeast(0.5f),
                brushes.strength, pxPerUnit, sculpt.invert, seed = sessionSeed++, automask = sculpt.automask,
                selectMask = sculpt.selectMask, curvePreset = sculpt.curvePreset, activeMaterial = materials.activeMaterial)
            ToolSession.TOOL_VERTEX_PAINT -> ToolSession.brushParams(
                ToolSession.vertexTool(if (mode == GreaseMode.DRAW) ProjectGreaseSelect.VPAINT_TINT else vertexPaintBrush), brushes.size.coerceAtLeast(0.5f), brushes.strength,
                pxPerUnit, r = r, g = g, b = b, target = vertexPaintTarget, selectMask = vertexSelectMask,
                curvePreset = paintCurvePreset)
            ToolSession.TOOL_WEIGHT_PAINT -> ToolSession.brushParams(
                weightPaintBrush, brushes.size.coerceAtLeast(0.5f), brushes.strength, pxPerUnit,
                invert = weightPaintSubtract, target = weightPaintGroup, weight = weightPaintValue,
                curvePreset = paintCurvePreset)
            ToolSession.TOOL_DRAW -> ToolSession.DrawSettings(
                material = materials.activeMaterial, thickness = materials.thickness,
                strength = brushes.strength, usePressure = brushes.usePressure,
                useStrengthPressure = brushes.useStrengthPressure,
                pressureCurve = brushes.pressureCurve, inputSamples = legacyInputSamples,
                lazy = legacyLazyEnabled, lazyRadius = legacyLazyRadius, lazyFactor = legacyLazyFactor,
                disableStabilizer = legacyDisableStabilizer, manhattan = legacyManhattanThreshold,
                euclidean = legacyEuclideanThreshold, activeSmooth = maxOf(legacyActiveSmooth, brushes.activeSmooth),
                jitter = legacyJitter,
                angleFactor = if (legacyDrawAngleFactor != 0f) legacyDrawAngleFactor else brushes.angleFactor,
                angle = if (legacyDrawAngleFactor != 0f) legacyDrawAngle else brushes.angle,
                guideType = view.guideType, guideX = view.guideCenterX, guideY = view.guideCenterY,
                guideAngle = view.guideAngle, guideSpacing = view.guideSpacing,
                pressureCurvePoints = brushes.pressureCurvePoints, strengthCurvePoints = brushes.strengthCurvePoints,
                pxPerUnit = pxPerUnit
            ).toParams()
            else -> null
        }
    }

    /**
     * One input batch for the native tool session: [samples] holds [count] samples of
     * (x, y, pressure, time) in canvas units, every historical sample included. Native applies the
     * tool and renders once. The gesture's END records one undo step when it changed the document.
     */
    /** BKE_gpencil_layer_is_editable(): the active layer is neither locked nor hidden. */
    fun activeLayerEditable():Boolean = layerState()?.let { !it.locked && it.visible } ?: true

    fun toolSamples(tool:Int, samples:FloatArray, count:Int, phase:Int, pxPerUnit:Float = 1f):Int {
        if (rendererHandle == 0L || tool < 0) return 0
        if (phase == ToolSession.PHASE_BEGIN) {
            // gpencil_draw_init(): "Active layer is locked or hidden" cancels the stroke; the sculpt
            // and paint brushes skip non-editable layers (BKE_gpencil_layer_is_editable).
            if (!activeLayerEditable()) { sessionTool = -1; return 0 }
            if (tool == ToolSession.TOOL_WEIGHT_PAINT) syncWeightPaintGroup()
            sessionTool = tool
            sessionGestureChanged = false
        } else if (tool != sessionTool) {
            return 0
        }
        val params = if (phase == ToolSession.PHASE_BEGIN) sessionParams(tool, pxPerUnit) else null
        val result = GPNative.nativeToolSamples(rendererHandle, tool, ToolSession.pack(samples, count, params), count, phase)
        if ((result and ToolSession.RESULT_CHANGED) != 0 && tool != ToolSession.TOOL_DRAW) {
            sessionGestureChanged = true
            document.markDirty()
        }
        if (phase == ToolSession.PHASE_END || phase == ToolSession.PHASE_CANCEL) {
            if ((result and ToolSession.RESULT_ENDED) != 0 || sessionGestureChanged) {
                if (phase == ToolSession.PHASE_END) { history.markEdit(); document.markDirty() }
            }
            sessionTool = -1
            sessionGestureChanged = false
        }
        return result
    }

    /** Blender's preset brushes also pick their tool: Fill Area the fill tool, the erasers Erase. */
    fun selectBrush(preset:BrushPreset) {
        brushes.select(preset)
        when (preset.kind) {
            BrushPreset.Kind.FILL -> { selectTool(GreaseTool.FILL); setFillOptions(dilate = 1) }
            BrushPreset.Kind.ERASE -> { selectTool(GreaseTool.ERASE); preset.eraser?.let { setEraserMode(it) } }
            BrushPreset.Kind.DRAW -> if (tools.activeTool == GreaseTool.ERASE || tools.activeTool == GreaseTool.FILL) selectTool(GreaseTool.DRAW)
        }
        setMaterialColor(materials.colorArgb)
    }
    fun setBrushStrength(value:Float) {
        brushes.setStrength(value)
        setMaterialColor(materials.colorArgb)
    }
    fun setMode(value:GreaseMode):Boolean {
        val supported = when (value) {
            GreaseMode.DRAW, GreaseMode.EDIT -> true
            GreaseMode.SCULPT -> FeatureRegistry.capability(FeatureId.SCULPT).state != FeatureState.NOT_IMPLEMENTED
            GreaseMode.VERTEX_PAINT -> FeatureRegistry.capability(FeatureId.VERTEX_PAINT).state != FeatureState.NOT_IMPLEMENTED
            GreaseMode.WEIGHT_PAINT -> FeatureRegistry.capability(FeatureId.WEIGHT_PAINT).state != FeatureState.NOT_IMPLEMENTED
        }
        if (!supported) return false
        mode = value
        if (value == GreaseMode.WEIGHT_PAINT) syncWeightPaintGroup()
        if (value == GreaseMode.EDIT && tools.activeTool == GreaseTool.DRAW) {
            tools.select(GreaseTool.SELECT)
        }
        return true
    }
    private data class PendingPoint(val x:Float,val y:Float,val pressure:Float,val time:Float)
    private val pendingShapePoints = mutableListOf<PendingPoint>()
    // Snapshot list: the UI draws the noose live while the lasso is dragged (path feedback).
    private val pendingLassoPoints = androidx.compose.runtime.mutableStateListOf<Pair<Float,Float>>()
    /** The lasso path being drawn, canvas units (empty when no lasso is open). */
    val lassoPath: List<Pair<Float,Float>> get() = pendingLassoPoints
    private var pendingShapeTool: GreaseTool? = null
    private var lastShapeAnchors: Pair<Pair<Float,Float>,Pair<Float,Float>>? = null
    private val polyline = PolylineSession()
    private val curve = CurveSession()
    private var curveAwaitingPress = false
    private var curveConfirmOnRelease = false
    private var curveLastX = 0f
    private var curveLastY = 0f
    /** Called when the curve handles change, so the UI can redraw its handle overlay. */
    var onOverlayChanged: (() -> Unit)? = null
    /** Curve handles in canvas space (start, end, cp1, cp2) while they can be edited, else empty. */
    fun curveHandles(): List<Pair<Float, Float>> = curve.handles()
    val curveEditing: Boolean get() = curve.phase == CurveSession.Phase.EDIT
    /** Commits the edited curve (gpencil_primitive.c confirm). */
    fun confirmCurve(): Boolean {
        val anchors = curve.anchors()
        curve.reset()
        curveAwaitingPress = false
        curveConfirmOnRelease = false
        showPrimitivePreview(null)
        onOverlayChanged?.invoke()
        val points = blenderPrimitivePoints(ProjectGreasePrimitive.CURVE, anchors)
        val ok = points != null && commitPrimitivePoints(points, false)
        if (!ok) render()
        return ok
    }
    /** Discards the curve being edited. */
    fun cancelCurve() {
        curve.reset()
        curveAwaitingPress = false
        curveConfirmOnRelease = false
        showPrimitivePreview(null)
        onOverlayChanged?.invoke()
        render()
    }
    private fun curveHitRadius(): Float = 32f / view.zoom.coerceAtLeast(0.1f)
    private fun showCurvePreview() {
        showPrimitivePreview(blenderPrimitivePoints(ProjectGreasePrimitive.CURVE, curve.anchors()))
        onOverlayChanged?.invoke()
    }
    private var polylineAwaitingPress = false
    private var polylineLastX = 0f
    private var polylineLastY = 0f

    private var legacyInputSamples = 4
    private var legacyLazyEnabled = false
    private var legacyLazyRadius = 12f
    private var legacyLazyFactor = 0.75f
    private var legacyDisableStabilizer = false
    private var legacyManhattanThreshold = 1
    private var legacyEuclideanThreshold = 1f
    private var legacyActiveSmooth = 0f
    private var legacyJitter = 0f
    private var legacyDrawAngleFactor = 0f
    private var legacyDrawAngle = 0f

    var stabilizerEnabled = false
        private set
    var stabilizerFactor = 0.75f
        private set
    var stabilizerRadius = 12f
        private set
    var spacing = 0f
        private set

    fun setStabilizer(enabled:Boolean, factor:Float=stabilizerFactor, radius:Float=stabilizerRadius) {
        legacyLazyEnabled=enabled
        legacyLazyFactor=factor.coerceIn(0f,1f)
        legacyLazyRadius=radius.coerceAtLeast(0f)
        stabilizerEnabled=enabled
        stabilizerFactor=legacyLazyFactor
        stabilizerRadius=legacyLazyRadius
    }
    fun setSpacing(value:Float) {
        spacing=value.coerceIn(0f,100f)
        legacyEuclideanThreshold = if (value > 0f) value else 1f // 0 restores Blender's default filter
    }

    /** Active smoothing of the Legacy GP brush (0 = off, 1 = strongest). */
    fun setActiveSmooth(value:Float) { legacyActiveSmooth = value.coerceIn(0f, 1f) }

    private var gizmoGestureFirst = false
    private var shapeAwaitingPress = false
    private var shapeConfirmOnRelease = false
    fun beginStroke():Boolean {
        if (rendererHandle == 0L) return false
        if (gizmoActive) { gizmoGestureFirst = true; return true }
        if (shapeEdit.isActive && tools.activeTool in setOf(GreaseTool.LINE, GreaseTool.RECTANGLE, GreaseTool.CIRCLE, GreaseTool.ARC)) {
            shapeAwaitingPress = true; shapeConfirmOnRelease = false; return true
        }
        // The primitive tools create strokes: not on a locked or hidden layer (gpencil_primitive_invoke).
        if (tools.activeTool in setOf(GreaseTool.LINE, GreaseTool.RECTANGLE, GreaseTool.CIRCLE, GreaseTool.ARC,
                GreaseTool.POLYLINE, GreaseTool.CURVE) && !activeLayerEditable()) return false
        return when (tools.activeTool) {
            GreaseTool.BOX_SELECT -> { areaStart = null; boxSelectRect = null; true }
            GreaseTool.CIRCLE_SELECT -> { areaFirstDab = true; true }
            // Draw runs in the native tool session (toolSamples), not through beginStroke.
            GreaseTool.LASSO -> { pendingLassoPoints.clear(); true }
            GreaseTool.POLYLINE -> {
                // The polyline outlives a single gesture: each gesture adds a vertex.
                polylineAwaitingPress = true
                true
            }
            GreaseTool.ANNOTATE -> {
                if (annotationEraser) true
                else GPNative.nativeAnnotationCommand(native.handle, ANNOT_BEGIN, null) != 0
            }
            GreaseTool.CURVE -> {
                // The curve outlives a gesture too: first the line, then handle drags.
                curveAwaitingPress = true
                curveConfirmOnRelease = false
                true
            }
            GreaseTool.LINE, GreaseTool.RECTANGLE, GreaseTool.CIRCLE, GreaseTool.ARC -> {
                pendingShapePoints.clear()
                pendingShapeTool = tools.activeTool
                true
            }
            else -> false
        }
    }

    fun addStrokePoint(x:Float,y:Float,pressure:Float,timeSeconds:Float){
        if (rendererHandle == 0L) return
        if (gizmoActive) {
            if (gizmoGestureFirst) { gizmoGestureFirst = false; gizmoPress(x, y) } else gizmoMove(x, y)
            return
        }
        if (shapeEdit.isActive && (shapeAwaitingPress || shapeEdit.dragging >= 0 || shapeConfirmOnRelease)) {
            if (shapeAwaitingPress) {
                shapeAwaitingPress = false
                if (!shapeEdit.press(x, y, curveHitRadius())) shapeConfirmOnRelease = true
            } else if (!shapeConfirmOnRelease) {
                val snapped = view.snapPoint(x, y)
                shapeEdit.move(snapped.first, snapped.second); showShapePreview()
            }
            return
        }
        if (tools.activeTool == GreaseTool.BOX_SELECT) {
            val start = areaStart ?: (x to y).also { areaStart = it }
            boxSelectRect = floatArrayOf(start.first, start.second, x, y)
            onOverlayChanged?.invoke()
            return
        }
        if (tools.activeTool == GreaseTool.CIRCLE_SELECT) {
            selectCircleAt(x, y, circleSelectRadius(), areaFirstDab)
            areaFirstDab = false
            return
        }
        if (tools.activeTool == GreaseTool.LASSO) {
            val snapped=view.snapPoint(x,y)
            pendingLassoPoints += snapped.first to snapped.second
        } else if (tools.activeTool == GreaseTool.POLYLINE) {
            val snapped=view.snapPoint(x,y)
            polylineLastX = snapped.first
            polylineLastY = snapped.second
            if (polylineAwaitingPress) {
                polyline.press(snapped.first, snapped.second)
                polylineAwaitingPress = false
            } else {
                polyline.move(snapped.first, snapped.second)
            }
            showPrimitivePreview(
                blenderPrimitivePoints(ProjectGreasePrimitive.POLYLINE, polyline.previewVertices())
            )
        } else if (tools.activeTool == GreaseTool.ANNOTATE) {
            if (annotationEraser) {
                if (GPNative.nativeAnnotationCommand(native.handle, ANNOT_ERASE, floatArrayOf(x, y, annotationEraserRadius())) != 0) {
                    document.markDirty()
                }
            } else {
                GPNative.nativeAnnotationCommand(native.handle, ANNOT_ADD_POINT, floatArrayOf(x, y))
            }
        } else if (tools.activeTool == GreaseTool.CURVE) {
            val snapped=view.snapPoint(x,y)
            curveLastX = snapped.first
            curveLastY = snapped.second
            if (curveAwaitingPress) {
                curveAwaitingPress = false
                // A press away from every handle confirms; the rest of that gesture is ignored.
                if (curve.press(snapped.first, snapped.second, curveHitRadius()) == CurveSession.Press.CONFIRM) {
                    curveConfirmOnRelease = true
                    return
                }
            } else if (!curveConfirmOnRelease) {
                curve.move(snapped.first, snapped.second)
            }
            if (!curveConfirmOnRelease) showCurvePreview()
        } else if (pendingShapeTool != null) {
            val snapped=view.snapPoint(x,y)
            pendingShapePoints += PendingPoint(snapped.first, snapped.second, pressure.coerceIn(0f,1f), timeSeconds)
            showPrimitivePreview(generatedShapePoints())
        }
    }

    /**
     * Shape geometry from Blender 3.6.23 gpencil_primitive.c, run natively
     * (project_grease_blender_primitive.c). Anchors are start/end for shapes and
     * the vertices for a polyline. Edges 0 = Blender's operator defaults.
     */
    private fun blenderPrimitivePoints(type:Int, anchors:List<Pair<Float,Float>>):FloatArray? {
        if (anchors.size < 2) return null
        val packed = FloatArray(anchors.size * 2)
        anchors.forEachIndexed { i, p ->
            packed[i * 2] = p.first
            packed[i * 2 + 1] = p.second
        }
        val points = GPNative.nativeGenerateBlenderPrimitive(type, packed, 0, false) ?: return null
        return if (points.size >= 4 && points.size % 2 == 0) points else null
    }

    private fun generatedShapePoints():FloatArray? {
        val tool = pendingShapeTool ?: return null
        val type = ProjectGreasePrimitive.forTool(tool) ?: return null
        val first = pendingShapePoints.firstOrNull() ?: return null
        val last = pendingShapePoints.last()
        // A tap without a drag is not a shape.
        if (first.x == last.x && first.y == last.y) return null
        lastShapeAnchors = (first.x to first.y) to (last.x to last.y)
        return blenderPrimitivePoints(type, listOf(first.x to first.y, last.x to last.y))
    }

    private fun showPrimitivePreview(xy:FloatArray?) {
        if (rendererHandle == 0L) return
        if (xy == null) {
            GPNative.nativeClearPreviewStrokeEglRenderer(rendererHandle)
            return
        }
        val count = xy.size / 2
        val packed = FloatArray(count * 3)
        for (i in 0 until count) {
            packed[i * 3] = xy[i * 2]
            packed[i * 3 + 1] = xy[i * 2 + 1]
            packed[i * 3 + 2] = 1f
        }
        GPNative.nativeSetPreviewStrokeEglRenderer(rendererHandle, packed, materials.thickness)
    }

    /**
     * Commits exactly the previewed Blender geometry, as the primitive operator
     * moves its temporary stroke into the frame. Uses the GP handle, not the
     * EGL renderer handle.
     */
    private fun commitPrimitivePoints(xy:FloatArray, cyclic:Boolean):Boolean {
        if (native.handle == 0L || xy.size < 4) return false
        val created = GPNative.nativeCreatePolyline(
            native.handle, xy, xy.size / 2,
            materials.activeMaterial, materials.thickness, cyclic
        )
        if (created) {
            selection.selectStroke(native.strokeCount() - 1)
            history.markEdit(); document.markDirty(); render()
        }
        return created
    }

    private fun finishPolyline():Boolean {
        val vertices = polyline.vertices
        polyline.reset()
        polylineAwaitingPress = false
        showPrimitivePreview(null)
        val points = blenderPrimitivePoints(ProjectGreasePrimitive.POLYLINE, vertices)
        val ok = points != null && commitPrimitivePoints(points, false)
        if (!ok) render()
        return ok
    }

    fun endStroke(){
        if (rendererHandle == 0L) return
        if (gizmoActive) { gizmoRelease(); return }
        if (shapeEdit.isActive && (shapeConfirmOnRelease || shapeEdit.dragging >= 0 || shapeAwaitingPress)) {
            // A tap (no move) away from every handle confirms, as a click outside does in gpencil_primitive.c.
            if (shapeAwaitingPress && shapeEdit.dragging < 0) shapeConfirmOnRelease = true
            shapeAwaitingPress = false
            if (shapeConfirmOnRelease) { shapeConfirmOnRelease = false; confirmShape() } else { shapeEdit.release(); showShapePreview() }
            return
        }
        if (tools.activeTool == GreaseTool.BOX_SELECT) {
            val r = boxSelectRect
            boxSelectRect = null; areaStart = null
            onOverlayChanged?.invoke()
            if (r != null) selectBox(minOf(r[0], r[2]), minOf(r[1], r[3]), maxOf(r[0], r[2]), maxOf(r[1], r[3])) else render()
            return
        }
        if (tools.activeTool == GreaseTool.CIRCLE_SELECT) { render(); return }
        if (tools.activeTool == GreaseTool.LASSO) {
            val noose = pendingLassoPoints.toList()
            pendingLassoPoints.clear()
            runSelectCommand(ProjectGreaseSelect.lasso(selectOp, ProjectGreaseSelect.areaMode(selectMode), noose))
            return
        }
        if (tools.activeTool == GreaseTool.POLYLINE) {
            when (polyline.release(polylineLastX, polylineLastY)) {
                PolylineSession.Release.FINISH -> finishPolyline()
                PolylineSession.Release.CONTINUE -> showPrimitivePreview(
                    blenderPrimitivePoints(ProjectGreasePrimitive.POLYLINE, polyline.previewVertices())
                )
            }
            return
        }
        if (tools.activeTool == GreaseTool.ANNOTATE) {
            if (!annotationEraser && GPNative.nativeAnnotationCommand(native.handle, ANNOT_END, null) != 0) {
                document.markDirty()
            }
            render()
            return
        }
        if (tools.activeTool == GreaseTool.CURVE) {
            if (curveConfirmOnRelease) {
                confirmCurve()
            } else {
                curve.release(curveLastX, curveLastY)
                showCurvePreview()
            }
            return
        }
        val shapeTool = pendingShapeTool
        val finalPoints = generatedShapePoints()
        showPrimitivePreview(null)
        pendingShapePoints.clear()
        pendingShapeTool = null
        val type = shapeTool?.let { ProjectGreasePrimitive.forTool(it) } ?: return
        if (finalPoints != null) {
            // gpencil_primitive.c keeps the shape editable (handles, subdivisions, extrude) until it
            // is confirmed; the shape shows as the preview meanwhile.
            val anchors = lastShapeAnchors
            if (anchors != null && shapeEdit.start(type, anchors.first, anchors.second)) showShapePreview()
            else commitPrimitivePoints(finalPoints, ProjectGreasePrimitive.isCyclic(type))
        }
    }

    fun cancelStroke(){
        if (rendererHandle != 0L && sessionTool >= 0) {
            toolSamples(sessionTool, FloatArray(0), 0, ToolSession.PHASE_CANCEL)
        }
        pendingShapePoints.clear()
        pendingShapeTool=null
        pendingLassoPoints.clear()
        if (tools.activeTool == GreaseTool.ANNOTATE && !annotationEraser && native.handle != 0L) {
            GPNative.nativeAnnotationCommand(native.handle, ANNOT_CANCEL, null)
        }
        if (tools.activeTool == GreaseTool.CURVE) {
            curveAwaitingPress = false
            curveConfirmOnRelease = false
            curve.cancelGesture()
            showCurvePreview()
        } else if (tools.activeTool == GreaseTool.POLYLINE) {
            polyline.cancelGesture()
            polylineAwaitingPress = false
            showPrimitivePreview(
                blenderPrimitivePoints(ProjectGreasePrimitive.POLYLINE, polyline.previewVertices())
            )
        } else {
            showPrimitivePreview(null)
        }
    }
    fun selectStrokeInLasso(points:List<Pair<Float,Float>>):Boolean =
        runSelectCommand(ProjectGreaseSelect.lasso(selectOp, ProjectGreaseSelect.areaMode(selectMode), points))

    // ---- Blender 3.6.23 Legacy GP selection (native/blender_gp/project_grease_blender_select.c) ----
    // Point vs. stroke select mode and the eSelectOp used by lasso/box/circle selection.
    // Edit-mode select mode switch (Point / Stroke / Segment); Stroke picks whole strokes as before.
    var selectMode = ProjectGreaseSelect.MODE_STROKE
        private set
    var selectOp = ProjectGreaseSelect.OP_SET
        private set
    fun setSelectMode(value:Int):Boolean {
        if (!ProjectGreaseSelect.isValidSelectMode(value)) return false
        selectMode = value
        pickEntireStrokes = value == ProjectGreaseSelect.MODE_STROKE
        return true
    }
    fun setSelectOp(value:Int):Boolean {
        if (!ProjectGreaseSelect.isValidOp(value)) return false
        selectOp = value
        return true
    }

    /** Runs a native selection command; selection changes are undoable like Blender's operators. */
    private fun runSelectCommand(command:ProjectGreaseSelect.Command?):Boolean {
        if (command == null || native.handle == 0L) return false
        val changed = native.applyEditCommand(command.id, command.args)
        if (changed) {
            history.markEdit()
            document.markDirty()
            render()
        }
        return changed
    }
    fun selectAll(action:Int = ProjectGreaseSelect.ACTION_SELECT) = runSelectCommand(ProjectGreaseSelect.all(action))
    fun deselectAll() = selectAll(ProjectGreaseSelect.ACTION_DESELECT)
    fun invertSelection() = selectAll(ProjectGreaseSelect.ACTION_INVERT)
    fun selectLinked() = runSelectCommand(ProjectGreaseSelect.linked())
    fun selectAlternate(unselectEnds:Boolean=false) = runSelectCommand(ProjectGreaseSelect.alternate(unselectEnds))
    fun selectMore() = runSelectCommand(ProjectGreaseSelect.more())
    fun selectLess() = runSelectCommand(ProjectGreaseSelect.less())
    fun selectLastPoints(onlySelectedStrokes:Boolean=false, extend:Boolean=false) =
        runSelectCommand(ProjectGreaseSelect.last(onlySelectedStrokes, extend))
    fun selectBox(x0:Float, y0:Float, x1:Float, y1:Float) =
        runSelectCommand(ProjectGreaseSelect.box(selectOp, ProjectGreaseSelect.areaMode(selectMode), x0, y0, x1, y1))
    /** One dab of a circle-select gesture; only the first dab of a SET gesture replaces the selection. */
    fun selectCircleAt(x:Float, y:Float, radius:Float, isFirst:Boolean) =
        runSelectCommand(ProjectGreaseSelect.circle(selectOp, ProjectGreaseSelect.areaMode(selectMode), x, y, radius, isFirst))

    fun fillSelectedStroke():Boolean {
        val i=selection.selectedStroke
        if(i<0) return false
        val ok=native.fillStroke(i)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }

    /** GP_ONION_FADE: ghosts fade with their keyframe distance. */
    fun setOnionFade(enabled:Boolean):Boolean {
        if (native.handle == 0L) return false
        native.applyEditCommand(ProjectGreaseSelect.CMD_ONION_FADE, ProjectGreaseSelect.onionFade(enabled).args)
        onion.setFade(enabled)
        render()
        return true
    }
    /** Per-layer "Use onion skinning" (GP_LAYER_ONIONSKIN). */
    fun layerOnion(index:Int = selectedLayer):Boolean = (native.layerInfo(index)?.getOrNull(3) ?: 1f) != 0f
    fun setLayerOnion(index:Int, enabled:Boolean):Boolean {
        val command = ProjectGreaseSelect.onionLayer(index, enabled) ?: return false
        val ok = native.applyEditCommand(command.id, command.args)
        if (ok) { history.markEdit(); document.markDirty(); render() }
        return ok
    }
    /** The onion overlay settings live in the editor; re-apply them after the document is replaced. */
    private fun reapplyOnion() {
        native.setOnionSkin(onion.enabled, onion.beforeFrames, onion.afterFrames, onion.opacity)
        native.applyEditCommand(ProjectGreaseSelect.CMD_ONION_FADE, ProjectGreaseSelect.onionFade(onion.fade).args)
        ProjectGreaseSelect.onionStyle(onion.mode, onion.usePrevColor, onion.useNextColor, onion.prevColor, onion.nextColor)
            ?.let { native.applyEditCommand(it.id, it.args) }
        ProjectGreaseSelect.onionFilter(onion.keyTypeFilter, onion.loop)?.let { native.applyEditCommand(it.id, it.args) }
    }
    /** Onion mode (Relative / Absolute / Selected) and the custom ghost colours before / after. */
    fun setOnionStyle(mode:Int = onion.mode, usePrevColor:Boolean = onion.usePrevColor, useNextColor:Boolean = onion.useNextColor,
                      prevColor:Int = onion.prevColor, nextColor:Int = onion.nextColor):Boolean {
        val command = ProjectGreaseSelect.onionStyle(mode, usePrevColor, useNextColor, prevColor, nextColor) ?: return false
        if (native.handle == 0L || !native.applyEditCommand(command.id, command.args)) return false
        onion.setStyle(mode, usePrevColor, useNextColor, prevColor, nextColor)
        render()
        return true
    }
    // ---- Vertex-group operators on the selection (active group) and layer operators (edit7) ----
    fun assignSelectionToGroup(weight:Float = 1f) = runSelectCommand(ProjectGreaseSelect.vgAssign(weightPaintGroup, weight))
    fun removeSelectionFromGroup() = runSelectCommand(ProjectGreaseSelect.vgOp(ProjectGreaseSelect.CMD_VG_REMOVE, weightPaintGroup))
    fun selectGroupPoints() = runSelectCommand(ProjectGreaseSelect.vgOp(ProjectGreaseSelect.CMD_VG_SELECT, weightPaintGroup))
    fun deselectGroupPoints() = runSelectCommand(ProjectGreaseSelect.vgOp(ProjectGreaseSelect.CMD_VG_DESELECT, weightPaintGroup))
    fun invertGroupWeights() = runSelectCommand(ProjectGreaseSelect.vgOp(ProjectGreaseSelect.CMD_VG_INVERT, weightPaintGroup))
    fun normalizeGroupWeights() = runSelectCommand(ProjectGreaseSelect.vgOp(ProjectGreaseSelect.CMD_VG_NORMALIZE, weightPaintGroup))
    /** Merges the active layer into the one below; native makes the lower layer active, so does the editor. */
    fun mergeLayerDown():Boolean {
        if (selectedLayer <= 0) return false
        val ok = runSelectCommand(ProjectGreaseSelect.layerOp(ProjectGreaseSelect.CMD_LAYER_MERGE))
        if (ok) {
            selectedLayer = (selectedLayer - 1).coerceIn(0, (native.layerCount() - 1).coerceAtLeast(0))
            native.selectLayer(selectedLayer)
            animation.refreshFromNative()
            render()
        }
        return ok
    }
    fun isolateLayer() = runSelectCommand(ProjectGreaseSelect.layerOp(ProjectGreaseSelect.CMD_LAYER_ISOLATE))
    fun lockAllLayers() = runSelectCommand(ProjectGreaseSelect.layerOp(ProjectGreaseSelect.CMD_LOCK_ALL))
    fun unlockAllLayers() = runSelectCommand(ProjectGreaseSelect.layerOp(ProjectGreaseSelect.CMD_UNLOCK_ALL))

    /** Outline modifier, baked: each selected open stroke becomes the closed perimeter of its shape. */
    fun outlineSelection(thickness:Int = 2) = runSelectCommand(ProjectGreaseSelect.outline(thickness))

    // ---- Project settings (ProjectSettings.java), saved in the project file under "settings" ----
    var projectSettings = ProjectSettings(); private set
    /** Applies validated settings: canvas size, fps and the timeline end; null or an error message. */
    fun applyProjectSettings(settings:ProjectSettings):String? {
        settings.validate()?.let { return it }
        projectSettings = settings
        document.canvasWidth = settings.width
        document.canvasHeight = settings.height
        animation.setFps(settings.fps)
        animation.setSceneEnd(settings.frameEnd)
        document.markDirty()
        render()
        return null
    }

    // ---- Material textures ----
    private val materialTextures = HashMap<Int, MaterialTexture>()
    private val textureImages = HashMap<Int, Triple<IntArray, Int, Int>>()
    private fun textureKey(slot:Int, fill:Boolean) = slot * 2 + if (fill) 1 else 0
    fun materialTexture(slot:Int = materials.activeMaterial, fill:Boolean):MaterialTexture =
        materialTextures[textureKey(slot, fill)] ?: MaterialTexture()
    /** Texture images whose URI is known but whose pixels are not loaded (after opening a project). */
    fun texturesNeedingImages():List<Triple<Int, Boolean, String>> =
        materialTextures.mapNotNull { (k, t) -> t.uri?.takeIf { textureImages[k] == null }?.let { Triple(k / 2, k % 2 == 1, it) } }
    private class TextureState(val settings: Map<Int, MaterialTexture>, val images: Map<Int, Triple<IntArray, Int, Int>>)
    private fun captureTextures(): Any = TextureState(HashMap(materialTextures), HashMap(textureImages))
    private fun restoreTextures(state: Any?) {
        val t = state as? TextureState ?: return
        if (t.settings == materialTextures && t.images == textureImages) return
        clearTextures()
        materialTextures.putAll(t.settings); textureImages.putAll(t.images)
        if (rendererHandle != 0L) for ((key, img) in textureImages)
            GPNative.nativeSetMaterialTextureEglRenderer(rendererHandle, key / 2, key % 2 == 1, img.first, img.second, img.third)
    }

    /** Sets the texture settings (and, when given, the image as ARGB pixels) of a material slot. */
    fun setMaterialTexture(slot:Int, fill:Boolean, texture:MaterialTexture, argb:IntArray? = null, width:Int = 0, height:Int = 0):Boolean {
        val command = ProjectGreaseSelect.materialTexture(slot, fill, texture.enabled, texture.mix, texture.scaleX, texture.scaleY,
            texture.offsetX, texture.offsetY, texture.angle, texture.pixelSize) ?: return false
        if (native.handle == 0L || !native.applyEditCommand(command.id, command.args)) return false
        val key = textureKey(slot, fill)
        materialTextures[key] = texture
        if (argb != null && width > 0 && height > 0 && argb.size >= width * height) {
            textureImages[key] = Triple(argb, width, height)
            if (rendererHandle != 0L) GPNative.nativeSetMaterialTextureEglRenderer(rendererHandle, slot, fill, argb, width, height)
        }
        history.markEdit()
        document.markDirty()
        render()
        return true
    }
    private fun clearTextures() {
        if (rendererHandle != 0L) for (key in materialTextures.keys + textureImages.keys)
            GPNative.nativeSetMaterialTextureEglRenderer(rendererHandle, key / 2, key % 2 == 1, null, 0, 0)
        materialTextures.clear(); textureImages.clear()
    }
    /** The presenter drops its textures with the surface; send the cached images again. */
    private fun reuploadTextures() {
        if (rendererHandle == 0L) return
        for ((key, img) in textureImages) GPNative.nativeSetMaterialTextureEglRenderer(rendererHandle, key / 2, key % 2 == 1, img.first, img.second, img.third)
    }
    private fun reapplyTextureSettings() {
        for ((key, t) in materialTextures) {
            ProjectGreaseSelect.materialTexture(key / 2, key % 2 == 1, t.enabled, t.mix, t.scaleX, t.scaleY, t.offsetX, t.offsetY, t.angle, t.pixelSize)
                ?.let { native.applyEditCommand(it.id, it.args) }
        }
    }
    private fun texturesJson():org.json.JSONArray = org.json.JSONArray().apply {
        for ((key, t) in materialTextures.toSortedMap()) put(org.json.JSONObject().put("slot", key / 2).put("fill", key % 2 == 1)
            .put("uri", t.uri ?: "").put("enabled", t.enabled).put("mix", t.mix.toDouble()).put("scaleX", t.scaleX.toDouble())
            .put("scaleY", t.scaleY.toDouble()).put("offsetX", t.offsetX.toDouble()).put("offsetY", t.offsetY.toDouble())
            .put("angle", t.angle.toDouble()).put("pixelSize", t.pixelSize.toDouble()))
    }
    private fun loadTexturesJson(array:org.json.JSONArray?) {
        // images already decoded for the same slot and URI are kept (document restored after the
        // surface was recreated); others are dropped and listed by texturesNeedingImages()
        val keep = materialTextures.mapNotNull { (k, t) -> textureImages[k]?.let { img -> Triple(k, t.uri, img) } }
        clearTextures()
        if (array == null) return
        for (i in 0 until array.length()) {
            val o = array.optJSONObject(i) ?: continue
            val slot = o.optInt("slot", -1); if (slot < 0) continue
            fun f(k:String, d:Float) = o.optDouble(k, d.toDouble()).toFloat().takeIf { it.isFinite() } ?: d
            materialTextures[textureKey(slot, o.optBoolean("fill", false))] = MaterialTexture(
                o.optString("uri", "").ifBlank { null }, o.optBoolean("enabled", false), f("mix", 0f), f("scaleX", 1f), f("scaleY", 1f),
                f("offsetX", 0f), f("offsetY", 0f), f("angle", 0f), f("pixelSize", 100f))
        }
        for ((k, uri, img) in keep) if (uri != null && materialTextures[k]?.uri == uri) textureImages[k] = img
        reapplyTextureSettings()
        reuploadTextures()
    }

    // ---- Animation export (GIF / PNG sequence): frames start..end of the project settings ----
    /** Renders every frame of the export range offscreen (holds show the previous keyframe) and hands
     *  each to [sink]; the current frame is restored. False when a frame cannot be rendered. */
    fun renderExportFrames(transparent:Boolean, sink:(FrameSequenceExport.Item, IntArray) -> Unit):Boolean {
        if (rendererHandle == 0L || native.handle == 0L) return false
        val s = projectSettings
        val keys = native.frameNumbers().sorted().toIntArray()
        val plan = FrameSequenceExport.plan(keys, s.frameStart.coerceAtLeast(1), s.frameEnd.coerceAtLeast(s.frameStart.coerceAtLeast(1)), document.projectName.ifBlank { "frame" }.replace(Regex("[^A-Za-z0-9_-]"), "_"), "png")
        val original = animation.currentFrame
        var ok = true
        GPNative.nativeSetExportBackgroundEglRenderer(rendererHandle, s.background)
        for (item in plan) {
            native.selectFrameOrHold(item.frame)
            val px = GPNative.nativeRenderCanvasPixelsEglRenderer(rendererHandle, document.canvasWidth, document.canvasHeight, transparent || s.transparentBackground)
            if (px == null || px.size != document.canvasWidth * document.canvasHeight) { ok = false; break }
            if (!(transparent || s.transparentBackground)) {
                // composite over the project background colour
                val bg = s.background
                for (i in px.indices) px[i] = over(px[i], bg)
            }
            sink(item, px)
        }
        native.selectFrameOrHold(original)
        animation.setFrame(original)
        render()
        return ok
    }
    private fun over(fg:Int, bg:Int):Int {
        val a = (fg ushr 24) and 255
        if (a == 255) return fg
        fun ch(s:Int) = (((fg ushr s) and 255) * a + ((bg ushr s) and 255) * (255 - a)) / 255
        return (0xFF shl 24) or (ch(16) shl 16) or (ch(8) shl 8) or ch(0)
    }
    fun setOnionSkin(enabled:Boolean,before:Int=2,after:Int=2,opacity:Float=0.35f):Boolean {
        val ok = native.setOnionSkin(enabled,before,after,opacity)
        if (ok) { if (enabled != onion.enabled) onion.toggle(); onion.setBefore(before); onion.setAfter(after); onion.setOpacity(opacity); render() }
        return ok
    }
    /** Vector pages (SVG/PDF export) for [frames]; the layer/frame selection is restored afterwards. */
    /** Layer names in document order (export dialog layer filter). */
    fun exportLayerNames():List<String> {
        if (native.handle == 0L) return emptyList()
        val adapter = NativeDocumentAdapter(native)
        return (0 until adapter.layerCount()).map { adapter.layerRecord(it)?.name?.ifBlank { "Layer ${it + 1}" } ?: "Layer ${it + 1}" }
    }
    fun exportPages(frames:List<Int>, includeAnnotations:Boolean = false, options:VectorExportOptions = VectorExportOptions()):List<VectorPage> {
        if (native.handle == 0L || frames.isEmpty()) return emptyList()
        val originalLayer = selectedLayer
        val originalFrame = animation.currentFrame
        var pages = VectorExport.pages(NativeDocumentAdapter(native), document.canvasWidth, document.canvasHeight, frames, options)
        if (includeAnnotations) {
            val dump = annotationDump()
            val style = annotationStyle()
            pages = pages.map { page ->
                val notes = AnnotationData.exportLayer(dump, style, page.frame)
                if (notes == null) page else VectorPage(page.frame, page.width, page.height, page.layers + notes, page.clip)
            }
        }
        if (native.layerCount() > 0) {
            native.selectLayer(originalLayer.coerceIn(0, native.layerCount() - 1))
            native.selectFrameOrHold(originalFrame)
        }
        render()
        return pages
    }

    fun saveDocumentJson():String? {
        if (native.handle == 0L) return null
        val originalLayer = selectedLayer
        val originalFrame = animation.currentFrame
        val json = ProjectDocumentCodec.encode(
            NativeDocumentAdapter(native),
            document.canvasWidth, document.canvasHeight, animation.fps, originalFrame, animation.sceneEnd,
            animation.timeline
        )
        if (native.layerCount() > 0) {
            native.selectLayer(originalLayer.coerceIn(0, native.layerCount() - 1))
            native.selectFrameOrHold(originalFrame)
        }
        render()
        // Project settings, material textures and annotations have their own keys (older files have none).
        return runCatching {
            val obj = org.json.JSONObject(json)
            obj.put("settings", org.json.JSONObject(projectSettings.toJson()))
            if (materialTextures.isNotEmpty()) obj.put("textures", texturesJson())
            AnnotationData.toJson(annotationDump(), annotationStyle())?.let { obj.put("annotations", it) }
            obj.toString()
        }.getOrDefault(json)
    }

    fun loadDocumentJson(raw:String):Boolean {
        if (native.handle == 0L) return false
        val parsed = ProjectDocumentCodec.parse(raw) ?: return false
        if (!GPNative.nativeResetDocumentEgl(rendererHandle)) return false
        reapplyOnion()
        document.canvasWidth = (parsed.width ?: document.canvasWidth).coerceAtLeast(1)
        document.canvasHeight = (parsed.height ?: document.canvasHeight).coerceAtLeast(1)
        animation.setFps((parsed.fps ?: animation.fps).coerceIn(1,120))
        animation.setSceneEnd(parsed.frameEnd)
        animation.restoreTimeline(parsed.timeline)
        if (!ProjectDocumentCodec.restore(parsed, NativeDocumentAdapter(native), brushes.size)) return false
        val rawJson = runCatching { org.json.JSONObject(raw) }.getOrNull()
        // "settings": older files use defaults with the file's canvas size, fps and end frame
        projectSettings = rawJson?.optJSONObject("settings")?.let { ProjectSettings.fromJson(it.toString()) }
            ?: ProjectSettings().apply {
                width = document.canvasWidth; height = document.canvasHeight; fps = animation.fps
                if (parsed.frameEnd > 0) frameEnd = parsed.frameEnd
            }
        if (projectSettings.validate() == null) {
            document.canvasWidth = projectSettings.width; document.canvasHeight = projectSettings.height
            animation.setFps(projectSettings.fps); animation.setSceneEnd(projectSettings.frameEnd)
        }
        loadTexturesJson(rawJson?.optJSONArray("textures"))
        val notes = AnnotationData.fromJson(runCatching { org.json.JSONObject(raw).optJSONObject("annotations") }.getOrNull())
        if (notes != null) {
            notes.style?.let { GPNative.nativeAnnotationCommand(native.handle, ANNOT_SET_STYLE, it) }
            GPNative.nativeAnnotationLoad(native.handle, notes.dump)
        }
        if (parsed.layers == null) return true
        syncActiveMaterial(parsed.materials.firstOrNull())
        val targetFrame = parsed.frame.coerceAtLeast(1)
        native.selectFrameOrHold(targetFrame)
        animation.setFrame(targetFrame)
        history.reset()
        document.markSaved()
        render()
        return true
    }

    /** Brings the palette UI state in line with the loaded material 0 (the editor's active material). */
    private fun syncActiveMaterial(material:MaterialRecord?) {
        materials.select(0)
        if (material == null) return
        syncMaterialUi(material)
    }
    private fun syncMaterialUi(material:MaterialRecord) {
        val s = material.stroke
        fun channel(v:Float) = (v.coerceIn(0f,1f) * 255f + 0.5f).toInt()
        materials.setColor((channel(s[3]) shl 24) or (channel(s[0]) shl 16) or (channel(s[1]) shl 8) or channel(s[2]))
        materials.setFillEnabled(material.fillEnabled)
        pushMaterialColor()
    }

    /** Layer state as the native document holds it; null when there is no such layer. */
    fun layerState(index:Int = selectedLayer):LayerRecord? = NativeDocumentAdapter(native).layerRecord(index)

    // ---- Layer masks (the layer is drawn only where its mask layers have coverage) ----
    fun layerName(index:Int):String = native.layerName(index) ?: ("Layer "+(index+1))
    fun layerMasks(layer:Int = selectedLayer):List<MaskRecord> = NativeDocumentAdapter(native).layerMasks(layer)
    fun layerUsesMask(layer:Int = selectedLayer):Boolean = native.layerUseMask(layer)
    private fun maskChanged(ok:Boolean):Boolean {
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }
    fun setLayerUsesMask(enabled:Boolean, layer:Int = selectedLayer) = maskChanged(native.setLayerUseMask(layer, enabled))
    fun addLayerMask(maskLayer:Int, layer:Int = selectedLayer):Boolean {
        val ok = native.maskAdd(layer, maskLayer)
        // Adding a first mask turns the mask on, as picking a mask layer in Blender's UI does.
        if (ok && !native.layerUseMask(layer)) native.setLayerUseMask(layer, true)
        return maskChanged(ok)
    }
    fun removeLayerMask(index:Int, layer:Int = selectedLayer) = maskChanged(native.maskRemove(layer, index))
    fun setLayerMaskFlags(index:Int, hidden:Boolean, inverted:Boolean, layer:Int = selectedLayer) =
        maskChanged(native.maskSetFlags(layer, index, (if (hidden) 1 else 0) or (if (inverted) 2 else 0)))

    fun setMultiframeEditing(enabled:Boolean):Boolean {
        val ok=native.setMultiframeEditing(enabled)
        if(ok){ multiframeEditing=enabled; render() }
        return ok
    }
    // Fill tool options (Blender fill brush): leak size (0 = Blender's ceil(3 x precision)),
    // dilate (negative contracts), boundary source.
    var fillLeak = 0; private set
    var fillDilate = 1; private set
    var fillBoundary = FILL_BOUNDARY_ALL; private set
    /** Fill "Extend Lines" (brush fill_extend_fac, Blender default 0). */
    var fillExtend = 0f; private set
    fun setFillExtend(value:Float) { fillExtend = if (value.isFinite()) value.coerceIn(0f, 10f) else 0f }
    /** Fill "Precision" (brush fill_factor, Blender default 1, range 0.05..8). */
    var fillPrecision = 1f; private set
    /** Extend Lines "Collide" (GP_BRUSH_FILL_STROKE_COLLIDE): only extensions that hit a stroke close gaps. */
    var fillCollide = false; private set
    fun setFillPrecision(value:Float = fillPrecision, collide:Boolean = fillCollide) {
        fillPrecision = if (value.isFinite()) value.coerceIn(0.05f, 8f) else 1f
        fillCollide = collide
    }
    fun setFillOptions(leak:Int = fillLeak, dilate:Int = fillDilate, boundary:Int = fillBoundary) {
        fillLeak = leak.coerceIn(0, 100)
        fillDilate = dilate.coerceIn(-40, 40)
        fillBoundary = boundary.coerceIn(FILL_BOUNDARY_ALL, FILL_BOUNDARY_EDIT_LINES)
    }
    fun fillAt(x: Float, y: Float): Boolean {
        if (rendererHandle == 0L || !activeLayerEditable()) return false
        // Blender Legacy GP Fill creates a closed filled stroke using the active material.
        // Enable the material's Fill component only when the Fill tool is actually used.
        val enabledFill = !materials.fillEnabled
        if (enabledFill) {
            // Part of the fill's own undo step, recorded below.
            materials.setFillEnabled(true)
            native.setMaterialFillEnabled(materials.activeMaterial, true)
        }
        GPNative.nativeSetFillOptionsEglRenderer(rendererHandle, fillLeak, fillDilate, fillBoundary)
        GPNative.nativeSetFillExtendEglRenderer(rendererHandle, fillExtend)
        GPNative.nativeSetFillPrecisionEglRenderer(rendererHandle, fillPrecision, fillCollide)
        val ok = GPNative.nativeFillAtEglRenderer(rendererHandle, x.toInt(), y.toInt(), materials.activeMaterial, materials.thickness)
        if (ok) { history.markEdit(); document.markDirty() }
        else if (enabledFill) {
            // No fill was made (it leaked, or the tap was outside a boundary): the material is left
            // as it was, since there is no undo step that could restore it.
            materials.setFillEnabled(false)
            native.setMaterialFillEnabled(materials.activeMaterial, false)
        }
        return ok
    }

    fun applySelectedModifier(name:String,factor:Float=0.5f,iterations:Int=2):Boolean {
        val index=selection.selectedStroke
        if(index<0)return false
        val ok=native.applyModifier(index,name,factor,iterations)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }

    fun smoothSelectedStroke(influence:Float=0.5f,iterations:Int=2):Boolean {
        val i=selection.selectedStroke
        if(i<0) return false
        val ok=native.smoothStroke(i,influence,iterations)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }
    fun pickColorAt(x:Int,y:Int):Boolean {
        if (rendererHandle == 0L) return false
        val argb = GPNative.nativePickColorEglRenderer(rendererHandle, x, y)
        if ((argb ushr 24) == 0) return false
        return setMaterialColor(argb)
    }

    fun setMaterialColor(argb:Int):Boolean {
        materials.setColor(argb)
        if (rendererHandle == 0L) return false
        val c=colorToFloats(argb)
        val alpha=c[3]*materials.opacity
        val stroke=floatArrayOf(c[0],c[1],c[2],alpha)
        val fill=floatArrayOf(c[0],c[1],c[2],alpha)
        val before=native.materialInfo(materials.activeMaterial)
        val ok=native.setMaterialColors(materials.activeMaterial,stroke,fill)
        if(ok) {
            pushMaterialColor()
            // A material colour change is its own undo step (Blender pushes one per property edit);
            // unrecorded, the next operation's undo would also revert it.
            if (before == null || !before.copyOfRange(0, 8).contentEquals(stroke + fill)) { history.markEdit(); document.markDirty() }
        }
        return ok
    }
    fun selectMaterial(index:Int):Boolean {
        if(index<0 || rendererHandle==0L) return false
        while(native.materialCount() <= index) {
            if(!native.createMaterial()) return false
        }
        materials.select(index)
        return true
    }
    /**
     * Delete material slot `index` (edit5 pg_gp_material_slot_remove): its strokes are deleted, higher
     * slots move down; the last slot stays. The active material follows its slot. Undoable.
     */
    fun deleteMaterial(index:Int = materials.activeMaterial):Boolean {
        val count = native.materialCount()
        if (index < 0 || index >= count || count <= 1) return false
        val command = ProjectGreaseSelect.materialRemove(index) ?: return false
        if (!native.applyEditCommand(command.id, command.args)) return false
        val active = materials.activeMaterial
        val next = when {
            active > index -> active - 1
            active == index -> index.coerceAtMost(count - 2)
            else -> active
        }
        materials.select(next)
        NativeDocumentAdapter(native).materialRecord(next)?.let { syncMaterialUi(it) }
        selection.clear()
        history.markEdit(); document.markDirty(); render()
        return true
    }
    fun materialCount():Int = native.materialCount()

    /** Fit canvas: whole canvas in view (see ViewController.fitCanvas). */
    fun fitCanvas() { view.fitCanvas(); render() }

    /**
     * The current frame as the screen shows it (modifiers, masks, effects; no annotations), rendered
     * offscreen at canvas size: ARGB pixels, top row first, or null when the renderer cannot.
     */
    fun renderCanvasPixels(transparent:Boolean):IntArray? {
        if (rendererHandle == 0L) return null
        val px = GPNative.nativeRenderCanvasPixelsEglRenderer(rendererHandle, document.canvasWidth, document.canvasHeight, transparent)
        render()
        return px?.takeIf { it.size == document.canvasWidth * document.canvasHeight }
    }

    /**
     * The current frame as the PNG export writes it: like every animation export, transparent when
     * asked or when the project settings say so, otherwise composited over the project background.
     */
    fun exportCanvasPixels(transparent:Boolean):IntArray? {
        val s = projectSettings
        if (rendererHandle == 0L) return null
        GPNative.nativeSetExportBackgroundEglRenderer(rendererHandle, s.background)
        return renderCanvasPixels(transparent || s.transparentBackground)
    }

    fun setMaterialFillEnabled(enabled:Boolean):Boolean {
        val was = native.materialInfo(materials.activeMaterial)?.getOrNull(9)?.let { it != 0f }
        materials.setFillEnabled(enabled)
        val ok = native.setMaterialFillEnabled(materials.activeMaterial,enabled)
        if (ok && was != enabled) { history.markEdit(); document.markDirty(); render() }
        return ok
    }
    fun pushMaterialColor(){
        if(rendererHandle==0L)return
        val c=colorToFloats(materials.colorArgb)
        // Material colour and opacity only. Strength is the per-point strength the draw session writes
        // (Blender's brush draw_strength); the presenter applies it once, for the open stroke as for
        // committed ones. Multiplying it in here as well made Strength apply twice.
        GPNative.nativeSetStrokeColorEglRenderer(
            rendererHandle,
            c[0], c[1], c[2], c[3] * materials.opacity
        )
    }
    private fun colorToFloats(argb:Int):FloatArray = floatArrayOf(
        ((argb ushr 16) and 255)/255f,
        ((argb ushr 8) and 255)/255f,
        (argb and 255)/255f,
        ((argb ushr 24) and 255)/255f
    )
    fun render(){
        if(rendererHandle!=0L){
            GPNative.nativeSetCanvasSize(rendererHandle,document.canvasWidth,document.canvasHeight)
            GPNative.nativeSetViewTransform(rendererHandle,view.zoom,view.panX,view.panY)
            // Weight Paint mode shows the active group's weights (blue 0 .. red 1) instead of the colors.
            GPNative.nativeSetWeightView(rendererHandle, if (mode == GreaseMode.WEIGHT_PAINT) weightPaintGroup else -1)
            // Blender's edit overlay (points, selected ones highlighted) whenever selecting is possible:
            // Edit mode, or a select/lasso/transform tool picked from the rail in another mode.
            GPNative.nativeSetSelectionOverlay(rendererHandle, selectionOverlayVisible())
            GPNative.nativeSetGuide(rendererHandle, if (tools.activeTool == GreaseTool.DRAW) view.guideType else -1,
                view.guideCenterX, view.guideCenterY, view.guideAngle, view.guideSpacing)
            GPNative.nativeRenderEgl(rendererHandle)
        }
        if (transformSettings.proportional || transformSettings.pivot == ProjectGreaseSelect.PIVOT_CURSOR) overlayTick++
    }
    /** Bumped on renders while the proportional circle or 2D cursor overlay is visible. */
    var overlayTick by androidx.compose.runtime.mutableIntStateOf(0)
    fun selectionOverlayVisible():Boolean = mode == GreaseMode.EDIT || tools.activeTool in SELECTION_TOOLS
    fun layerCount() = native.layerCount()

    // Live modifier stack of the selected layer (see ModifierStack.kt). Every change is an undo step
    // and redraws; parameter drags call setModifierParam(commit = false) and commitModifierEdit() once.
    fun modifiers(layer:Int=selectedLayer):List<ModifierRecord> = ModifierStackCommands.list(native,layer)
    private fun modifierChanged(ok:Boolean):Boolean {
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }
    fun addModifier(type:Int,layer:Int=selectedLayer):Boolean = modifierChanged(ModifierStackCommands.add(native,layer,type)>=0)
    fun removeModifier(index:Int,layer:Int=selectedLayer):Boolean = modifierChanged(ModifierStackCommands.remove(native,layer,index))
    fun moveModifier(index:Int,delta:Int,layer:Int=selectedLayer):Boolean = modifierChanged(ModifierStackCommands.moveBy(native,layer,index,delta))
    fun setModifierEnabled(index:Int,enabled:Boolean,layer:Int=selectedLayer):Boolean = modifierChanged(ModifierStackCommands.setEnabled(native,layer,index,enabled))
    fun setModifierParam(index:Int,paramIndex:Int,value:Float,commit:Boolean=true,layer:Int=selectedLayer):Boolean {
        val ok=ModifierStackCommands.setParam(native,layer,index,paramIndex,value)
        if(ok){ if(commit){history.markEdit()}; document.markDirty(); render() }
        return ok
    }
    fun commitModifierEdit():Boolean = history.markEdit()

    // Shader effects of a layer (see ShaderFx.kt): a 2D post-pass over the rendered layer, same
    // undo/redraw behaviour as the modifier stack. Strokes are never changed.
    fun effects(layer:Int=selectedLayer):List<FxRecord> = FxCommands.list(native,layer)
    fun addEffect(type:Int,layer:Int=selectedLayer):Boolean = modifierChanged(FxCommands.add(native,layer,type)>=0)
    fun removeEffect(index:Int,layer:Int=selectedLayer):Boolean = modifierChanged(FxCommands.remove(native,layer,index))
    fun moveEffect(index:Int,delta:Int,layer:Int=selectedLayer):Boolean = modifierChanged(FxCommands.moveBy(native,layer,index,delta))
    fun setEffectEnabled(index:Int,enabled:Boolean,layer:Int=selectedLayer):Boolean = modifierChanged(FxCommands.setEnabled(native,layer,index,enabled))
    /** Whole layer, strokes only or fills only ([FxTarget]). */
    fun setEffectTarget(index:Int,target:Int,layer:Int=selectedLayer):Boolean = modifierChanged(FxCommands.setTarget(native,layer,index,target))
    fun setEffectParam(index:Int,paramIndex:Int,value:Float,commit:Boolean=true,layer:Int=selectedLayer):Boolean {
        val ok=FxCommands.setParam(native,layer,index,paramIndex,value)
        if(ok){ if(commit){history.markEdit()}; document.markDirty(); render() }
        return ok
    }
    fun commitEffectEdit():Boolean = history.markEdit()
    /** Bakes the modifier into the layer's strokes (all frames) and removes it from the stack. */
    fun applyLayerModifier(index:Int,layer:Int=selectedLayer):Boolean = modifierChanged(ModifierStackCommands.apply(native,layer,index))

    fun setLayerVisibility(index:Int, visible:Boolean):Boolean {
        val ok=native.setLayerVisibility(index,visible)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }
    fun setLayerLocked(index:Int, locked:Boolean):Boolean {
        val ok=native.setLayerLocked(index,locked)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }
    fun moveLayer(from:Int,to:Int):Boolean {
        val ok=native.moveLayer(from,to)
        if(ok){selectedLayer=to;history.markEdit();document.markDirty();render()}
        return ok
    }
    fun duplicateLayer(index:Int=selectedLayer):Boolean {
        val ok=native.duplicateLayer(index)
        if(ok){selectedLayer=(index+1).coerceAtMost(native.layerCount()-1);history.markEdit();document.markDirty();render()}
        return ok
    }
    fun deleteLayer(index:Int=selectedLayer):Boolean {
        if(native.layerCount()<=1)return false
        val ok=native.deleteLayer(index)
        if(ok){selectedLayer=(index-1).coerceAtLeast(0).coerceAtMost(native.layerCount()-1);history.markEdit();document.markDirty();render()}
        return ok
    }
    fun renameLayer(index:Int=selectedLayer,name:String):Boolean {
        val ok=native.renameLayer(index,name)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }
    /**
     * Fills a freshly reset document from a template (GreaseTemplates): layers bottom to top, one
     * material slot per template material (stroke color, fill enabled when it has a fill color),
     * fps and the scene end frame. The top layer and material 0 end up active.
     */
    fun applyTemplate(template:GreaseTemplates.Template):Boolean {
        setupTrace("applyTemplate ${template.id} handle=${native.handle != 0L} layers=${native.layerCount()}")
        if (native.handle == 0L || template.layers.isEmpty()) return false
        if (native.layerCount() == 0) {
            if (!native.createLayer(template.layers[0])) { setupTrace("createLayer0 failed"); return false }
        } else {
            native.renameLayer(0, template.layers[0])
        }
        native.selectLayer(0)
        if (native.frameCount() == 0) native.createFrame(1)
        for (name in template.layers.drop(1)) {
            if (!native.createLayer(name)) { setupTrace("createLayer $name failed"); return false }
            native.createFrame(1)
        }
        template.materials.forEachIndexed { i, m ->
            while (native.materialCount() <= i) if (!native.createMaterial()) { setupTrace("createMaterial failed"); return false }
            val stroke = colorToFloats(m.stroke)
            val fill = m.fill?.let { colorToFloats(it) } ?: stroke
            native.setMaterialColors(i, stroke, fill)
            native.setMaterialFillEnabled(i, m.fill != null)
        }
        selectedLayer = native.layerCount() - 1
        native.selectLayer(selectedLayer)
        animation.setFps(template.fps)
        animation.initialize()
        animation.setSceneEnd(template.endFrame)
        animation.restoreTimeline(TimelineState())
        materials.select(0)
        template.materials.firstOrNull()?.let { materials.setColor(it.stroke) }
        pushMaterialColor()
        history.reset()
        document.markDirty()
        render()
        return true
    }

    // ---- Annotations (native/blender_gp/project_grease_annotations.h) ------------------------------
    // Separate overlay data like Blender's annotation tool: drawn over the drawing with a fixed
    // screen-space thickness, saved in the project file, left out of export unless asked. Edits
    // are saved with the project but are not undo steps (they are not part of the drawing).
    private companion object {
        const val ANNOT_BEGIN = 0; const val ANNOT_ADD_POINT = 1; const val ANNOT_END = 2; const val ANNOT_CANCEL = 3
        const val ANNOT_ERASE = 4; const val ANNOT_CLEAR = 5; const val ANNOT_SET_STYLE = 6; const val ANNOT_SET_VISIBLE = 7
        const val ANNOT_COUNT = 8
    }
    /** Annotate tool mode: false draws notes, true erases notes only. */
    var annotationEraser = false
        private set
    fun setAnnotationEraser(value:Boolean) { annotationEraser = value }
    private fun annotationEraserRadius():Float = 16f / view.zoom.coerceAtLeast(0.1f)
    /** r, g, b, a, thickness (px), visible. */
    fun annotationStyle():FloatArray = (if (native.handle != 0L) GPNative.nativeAnnotationStyle(native.handle) else null)
        ?: floatArrayOf(0f, 0.6f, 1f, 1f, 3f, 1f)
    val annotationColorArgb:Int get() {
        val s = annotationStyle()
        fun c(v:Float) = (v.coerceIn(0f, 1f) * 255f + 0.5f).toInt()
        return (c(s[3]) shl 24) or (c(s[0]) shl 16) or (c(s[1]) shl 8) or c(s[2])
    }
    private fun setAnnotationStyle(rgba:FloatArray, thickness:Float):Boolean {
        if (native.handle == 0L) return false
        val ok = GPNative.nativeAnnotationCommand(native.handle, ANNOT_SET_STYLE, floatArrayOf(rgba[0], rgba[1], rgba[2], rgba[3], thickness)) != 0
        if (ok) { document.markDirty(); render() }
        return ok
    }
    fun setAnnotationColor(argb:Int):Boolean = setAnnotationStyle(colorToFloats(argb or (0xFF shl 24)), annotationStyle()[4])
    fun setAnnotationThickness(px:Float):Boolean { val s = annotationStyle(); return setAnnotationStyle(s, px) }
    fun annotationCount():Int = if (native.handle != 0L) GPNative.nativeAnnotationCommand(native.handle, ANNOT_COUNT, null) else 0
    fun clearAnnotations():Boolean {
        if (native.handle == 0L) return false
        val ok = GPNative.nativeAnnotationCommand(native.handle, ANNOT_CLEAR, null) != 0
        if (ok) { document.markDirty(); render() }
        return ok
    }
    val annotationsVisible:Boolean get() = annotationStyle()[5] != 0f
    fun setAnnotationsVisible(visible:Boolean):Boolean {
        if (native.handle == 0L) return false
        val ok = GPNative.nativeAnnotationCommand(native.handle, ANNOT_SET_VISIBLE, floatArrayOf(if (visible) 1f else 0f)) != 0
        if (ok) render()
        return ok
    }
    private fun annotationDump():FloatArray? = if (native.handle != 0L) GPNative.nativeAnnotationDump(native.handle) else null

    /**
     * Adds planned strokes (StrokeImport) to the active frame as one undo step: first the new
     * material slots, then each stroke through the same polyline path as the primitives. With
     * [newLayer] the strokes go to a new layer of that name, otherwise to the active layer.
     * Returns the number of strokes created.
     */
    fun importStrokes(plan:StrokeImport.Plan, newLayer:String? = null):Int {
        if (native.handle == 0L || plan.strokes.isEmpty()) return 0
        if (newLayer != null) {
            if (!native.createLayer(newLayer)) return 0
            selectedLayer = (native.layerCount() - 1).coerceAtLeast(0)
            native.createFrame(animation.currentFrame)
            native.selectFrameOrHold(animation.currentFrame)
        } else if (native.frameCount() == 0) {
            native.createFrame(animation.currentFrame)
        }
        for (m in plan.newMaterials) {
            val index = native.materialCount()
            if (!native.createMaterial()) break
            native.setMaterialColors(index, m.stroke, m.fill)
            native.setMaterialFillEnabled(index, m.fillEnabled)
        }
        var created = 0
        for (s in plan.strokes) {
            if (s.material >= native.materialCount()) continue
            if (GPNative.nativeCreatePolyline(native.handle, s.xy, s.xy.size / 2, s.material, s.thickness, s.cyclic)) created++
        }
        if (created > 0 || newLayer != null) {
            animation.initialize()
            animation.setFrame(animation.currentFrame)
            history.markEdit(); document.markDirty(); render()
        }
        return created
    }

    private fun existingMaterials():List<MaterialRecord> {
        val adapter = NativeDocumentAdapter(native)
        return (0 until native.materialCount()).map { adapter.materialRecord(it) ?: MaterialRecord(FloatArray(4), FloatArray(4), true, false) }
    }

    /**
     * Line Art from the 3D reference (ReferenceScene): Blender's Line Art strokes, mapped to the canvas,
     * become strokes on a new "Line Art" layer at the current frame, in the active color.
     * Returns the stroke count.
     */
    fun generateLineArt(thickness:Float = 3f, includeHidden:Boolean = false):Int {
        val strokes = reference.lineArtStrokes(document.canvasWidth, document.canvasHeight, if (includeHidden) 1 else 0)
        if (strokes.isEmpty()) return 0
        val color = materials.colorArgb or (0xFF shl 24)
        val sources = strokes.map { xy -> StrokeImport.Source((0 until xy.size / 2).map { floatArrayOf(xy[it * 2], xy[it * 2 + 1]) }, false, color, null, thickness) }
        val fit = StrokeImport.fit(0f, 0f, document.canvasWidth.toFloat(), document.canvasHeight.toFloat(), document.canvasWidth, document.canvasHeight)
        return importStrokes(StrokeImport.plan(sources, fit, existingMaterials()), newLayer = "Line Art")
    }

    /**
     * Bake Line Art to frames: for every frame of [fromFrame, toFrame] the reference camera orbits
     * linearly from yawFrom to yawTo (ReferenceCamera.orbitAt) and that frame's Line Art strokes are
     * written to the same keyframe of a new "Line Art bake" layer. The reference camera is restored.
     * Returns the number of frames that received strokes.
     */
    fun bakeLineArt(fromFrame:Int, toFrame:Int, yawFrom:Float, yawTo:Float, thickness:Float = 3f, includeHidden:Boolean = false):Int {
        if (native.handle == 0L || toFrame < fromFrame || reference.isEmpty) return 0
        val w = document.canvasWidth
        val h = document.canvasHeight
        val saved = reference.camera
        val color = materials.colorArgb or (0xFF shl 24)
        val fit = StrokeImport.fit(0f, 0f, w.toFloat(), h.toFloat(), w, h)
        if (!native.createLayer("Line Art bake")) return 0
        selectedLayer = (native.layerCount() - 1).coerceAtLeast(0)
        var baked = 0
        try {
            for (f in fromFrame..toFrame) {
                val t = if (toFrame == fromFrame) 0f else (f - fromFrame).toFloat() / (toFrame - fromFrame)
                reference.setCamera(saved.orbitAt(yawFrom, yawTo, t), w, h)
                val strokes = reference.lineArtStrokes(w, h, if (includeHidden) 1 else 0)
                native.createFrame(f)
                native.selectFrameOrHold(f)
                if (strokes.isEmpty()) continue
                val sources = strokes.map { xy -> StrokeImport.Source((0 until xy.size / 2).map { floatArrayOf(xy[it * 2], xy[it * 2 + 1]) }, false, color, null, thickness) }
                val plan = StrokeImport.plan(sources, fit, existingMaterials())
                for (m in plan.newMaterials) {
                    val index = native.materialCount()
                    if (!native.createMaterial()) break
                    native.setMaterialColors(index, m.stroke, m.fill)
                    native.setMaterialFillEnabled(index, m.fillEnabled)
                }
                var created = 0
                for (s in plan.strokes) {
                    if (s.material >= native.materialCount()) continue
                    if (GPNative.nativeCreatePolyline(native.handle, s.xy, s.xy.size / 2, s.material, s.thickness, s.cyclic)) created++
                }
                if (created > 0) baked++
            }
        } finally {
            reference.setCamera(saved, w, h)
            animation.initialize()
            animation.setFrame(animation.currentFrame)
            history.markEdit(); document.markDirty(); render()
        }
        return baked
    }

    /** Import SVG: every shape becomes a stroke on the active layer/frame; the viewBox is fitted into the canvas. */
    fun importSvg(svg:String):Int {
        val sources = StrokeImport.fromSvg(SvgImport.parse(svg))
        val box = StrokeImport.svgViewBox(svg) ?: StrokeImport.bounds(sources) ?: return 0
        val fit = StrokeImport.fit(box[0], box[1], box[2], box[3], document.canvasWidth, document.canvasHeight)
        return importStrokes(StrokeImport.plan(sources, fit, existingMaterials()))
    }

    /**
     * Trace image: outlines of the thresholded image become closed, filled strokes on a new
     * "Trace" layer, scaled to the canvas, in the active color.
     */
    fun traceImage(argb:IntArray, width:Int, height:Int, threshold:Float, traceBright:Boolean, tolerance:Float):Int {
        if (width <= 0 || height <= 0 || argb.size < width * height) return 0
        val outlines = ImageTrace.trace(ImageTrace.mask(argb, width, height, threshold, traceBright), width, height, tolerance, 3)
        val color = materials.colorArgb or (0xFF shl 24)
        val sources = outlines.map { StrokeImport.Source(it, true, color, color, 1f) }
        val fit = StrokeImport.fit(0f, 0f, width.toFloat(), height.toFloat(), document.canvasWidth, document.canvasHeight)
        return importStrokes(StrokeImport.plan(sources, fit, existingMaterials()), newLayer = "Trace")
    }

    fun createLayer(name:String):Boolean {
        val ok = native.createLayer(name)
        if (ok) {
            selectedLayer = (native.layerCount() - 1).coerceAtLeast(0)
            val frameReady = native.createFrame(animation.currentFrame)
            animation.initialize()
            if (!frameReady && native.frameCount() == 0) {
                return false
            }
            history.markEdit()
            document.markDirty()
            render()
        }
        return ok
    }
    fun selectLayer(index:Int):Boolean {
        val ok = native.selectLayer(index)
        if (ok) {
            selectedLayer = index
            animation.initialize()
            render()
        }
        return ok
    }
    fun createFrame(frame:Int):Boolean {
        val ok = animation.ensureFrame(frame)
        if (ok) {
            history.markEdit()
            document.markDirty()
            render()
        }
        return ok
    }
    /** Timeline frame edits as one undo step each (Blender's ACTION_OT_duplicate / delete, interpolate). */
    private fun frameEdit(ok:Boolean):Boolean {
        if (ok) { history.markEdit(); document.markDirty(); render() }
        return ok
    }
    fun duplicateFrame(sourceFrame:Int, targetFrame:Int):Boolean = frameEdit(animation.duplicateFrame(sourceFrame, targetFrame))
    fun deleteFrame(frameNumber:Int):Boolean = frameEdit(animation.deleteFrame(frameNumber))
    fun interpolateFrameAt(frame:Int):Boolean = frameEdit(animation.interpolateAt(frame))
    fun interpolateSequence(frame:Int = animation.currentFrame):Int = animation.interpolateSequence(frame).also { frameEdit(it > 0) }

    fun selectFrame(frame:Int):Boolean {
        val ok = animation.ensureFrame(frame)
        if (ok) render()
        return ok
    }
    fun frameNumbers(): IntArray = native.frameNumbers()
    fun strokeCount() = native.strokeCount()
    fun pointCount() = native.pointCount()
    fun selectedPointCount() = native.selectedPointCount()
    /** True once the EGL surface (and with it the native document) is up. */
    val rendererReady:Boolean get() = rendererHandle != 0L && native.handle != 0L
    fun selectStroke(index:Int)=selection.selectStroke(index)
    fun joinSelectedStrokes():Boolean {
        val ok=native.applyEditCommand(7)
        if(ok){history.markEdit();document.markDirty();selection.clear();render()}
        return ok
    }
    fun selectFirstPoints(onlySelectedStrokes:Boolean=false, extend:Boolean=false):Boolean =
        runSelectCommand(ProjectGreaseSelect.first(onlySelectedStrokes, extend))
    fun selectGroupedByLayer():Boolean =
        runSelectCommand(ProjectGreaseSelect.grouped(ProjectGreaseSelect.GROUP_LAYER))
    fun selectGroupedByMaterial():Boolean =
        runSelectCommand(ProjectGreaseSelect.grouped(ProjectGreaseSelect.GROUP_MATERIAL))
    fun moveSelectedStroke(dx:Float,dy:Float):Boolean {
        val i=selection.selectedStroke
        if(i<0) return false
        val ok=native.translateStroke(i,dx,dy,0f)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }
    fun hitTestAndSelectStroke(x:Float, y:Float, radius:Float = 24f):Boolean {
        // The select tool is a plain Blender click-select.
        if (tools.activeTool == GreaseTool.SELECT) return pickSelect(x, y)
        val index = native.hitTestStroke(x, y, radius)
        if (index < 0) return false
        // Transform tools: pass-through keeps the current selection when the tapped stroke is
        // part of it, so move/rotate/scale/mirror act on everything that is selected.
        runSelectCommand(ProjectGreaseSelect.pick(
            x, y, ProjectGreaseSelect.pickRadiusSquared(view.zoom),
            ProjectGreaseSelect.PICK_ENTIRE or ProjectGreaseSelect.PICK_PASSTHROUGH,
            ProjectGreaseSelect.MODE_STROKE))
        if (selectionPivot() == null) return selection.selectStroke(index)
        selection.note(index)
        return true
    }
    var eraserMode = EraserMode.HARD
        private set
    private var eraseGestureChanged = false
    fun setEraserMode(value:EraserMode) { eraserMode = value }
    /** Eraser radius in canvas units: the brush Size, as Blender's eraser brush size. */
    fun eraserRadius():Float = brushes.size.coerceAtLeast(0.5f)

    fun eraseAt(x:Float, y:Float, radius:Float = eraserRadius(), recordHistory:Boolean = true):Boolean {
        val ok = when (eraserMode) {
            EraserMode.HARD -> native.eraseAt(x, y, radius)
            EraserMode.SOFT -> native.handle != 0L &&
                GPNative.nativeSoftEraseAt(native.handle, x, y, radius, brushes.strength)
            EraserMode.STROKE -> {
                val index = native.hitTestStroke(x, y, radius)
                index >= 0 && native.deleteStroke(index)
            }
        }
        if (ok) {
            selection.clear()
            eraseGestureChanged = true
            document.markDirty()
            if (recordHistory) {
                history.markEdit()
                eraseGestureChanged = false
                render()
            }
        }
        return ok
    }
    fun endErase() {
        if (eraseGestureChanged) history.markEdit()
        eraseGestureChanged = false
    }
    fun deleteSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.deleteStroke(i);if(ok){selection.clear();history.markEdit();document.markDirty();render()};return ok}
    fun deleteLastStroke():Boolean{val ok=native.deleteLastStroke();if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun duplicateSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.duplicateStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun translateSelectedStroke(dx:Float,dy:Float):Boolean{if(transformSettings.needsEdit9&&selectionPivot()!=null)return edit9Translate(dx,dy);if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.translate(dx,dy));val i=selection.selectedStroke;if(i<0)return false;val ok=native.translateStroke(i,dx,dy,0f);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun flipSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.flipStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    /** Median of every selected point (native stroke_center(-1)); null when nothing is selected. */
    fun selectionPivot():FloatArray? = native.strokeCenter(-1)?.takeIf { it.size >= 2 }
    fun selectedStrokeCenter():FloatArray? {
        selectionPivot()?.let { return it }
        val i=selection.selectedStroke
        if(i<0) return null
        return native.strokeCenter(i)
    }
    /** Tapping selects whole strokes (true) or the nearest point only (false, Blender's point mode). */
    var pickEntireStrokes = true
        private set
    fun setPickEntireStrokes(value:Boolean) { pickEntireStrokes = value }

    /** Blender gpencil_select_exec(): nearest point under the tap; a tap on empty space deselects. */
    fun pickSelect(x:Float, y:Float):Boolean {
        val flags = ProjectGreaseSelect.PICK_DESELECT_ALL or
            (if (pickEntireStrokes) ProjectGreaseSelect.PICK_ENTIRE else 0)
        val radiusSquared = ProjectGreaseSelect.pickRadiusSquared(view.zoom)
        val changed = runSelectCommand(
            if (selectMode == ProjectGreaseSelect.MODE_SEGMENT) ProjectGreaseSelect.segmentPick(x, y, radiusSquared, ProjectGreaseSelect.PICK_DESELECT_ALL)
            else ProjectGreaseSelect.pick(x, y, radiusSquared, flags, ProjectGreaseSelect.areaMode(selectMode)))
        val index = native.hitTestStroke(x, y, 24f)
        if (index >= 0) selection.note(index) else if (changed) selection.clear()
        return changed || index >= 0
    }
    fun dashSelection(dash:Int=3, gap:Int=2) = runSelectCommand(ProjectGreaseSelect.dash(dash, gap))
    fun multiplySelection(copies:Int=2, distance:Float=8f) = runSelectCommand(ProjectGreaseSelect.multiply(copies, distance))
    fun arraySelection(count:Int=3, dx:Float=60f, dy:Float=0f) = runSelectCommand(ProjectGreaseSelect.array(count, dx, dy))
    fun mergeSelectionByDistance(threshold:Float=2f) = runSelectCommand(ProjectGreaseSelect.mergeByDistance(threshold))
    fun toggleSelectionCaps(type:Int=ProjectGreaseSelect.CAPS_TOGGLE_BOTH) = runSelectCommand(ProjectGreaseSelect.caps(type))
    fun setSelectionStartPoint() = runSelectCommand(ProjectGreaseSelect.startSet())
    fun separateSelectionToLayer() = runSelectCommand(ProjectGreaseSelect.separateToLayer())
    fun separateSelection(mode:Int=ProjectGreaseSelect.SEPARATE_POINT) = runSelectCommand(ProjectGreaseSelect.separate(mode))
    fun moveSelectionToLayer(index:Int) = runSelectCommand(ProjectGreaseSelect.moveToLayer(index))
    /** Copy does not change the document; call native directly so no undo step is recorded. */
    fun copySelection():Boolean = native.handle != 0L &&
        native.applyEditCommand(ProjectGreaseSelect.CMD_COPY, FloatArray(0))
    fun pasteStrokes() = runSelectCommand(ProjectGreaseSelect.paste())
    fun deleteSelectedStrokes() = runSelectCommand(ProjectGreaseSelect.deleteStrokes()).also { if (it) selection.clear() }
    fun deleteSelectedPoints() = runSelectCommand(ProjectGreaseSelect.deletePoints()).also { if (it) selection.clear() }
    fun applyThicknessModifier(factor:Float, normalize:Boolean=false, thickness:Int=0) =
        runSelectCommand(ProjectGreaseSelect.thicknessModifier(factor, normalize, thickness))
    fun applyOpacityModifier(mode:Int, factor:Float, normalize:Boolean=false, hardness:Float=1f) =
        runSelectCommand(ProjectGreaseSelect.opacityModifier(mode, factor, normalize, hardness))
    fun applyLengthModifier(start:Float, end:Float, mode:Int=ProjectGreaseSelect.LENGTH_RELATIVE) =
        runSelectCommand(ProjectGreaseSelect.lengthModifier(start, end, mode))
    /** Tint the selected strokes toward the current color (vertex colors, like Blender's Tint). */
    fun applyTintModifier(factor:Float, mode:Int=ProjectGreaseSelect.PAINT_BOTH):Boolean {
        val argb = materials.colorArgb
        val r = ((argb shr 16) and 0xFF) / 255f
        val g = ((argb shr 8) and 0xFF) / 255f
        val b = (argb and 0xFF) / 255f
        return runSelectCommand(ProjectGreaseSelect.tintModifier(mode, factor, r, g, b))
    }
    fun applyColorModifier(hue:Float=0.5f, saturation:Float=1f, value:Float=1f, mode:Int=ProjectGreaseSelect.MODIFY_BOTH) =
        runSelectCommand(ProjectGreaseSelect.colorModifier(mode, hue, saturation, value))
    fun arrangeSelection(direction:Int) = runSelectCommand(ProjectGreaseSelect.arrange(direction))
    fun assignActiveMaterialToSelection() = runSelectCommand(ProjectGreaseSelect.setMaterial(materials.activeMaterial))
    fun resetSelectionVertexColor(mode:Int=ProjectGreaseSelect.PAINT_BOTH) = runSelectCommand(ProjectGreaseSelect.resetVertexColor(mode))
    fun flipSelection() = runSelectCommand(ProjectGreaseSelect.flip())
    fun setSelectionCyclic(type:Int) = runSelectCommand(ProjectGreaseSelect.cyclic(type))
    fun snapSelectionToGrid() = runSelectCommand(ProjectGreaseSelect.snapToGrid(view.gridSize))
    fun duplicateSelection() = runSelectCommand(ProjectGreaseSelect.duplicate())
    fun dissolveSelection(type:Int=ProjectGreaseSelect.DISSOLVE_POINTS) = runSelectCommand(ProjectGreaseSelect.dissolve(type))
    fun splitSelection() = runSelectCommand(ProjectGreaseSelect.split())
    fun joinSelection(leaveGaps:Boolean=false) = runSelectCommand(ProjectGreaseSelect.join(leaveGaps))
    // ---- Vertex Paint mode: one undo step per drag (dabs do not snapshot history) ----
    var vertexPaintBrush = ProjectGreaseSelect.VPAINT_DRAW
        private set
    var vertexPaintTarget = ProjectGreaseSelect.PAINT_STROKE
        private set
    fun setVertexPaintBrush(brush:Int) { if (brush in ProjectGreaseSelect.VPAINT_DRAW..ProjectGreaseSelect.VPAINT_TINT) vertexPaintBrush = brush }
    /** Vertex colour palette (Paint.palette swatches): picking one makes it the paint colour. */
    val vertexPalette = androidx.compose.runtime.mutableStateListOf<Int>()
    fun addPaletteColor(argb:Int = materials.colorArgb):Boolean { if (argb in vertexPalette || vertexPalette.size >= 64) return false; vertexPalette.add(argb); document.markDirty(); return true }
    fun removePaletteColor(index:Int):Boolean { if (index !in vertexPalette.indices) return false; vertexPalette.removeAt(index); document.markDirty(); return true }
    fun usePaletteColor(index:Int):Boolean { val c = vertexPalette.getOrNull(index) ?: return false; materials.setColor(c); return true }
    fun setVertexPaintTarget(target:Int) { if (target in ProjectGreaseSelect.PAINT_STROKE..ProjectGreaseSelect.PAINT_BOTH) vertexPaintTarget = target }
    /** Draw mode's Tint tool: paints vertex colour with the active colour instead of drawing. */
    var drawTint by androidx.compose.runtime.mutableStateOf(false)
    /** gpencil_selectmode_vertex (GP_VERTEX_MASK_SELECTMODE_*) and the paint brushes' falloff curve preset. */
    var vertexSelectMask = 0
    var paintCurvePreset = 0
    /** Mirror modifier as copies, about the selection median (Blender uses the object origin). */
    fun mirrorSelectionCopy(axisX:Boolean, axisY:Boolean):Boolean {
        val pivot = selectionPivot() ?: return false
        return runSelectCommand(ProjectGreaseSelect.mirrorCopy(axisX, axisY, pivot[0], pivot[1]))
    }
    // ---- Weight Paint: one undo step per drag ----
    var weightPaintGroup = 0
        private set
    var weightPaintValue = 1f
        private set
    /** GPWEIGHT_TOOL_* (Draw, Blur, Average, Smear). */
    var weightPaintBrush = ToolSession.GPWEIGHT_DRAW
        private set
    /** Draw subtracts the weight instead of adding it (Blender's brush direction). */
    var weightPaintSubtract = false
        private set
    fun setWeightPaintBrush(brush:Int) { if (brush in ToolSession.GPWEIGHT_DRAW..ToolSession.GPWEIGHT_SMEAR) weightPaintBrush = brush }
    fun setWeightPaintSubtract(value:Boolean) { weightPaintSubtract = value }
    fun setWeightPaintGroup(group:Int) { if (group >= 0) weightPaintGroup = group }
    fun setWeightPaintValue(value:Float) { weightPaintValue = value.coerceIn(0f, 1f) }
    /** The group to paint: the document's active vertex group, created ("Group") when there is none. */
    fun syncWeightPaintGroup() {
        if (native.handle == 0L) return
        // Weight Paint creates the first group itself (as Blender does on the first stroke): an undo
        // step of its own, so undoing a paint stroke keeps the group.
        if (native.vertexGroupCount() == 0 && native.vertexGroupAdd("Group") >= 0) { history.markEdit(); document.markDirty() }
        val active = native.vertexGroupActive()
        weightPaintGroup = if (active >= 0) active else 0
    }
    fun vertexGroups():List<String> = (0 until native.vertexGroupCount()).map { native.vertexGroupName(it) ?: "Group" }
    fun selectVertexGroup(group:Int):Boolean {
        if (!native.setVertexGroupActive(group)) return false
        weightPaintGroup = group
        render()
        return true
    }
    private fun vertexGroupChanged(ok:Boolean):Boolean {
        if (ok) { history.markEdit(); document.markDirty(); render() }
        return ok
    }
    fun addVertexGroup(name:String = "Group"):Boolean {
        val index = native.vertexGroupAdd(name)
        if (index >= 0) { native.setVertexGroupActive(index); weightPaintGroup = index }
        return vertexGroupChanged(index >= 0)
    }
    fun renameVertexGroup(group:Int, name:String) = vertexGroupChanged(native.vertexGroupRename(group, name))
    fun removeVertexGroup(group:Int):Boolean {
        val ok = native.vertexGroupRemove(group)
        if (ok) syncWeightPaintGroup()
        return vertexGroupChanged(ok)
    }
    fun applyThicknessModifierWithWeights(factor:Float, invert:Boolean=false) =
        runSelectCommand(ProjectGreaseSelect.thicknessModifierVGroup(weightPaintGroup, invert, factor))
    fun selectByVertexColor(threshold:Float=0.05f, extend:Boolean=false):Boolean {
        val argb = materials.colorArgb
        return runSelectCommand(ProjectGreaseSelect.selectVertexColor(((argb shr 16) and 0xFF)/255f,
            ((argb shr 8) and 0xFF)/255f, (argb and 0xFF)/255f, threshold, extend))
    }
    fun normalizeSelection(mode:Int, value:Float) = runSelectCommand(ProjectGreaseSelect.normalize(mode, value))
    fun simplifySelectionFixed(steps:Int=1) = runSelectCommand(ProjectGreaseSelect.simplifyFixed(steps))
    fun sampleSelection(length:Float) = runSelectCommand(ProjectGreaseSelect.sample(length))
    fun extrudeSelection() = runSelectCommand(ProjectGreaseSelect.extrude())
    private var randomSeed = 0
    fun selectRandom(ratio:Float=0.5f) = runSelectCommand(ProjectGreaseSelect.selectRandom(ratio, randomSeed++))
    /** Insert a blank keyframe at the current frame, shifting later frames (GPENCIL_OT_blank_frame_add). */
    fun insertBlankFrame() = runSelectCommand(ProjectGreaseSelect.blankFrame(animation.currentFrame))
        .also { if (it) { animation.refreshFromNative(); render() } }
    fun setFillColor(argb:Int) = runSelectCommand(ProjectGreaseSelect.fillColor(materials.activeMaterial,
        ((argb shr 16) and 0xFF)/255f, ((argb shr 8) and 0xFF)/255f, (argb and 0xFF)/255f, ((argb ushr 24) and 0xFF)/255f))
    fun cleanLoosePoints(limit:Int=1) = runSelectCommand(ProjectGreaseSelect.cleanLoose(limit))
    fun cleanDuplicateFrames() = runSelectCommand(ProjectGreaseSelect.cleanDuplicateFrames())
        .also { if (it) { animation.refreshFromNative(); render() } }
    fun setSelectionVertexColor(mode:Int=ProjectGreaseSelect.PAINT_STROKE):Boolean {
        val argb = materials.colorArgb
        return runSelectCommand(ProjectGreaseSelect.vcolorSet(mode, ((argb shr 16) and 0xFF)/255f,
            ((argb shr 8) and 0xFF)/255f, (argb and 0xFF)/255f))
    }
    fun invertSelectionVertexColor(mode:Int=ProjectGreaseSelect.PAINT_BOTH) = runSelectCommand(ProjectGreaseSelect.vcolorInvert(mode))
    fun selectionVertexColorBrightnessContrast(b:Float, c:Float, mode:Int=ProjectGreaseSelect.PAINT_BOTH) =
        runSelectCommand(ProjectGreaseSelect.vcolorBrightnessContrast(mode, b, c))
    fun selectionVertexColorHsv(h:Float=0.5f, s:Float=1f, v:Float=1f, mode:Int=ProjectGreaseSelect.PAINT_BOTH) =
        runSelectCommand(ProjectGreaseSelect.vcolorHsv(mode, h, s, v))
    fun selectionVertexColorLevels(offset:Float, gain:Float, mode:Int=ProjectGreaseSelect.PAINT_BOTH) =
        runSelectCommand(ProjectGreaseSelect.vcolorLevels(mode, offset, gain))
    fun rotateSelectedStroke(radians:Float):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.rotateStroke(i,radians);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun rotateSelectedStrokeAround(radians:Float,centerX:Float,centerY:Float):Boolean{if(transformSettings.needsEdit9&&selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.transform(ProjectGreaseSelect.XFORM_ROTATE,snapRotation(radians),0f,transformSettings));if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.rotate(radians,floatArrayOf(centerX,centerY)));val i=selection.selectedStroke;if(i<0)return false;val ok=native.rotateStrokeAbout(i,radians,centerX,centerY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun scaleSelectedStroke(scaleX:Float,scaleY:Float):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.scaleStroke(i,scaleX,scaleY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun scaleSelectedStrokeAround(scaleX:Float,scaleY:Float,centerX:Float,centerY:Float):Boolean{if(transformSettings.needsEdit9&&selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.transform(ProjectGreaseSelect.XFORM_SCALE,scaleX,scaleY,transformSettings));if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.scale(scaleX,scaleY,floatArrayOf(centerX,centerY)));val i=selection.selectedStroke;if(i<0)return false;val ok=native.scaleStrokeAbout(i,scaleX,scaleY,centerX,centerY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun mirrorSelectedStroke(mirrorX:Boolean,mirrorY:Boolean):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.mirrorStroke(i,mirrorX,mirrorY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun mirrorSelectedStrokeAround(mirrorX:Boolean,mirrorY:Boolean,centerX:Float,centerY:Float):Boolean{if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.mirror(mirrorX,mirrorY,floatArrayOf(centerX,centerY)));val i=selection.selectedStroke;if(i<0)return false;val ok=native.mirrorStrokeAbout(i,mirrorX,mirrorY,centerX,centerY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun subdivideSelectedStroke(level:Int=1):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.subdivideStroke(i,level);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun closeSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.closeStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun trimSelectedStroke(from:Int,to:Int,keepSinglePoint:Boolean=true):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.trimStroke(i,from,to,keepSinglePoint);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun reverseSelectedStroke():Boolean {
        val index=selection.selectedStroke
        if(index<0)return false
        val ok=native.reverseStroke(index)
        if(ok)history.markEdit()
        return ok
    }
    fun uniformSubdivideSelectedStroke(targetPoints:Int):Boolean {
        val index=selection.selectedStroke
        if(index<0)return false
        val ok=native.uniformSubdivideStroke(index,targetPoints)
        if(ok)history.markEdit()
        return ok
    }
    fun shrinkSelectedStroke(distance:Float,mode:Int):Boolean {
        val index=selection.selectedStroke
        if(index<0)return false
        val ok=native.shrinkStroke(index,distance,mode)
        if(ok)history.markEdit()
        return ok
    }
    fun randomizeSelectedStrokeColor():Boolean {
        val index=selection.selectedStroke
        if(index<0)return false
        val ok=native.randomizeStrokeColor(index)
        if(ok)history.markEdit()
        return ok
    }

    fun trimSelectedStrokeToIntersection():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.trimStrokeToIntersection(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun splitSelectedStroke(beforeIndex:Int):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.splitStroke(i,beforeIndex);if(ok){history.markEdit();document.markDirty();render()};return ok}
    /**
     * Undo/redo restore the selection with the document (GP_STROKE_SELECT is stroke data): the
     * "selected stroke" the stroke menu acts on follows it instead of being dropped.
     */
    private fun restoreSelectedStroke() {
        val first = native.docQuery(ProjectGreaseSelect.DOC_Q_SELECTED_STROKES)?.firstOrNull()?.toInt()
        if (first != null && first >= 0) selection.note(first) else selection.clear()
    }
    fun undo():Boolean {
        val ok = history.undo()
        if (ok) {
            reapplyOnion()
            restoreSelectedStroke()
            animation.initialize()
            document.markDirty()
            render()
        }
        return ok
    }
    fun redo():Boolean {
        val ok = history.redo()
        if (ok) {
            reapplyOnion()
            restoreSelectedStroke()
            animation.initialize()
            document.markDirty()
            render()
        }
        return ok
    }

    // =========================================================================================
    // Batch 21: keyframe types, frame selection, layer blend / tint / line change, materials,
    // onion filter, canvas handles of modifiers, primitive edit phase, box / circle select.
    // =========================================================================================
    private fun docChanged(ok:Boolean):Boolean { if (ok) { history.markEdit(); document.markDirty(); render() }; return ok }

    /** Key type of keyframe [frame] (BEZT_KEYTYPE_*) on the active layer, 0 when not a keyframe. */
    fun frameKeyType(frame:Int):Int = animation.keyTypes[frame] ?: 0
    /** Sets the key type of keyframe [frame]; the selected keyframes too when [frame] is one of them. */
    /** Pivot, proportional editing and increment snapping (ToolSettings) for selection transforms. */
    var transformSettings by androidx.compose.runtime.mutableStateOf(ProjectGreaseSelect.TransformSettings())
    /** Next canvas tap places the 2D cursor (pivot "2D Cursor"). */
    var placingCursor2D = false
    private var snapRemX = 0f
    private var snapRemY = 0f
    private var snapRemRot = 0f
    fun setPivot(pivot:Int):Boolean { if (pivot !in ProjectGreaseSelect.PIVOT_MEDIAN..ProjectGreaseSelect.PIVOT_CURSOR) return false; transformSettings = transformSettings.copy(pivot = pivot); render(); return true }
    /** 2D cursor placement (tap with the 2D Cursor pivot). */
    fun setCursor2D(x:Float, y:Float) { if (x.isFinite() && y.isFinite()) { transformSettings = transformSettings.copy(cursorX = x, cursorY = y); render() } }
    fun toggleProportional():Boolean { transformSettings = transformSettings.copy(proportional = !transformSettings.proportional); render(); return transformSettings.proportional }
    fun setProportionalConnected(on:Boolean) { transformSettings = transformSettings.copy(connected = on) }
    fun setProportionalFalloff(falloff:Int):Boolean { if (falloff !in ProjectGreaseSelect.FALLOFF_VALUES) return false; transformSettings = transformSettings.copy(falloff = falloff); return true }
    /** Proportional size (pinch while transforming), clamped like Blender's 0.00001..5000 range. */
    fun setProportionalSize(size:Float) { if (size.isFinite()) { transformSettings = transformSettings.copy(size = size.coerceIn(0.00001f, 5000f)); render() } }
    fun setSnapIncrement(increment:Float) { transformSettings = transformSettings.copy(snapIncrement = if (increment.isFinite() && increment > 0f) increment else 0f); snapRemX = 0f; snapRemY = 0f; snapRemRot = 0f }
    /** Incremental drag deltas are accumulated so snapping moves in whole increments without losing motion. */
    private fun edit9Translate(dx:Float, dy:Float):Boolean {
        val inc = transformSettings.snapIncrement
        var mx = dx; var my = dy
        if (inc > 0f) {
            snapRemX += dx; snapRemY += dy
            mx = kotlin.math.round(snapRemX / inc) * inc; my = kotlin.math.round(snapRemY / inc) * inc
            snapRemX -= mx; snapRemY -= my
            if (mx == 0f && my == 0f) return false
        }
        return runSelectCommand(ProjectGreaseSelect.transform(ProjectGreaseSelect.XFORM_TRANSLATE, mx, my, transformSettings))
    }
    private fun snapRotation(radians:Float):Float {
        if (transformSettings.snapIncrement <= 0f) return radians
        val step = (Math.PI / 36.0).toFloat()
        snapRemRot += radians
        val r = kotlin.math.round(snapRemRot / step) * step
        snapRemRot -= r
        return r
    }
    /** Dope sheet (action editor) frame operators; each is one undo step. */
    private fun runFrameCommand(c:ProjectGreaseSelect.Command?):Boolean {
        if (c == null) return false
        val ok = native.applyEditCommand(c.id, c.args)
        animation.refreshKeyInfo()
        return docChanged(ok)
    }
    fun boxSelectFrames(fmin:Int, fmax:Int, extend:Boolean = false) = runFrameCommand(ProjectGreaseSelect.framesSelectRange(fmin, fmax, extend))
    fun moveSelectedFrames(offset:Int) = offset != 0 && runFrameCommand(ProjectGreaseSelect.framesMove(offset))
    fun scaleSelectedFrames(factor:Float) = runFrameCommand(ProjectGreaseSelect.framesScale(animation.currentFrame, factor))
    fun copySelectedFrames():Boolean { val c = ProjectGreaseSelect.framesCopy(); return native.applyEditCommand(c.id, c.args) }
    fun pasteFrames() = runFrameCommand(ProjectGreaseSelect.framesPaste(animation.currentFrame))
    fun setDashSegments(offset:Int, segments:List<Pair<Int,Int>>) = docChanged(ProjectGreaseSelect.dashSegments(offset, segments)?.let { native.applyEditCommand(it.id, it.args) } ?: false)
    /**
     * ACTION_OT_keyframe_type: applies to the selected keyframes when [frame] is one of them or is not a
     * keyframe itself (long press on a hold cell with a selection); otherwise just to [frame].
     */
    fun setFrameKeyType(frame:Int, type:Int):Boolean {
        val selected = animation.selectedFrames.filter { it in animation.keyframes }.toSet()
        val isKey = frame in animation.keyframes
        val targets = when {
            frame in selected -> selected
            !isKey -> selected
            else -> setOf(frame)
        }
        if (targets.isEmpty()) return false
        var any = false
        for (f in targets) ProjectGreaseSelect.frameKeyType(f, type)?.let { any = native.applyEditCommand(it.id, it.args) || any }
        animation.refreshKeyInfo()
        return docChanged(any)
    }
    // ---- scene markers and preview range (TimelineRules); each change is one undo step ----
    private fun timelineChanged(state:TimelineState?):Boolean {
        if (!animation.applyTimeline(state)) return false
        history.markEdit(); document.markDirty(); return true
    }
    private fun markerEdit(rule:(List<TimeMarker>) -> List<TimeMarker>?):Boolean =
        timelineChanged(rule(animation.timeline.markers)?.let { animation.timeline.copy(markers = it) })
    /** MARKER_OT_add at the current frame (named "F_<frame>", selected alone); refused when one is there. */
    fun addMarker(frame:Int = animation.currentFrame) = markerEdit { TimelineRules.addMarker(it, frame) }
    fun renameMarker(name:String) = markerEdit { TimelineRules.renameMarker(it, name) }
    fun moveSelectedMarkers(offset:Int) = markerEdit { TimelineRules.moveMarkers(it, offset) }
    fun deleteSelectedMarkers() = markerEdit { TimelineRules.deleteMarkers(it) }
    /** Marker selection is not an undo step in this app (frame selection is not either). */
    fun selectMarker(frame:Int, extend:Boolean = false):Boolean =
        animation.applyTimeline(animation.timeline.copy(markers = TimelineRules.selectMarker(animation.timeline.markers, frame, extend)))
    fun setPreviewRange(start:Int, end:Int) = timelineChanged(animation.timeline.copy(preview = TimelineRules.setPreviewRange(start, end)))
    fun clearPreviewRange() = timelineChanged(animation.timeline.copy(preview = PreviewRange()))
    /** Timeline drag: moves the current frame to the whole frame under [position] (no undo step, like Blender). */
    fun scrubTo(position:Float):Boolean { val ok = animation.scrubTo(position); if (ok) render(); return ok }

    /** Timeline frame selection (GP_FRAME_SELECT): replace, toggle or extend; drives multiframe editing. */
    fun selectTimelineFrame(frame:Int, mode:Int = ProjectGreaseSelect.FRAME_SELECT_TOGGLE):Boolean {
        val c = ProjectGreaseSelect.frameSelect(frame, mode)
        val ok = native.applyEditCommand(c.id, c.args)
        animation.refreshKeyInfo()
        if (ok) { document.markDirty(); render() }
        return ok
    }
    fun deselectTimelineFrames():Boolean {
        val c = ProjectGreaseSelect.frameDeselect()
        val ok = native.applyEditCommand(c.id, c.args)
        animation.refreshKeyInfo()
        if (ok) render()
        return ok
    }

    // ---- layer blend / tint / line change / pass (saved in the project file) ----
    fun setLayerBlend(mode:Int, layer:Int = selectedLayer):Boolean =
        ProjectGreaseSelect.layerBlend(layer, mode)?.let { docChanged(native.applyEditCommand(it.id, it.args)) } ?: false
    fun setLayerTint(argb:Int, factor:Float, layer:Int = selectedLayer):Boolean {
        val c = colorToFloats(argb)
        val cmd = ProjectGreaseSelect.layerTint(layer, c[0], c[1], c[2], factor.coerceIn(0f, 1f))
        return docChanged(native.applyEditCommand(cmd.id, cmd.args))
    }
    fun setLayerLineChange(px:Int, layer:Int = selectedLayer):Boolean =
        docChanged(ProjectGreaseSelect.layerLineChange(layer, px).let { native.applyEditCommand(it.id, it.args) })
    fun setLayerPass(pass:Int, layer:Int = selectedLayer):Boolean =
        docChanged(ProjectGreaseSelect.layerPass(layer, pass).let { native.applyEditCommand(it.id, it.args) })

    // ---- materials: name, order, lock / hide / solo, line type, pass ----
    /** Gradient fill (GP_MATERIAL_FILL_STYLE_GRADIENT); null turns it back to a solid fill. */
    fun setMaterialGradient(slot:Int, gradient:FloatArray?):Boolean {
        val g = gradient ?: (materialRecord(slot)?.gradient ?: return false)
        return docChanged(ProjectGreaseSelect.materialGradient(slot, gradient != null, g)?.let { native.applyEditCommand(it.id, it.args) } ?: false)
    }
    fun setMaterialOptions(slot:Int, strokeHoldout:Boolean, fillHoldout:Boolean, selfOverlap:Boolean) =
        docChanged(ProjectGreaseSelect.materialOptions(slot, strokeHoldout, fillHoldout, selfOverlap).let { native.applyEditCommand(it.id, it.args) })
    fun materialRecord(slot:Int = materials.activeMaterial):MaterialRecord? = NativeDocumentAdapter(native).materialRecord(slot)
    fun materialName(slot:Int):String = native.materialName(slot)?.takeIf { it.isNotBlank() } ?: "Material ${slot + 1}"
    fun renameMaterial(slot:Int, name:String):Boolean {
        return docChanged(native.setMaterialName(slot, name.trim()))
    }
    /** Moves a slot (GPENCIL_OT_material_slot_move): strokes keep their material, textures follow. */
    fun moveMaterial(slot:Int, delta:Int):Boolean {
        val to = slot + delta
        if (slot !in 0 until native.materialCount() || to !in 0 until native.materialCount()) return false
        val c = ProjectGreaseSelect.materialMove(slot, to)
        if (!native.applyEditCommand(c.id, c.args)) return false
        remapMaterialTextures(slot, to)
        if (materials.activeMaterial == slot) materials.select(to)
        else if (materials.activeMaterial == to) materials.select(slot)
        return docChanged(true)
    }
    private fun remapMaterialTextures(from:Int, to:Int) {
        fun map(slot:Int) = when {
            slot == from -> to
            from < to && slot in (from + 1)..to -> slot - 1
            from > to && slot in to until from -> slot + 1
            else -> slot
        }
        val t = HashMap(materialTextures); val img = HashMap(textureImages)
        clearTextures()
        for ((k, v) in t) materialTextures[map(k / 2) * 2 + k % 2] = v
        for ((k, v) in img) textureImages[map(k / 2) * 2 + k % 2] = v
        reapplyTextureSettings(); reuploadTextures()
    }
    fun setMaterialLocked(slot:Int, locked:Boolean):Boolean {
        val r = materialRecord(slot) ?: return false
        return docChanged(ProjectGreaseSelect.materialFlags(slot, locked, !r.visible).let { native.applyEditCommand(it.id, it.args) })
    }
    fun setMaterialHidden(slot:Int, hidden:Boolean):Boolean {
        val r = materialRecord(slot) ?: return false
        return docChanged(ProjectGreaseSelect.materialFlags(slot, r.locked, hidden).let { native.applyEditCommand(it.id, it.args) })
    }
    fun soloMaterial(slot:Int = materials.activeMaterial):Boolean =
        docChanged(ProjectGreaseSelect.materialSolo(slot).let { native.applyEditCommand(it.id, it.args) })
    fun setMaterialLineType(mode:Int, alignment:Int = 0, rotation:Float = 0f, slot:Int = materials.activeMaterial):Boolean =
        ProjectGreaseSelect.materialMode(slot, mode, alignment, rotation)?.let { docChanged(native.applyEditCommand(it.id, it.args)) } ?: false
    fun setMaterialPass(pass:Int, slot:Int = materials.activeMaterial):Boolean =
        docChanged(ProjectGreaseSelect.materialPass(slot, pass).let { native.applyEditCommand(it.id, it.args) })

    // ---- onion keyframe-type filter and loop (bGPdata.onion_keytype / GP_ONION_LOOP) ----
    fun setOnionFilter(keyType:Int = onion.keyTypeFilter, loop:Boolean = onion.loop):Boolean {
        val c = ProjectGreaseSelect.onionFilter(keyType, loop) ?: return false
        native.applyEditCommand(c.id, c.args)
        onion.setFilter(keyType, loop)
        render()
        return true
    }

    // ---- modifier custom curve and canvas handles ----
    fun setModifierCurve(index:Int, use:Boolean, points:List<Pair<Float,Float>>, layer:Int = selectedLayer):Boolean {
        val m = modifiers(layer).getOrNull(index) ?: return false
        return modifierChanged(ModifierStackCommands.setAll(native, layer, index, ModifierSpecs.withCurve(m.params, use, CurvePoints.clean(points).take(7))))
    }
    /** The modifier whose handles are shown and dragged on the canvas (Hook, Lattice, Weight Proximity, Mirror). */
    var gizmo:Pair<Int,Int>? = null; private set
    val gizmoActive:Boolean get() = gizmo != null
    fun editModifierHandles(index:Int?, layer:Int = selectedLayer) {
        gizmo = index?.let { layer to it }
        onOverlayChanged?.invoke()
        render()
    }
    fun gizmoHandles():List<Pair<Float,Float>> {
        val (layer, index) = gizmo ?: return emptyList()
        val m = modifiers(layer).getOrNull(index) ?: return emptyList()
        return ModifierSpecs.canvasHandles(m.type, m.params)
    }
    private var gizmoDrag = -1
    private fun gizmoPress(x:Float, y:Float):Boolean {
        val handles = gizmoHandles()
        gizmoDrag = CurvePoints.hit(handles, x, y, curveHitRadius())
        return gizmoDrag >= 0
    }
    private fun gizmoMove(x:Float, y:Float) {
        val (layer, index) = gizmo ?: return
        if (gizmoDrag < 0) return
        val m = modifiers(layer).getOrNull(index) ?: return
        if (ModifierStackCommands.setAll(native, layer, index, ModifierSpecs.moveHandle(m.type, m.params, gizmoDrag, x, y))) {
            document.markDirty(); render(); onOverlayChanged?.invoke()
        }
    }
    private fun gizmoRelease() { if (gizmoDrag >= 0) history.markEdit(); gizmoDrag = -1 }

    // ---- primitive edit phase (gpencil_primitive.c IN_PROGRESS / edit handles) ----
    private val shapeEdit = ShapeEditSession()
    val shapeEditing:Boolean get() = shapeEdit.isActive
    fun shapeHandles():List<Pair<Float,Float>> = shapeEdit.handles()
    /** Subdivisions (Blender's "edges", + / - keys); 0 = Blender's default for the type. */
    fun shapeSubdivisions():Int = shapeEdit.edges
    fun changeShapeSubdivisions(delta:Int):Boolean {
        if (!shapeEdit.isActive) return false
        val base = if (shapeEdit.edges > 0) shapeEdit.edges else GPNative.nativeBlenderPrimitiveDefaultEdges(shapeEdit.type).coerceAtLeast(1)
        shapeEdit.edges = (base + delta).coerceIn(1, 128)
        showShapePreview(); return true
    }
    /** Extrude (E key): a line becomes a polyline with a new end point to drag. */
    fun extrudeShape():Boolean = shapeEdit.extrude().also { if (it) showShapePreview() }
    fun confirmShape():Boolean {
        if (!shapeEdit.isActive) return false
        val type = shapeEdit.effectiveType()
        val points = blenderPrimitivePointsEdges(type, shapeEdit.anchors(), shapeEdit.edges)
        shapeEdit.reset(); showPrimitivePreview(null); onOverlayChanged?.invoke()
        val ok = points != null && commitPrimitivePoints(points, ProjectGreasePrimitive.isCyclic(type))
        if (!ok) render()
        return ok
    }
    fun cancelShape() { shapeEdit.reset(); showPrimitivePreview(null); onOverlayChanged?.invoke(); render() }
    private fun showShapePreview() {
        showPrimitivePreview(blenderPrimitivePointsEdges(shapeEdit.effectiveType(), shapeEdit.anchors(), shapeEdit.edges))
        onOverlayChanged?.invoke()
    }
    private fun blenderPrimitivePointsEdges(type:Int, anchors:List<Pair<Float,Float>>, edges:Int):FloatArray? {
        if (anchors.size < 2) return null
        val packed = FloatArray(anchors.size * 2)
        anchors.forEachIndexed { i, p -> packed[i * 2] = p.first; packed[i * 2 + 1] = p.second }
        val points = GPNative.nativeGenerateBlenderPrimitive(type, packed, edges, false) ?: return null
        return if (points.size >= 4 && points.size % 2 == 0) points else null
    }

    // ---- box / circle select tools ----
    private var areaStart:Pair<Float,Float>? = null
    private var areaFirstDab = true
    /** Box being dragged (canvas units, x0 y0 x1 y1) for the overlay, or null. */
    var boxSelectRect:FloatArray? = null; private set
    fun circleSelectRadius():Float = brushes.size.coerceAtLeast(4f)
}
