package com.smitnk.projectgrease.editor

import kotlin.math.pow

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
    fun materialInfo(index: Int) = if (handle != 0L) GPNative.nativeGetMaterialInfo(handle, index) else null
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

    // History is the actual Blender Legacy GP datablock state, not a UI flag.
    fun markEdit(): Boolean = native.historyRecord()
    fun reset(): Boolean = native.historyReset()
    fun undo(): Boolean = native.historyUndo()
    fun redo(): Boolean = native.historyRedo()
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
    var currentFrame = 1; private set
    var fps = 12; private set
    var playing = false; private set
    var loop = true; private set
    var frameCount = 1; private set
    var timelineEnd = 1; private set
    /** Scene end frame (a template's frame_end); the timeline shows at least this many frames. 0 = none. */
    var sceneEnd = 0; private set
    fun setSceneEnd(value:Int) { sceneEnd = value.coerceIn(0, 100000); timelineEnd = endFrame() }
    private fun endFrame() = maxOf(native.frameEnd(), sceneEnd).coerceAtLeast(1)
    private val handler = android.os.Handler(android.os.Looper.getMainLooper())
    private val tick = object : Runnable {
        override fun run() {
            if (!playing || native.handle == 0L) return
            val end = endFrame()
            timelineEnd = end
            var next = currentFrame + 1
            if (next > end) {
                if (loop) next = 1 else {
                    playing = false
                    return
                }
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
            timelineEnd = endFrame()
        }
    }
    fun setFrame(value: Int): Boolean {
        val target = value.coerceAtLeast(1)
        if (native.handle == 0L) return false
        if (!native.selectFrameOrHold(target)) return false
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        timelineEnd = endFrame()
        return true
    }
    fun ensureFrame(frameNumber: Int): Boolean {
        val target = frameNumber.coerceAtLeast(1)
        if (native.handle == 0L) return false
        if (native.selectFrame(target)) {
            currentFrame = target
            frameCount = native.frameCount().coerceAtLeast(1)
            timelineEnd = endFrame()
            return true
        }
        if (!native.createFrame(target)) return false
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        timelineEnd = endFrame()
        return true
    }
    fun duplicateFrame(sourceFrame:Int,targetFrame:Int):Boolean {
        if (native.handle == 0L || targetFrame < 1) return false
        if (!native.duplicateFrame(sourceFrame,targetFrame)) return false
        currentFrame=targetFrame; frameCount=native.frameCount().coerceAtLeast(1); timelineEnd=endFrame(); return true
    }
    fun frameNumbers(): IntArray = native.frameNumbers()
    /** Re-reads frame count/end and re-selects the current frame (or its hold) after native frame edits. */
    fun refreshFromNative(): Boolean = setFrame(currentFrame)

    /** Interpolation easing (Blender's gpencil_interpolate easing): type Linear..Bounce, mode In/Out/In-Out. */
    var easingType = ProjectGreaseSelect.EASE_LINEAR
        private set
    var easingMode = ProjectGreaseSelect.EASE_IN
        private set
    fun setEasing(type:Int, mode:Int) {
        if (type in ProjectGreaseSelect.EASE_LINEAR..ProjectGreaseSelect.EASE_BOUNCE) easingType = type
        if (mode in ProjectGreaseSelect.EASE_IN..ProjectGreaseSelect.EASE_IN_OUT) easingMode = mode
    }
    fun interpolateAt(frame:Int):Boolean {
        if (native.handle == 0L) return false
        val keys = native.frameNumbers().sorted()
        val previous = keys.lastOrNull { it < frame } ?: return false
        val next = keys.firstOrNull { it > frame } ?: return false
        val span = (next - previous).coerceAtLeast(1)
        val factor = (frame - previous).toFloat() / span.toFloat()
        if (!native.interpolateFrame(previous,next,frame,factor,easingType,easingMode)) return false
        currentFrame=frame
        frameCount=native.frameCount().coerceAtLeast(1)
        timelineEnd=endFrame()
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
        timelineEnd = endFrame()
        return true
    }

    fun setFps(value:Int){
        fps=value.coerceIn(1,120)
        if(playing){handler.removeCallbacks(tick);handler.postDelayed(tick,(1000L/fps).coerceAtLeast(1L))}
    }
    fun togglePlayback(){
        playing=!playing
        handler.removeCallbacks(tick)
        if(playing) handler.post(tick)
    }
    fun toggleLoop(){loop=!loop}
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
    fun setThickness(value:Float){thickness=value.coerceIn(0.5f,100f)}
    fun setOpacity(value:Float){opacity=value.coerceIn(0f,1f)}
}

enum class GreaseMode { DRAW, EDIT, SCULPT, VERTEX_PAINT, WEIGHT_PAINT }

enum class BrushPreset {
    PENCIL, PEN, INK, MARKER, AIRBRUSH
}

class BrushController(private val materials: MaterialController) {
    var preset = BrushPreset.PENCIL
        private set
    var size = materials.thickness
        private set
    var strength = 1f
        private set
    var pressureCurve = 1f
        private set

    fun select(value: BrushPreset) {
        preset = value
        when (value) {
            BrushPreset.PENCIL -> apply(4f, 0.80f, 1.15f)
            BrushPreset.PEN -> apply(7f, 0.95f, 1.0f)
            BrushPreset.INK -> apply(5f, 1.0f, 0.85f)
            BrushPreset.MARKER -> apply(14f, 0.75f, 0.9f)
            BrushPreset.AIRBRUSH -> apply(24f, 0.35f, 0.7f)
        }
    }

    fun setSize(value: Float) {
        size = value.coerceIn(0.5f, 100f)
        materials.setThickness(size)
    }

    fun setStrength(value: Float) {
        strength = value.coerceIn(0f, 1f)
    }

    fun setPressureCurve(value: Float) {
        pressureCurve = value.coerceIn(0.25f, 3f)
    }

    fun pressure(input: Float): Float =
        input.coerceIn(0f, 1f).let { it.toDouble().pow(pressureCurve.toDouble()).toFloat() }

    private fun apply(newSize: Float, newStrength: Float, curve: Float) {
        size = newSize
        strength = newStrength
        pressureCurve = curve
        materials.setThickness(size)
    }
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
    SMOOTH, THICKNESS, STRENGTH, GRAB, PUSH, PINCH, TWIST, RANDOMIZE
}

class SculptController(private val native: NativeEditorBridge) {
    var brush = SculptBrush.SMOOTH
        private set

    private val engine = LegacyGpSculptEngine { native.handle }
    private val settings = LegacyGpSculptEngine.Settings()

    fun isAvailable() =
        FeatureRegistry.capability(FeatureId.SCULPT).state != FeatureState.NOT_IMPLEMENTED

    fun select(value: SculptBrush) { brush = value }

    fun setRadius(value: Float) { settings.radius = value.coerceIn(2f, 300f) }

    fun setStrength(value: Float) { settings.strength = value.coerceIn(0f, 1f) }

    fun setPressureCurve(value: Float) {
        settings.pressureCurve = value.coerceIn(0.25f, 4f)
    }

    fun setInvert(value: Boolean) { settings.invert = value }

    private fun tool(): LegacyGpSculptEngine.Tool = when (brush) {
        SculptBrush.SMOOTH -> LegacyGpSculptEngine.Tool.SMOOTH
        SculptBrush.THICKNESS -> LegacyGpSculptEngine.Tool.THICKNESS
        SculptBrush.STRENGTH -> LegacyGpSculptEngine.Tool.STRENGTH
        SculptBrush.GRAB -> LegacyGpSculptEngine.Tool.GRAB
        SculptBrush.PUSH -> LegacyGpSculptEngine.Tool.PUSH
        SculptBrush.PINCH -> LegacyGpSculptEngine.Tool.PINCH
        SculptBrush.TWIST -> LegacyGpSculptEngine.Tool.TWIST
        SculptBrush.RANDOMIZE -> LegacyGpSculptEngine.Tool.RANDOMIZE
    }

    fun begin(x: Float, y: Float, radius: Float, pressure: Float = 1f): Boolean {
        settings.radius = radius.coerceIn(2f, 300f)
        settings.pressure = pressure.coerceIn(0f, 1f)
        settings.strength = settings.strength.coerceIn(0f, 1f)
        return engine.begin(tool(), x, y, settings)
    }

    fun update(x: Float, y: Float, pressure: Float = 1f): Boolean {
        settings.pressure = pressure.coerceIn(0f, 1f)
        return engine.update(x, y, settings)
    }

    fun end() = engine.end()

    fun cancel() = engine.cancel()
}

class OnionSkinController {
    var enabled=false; private set
    var beforeFrames=2; private set
    var afterFrames=2; private set
    var opacity=0.35f; private set
    var fade=true; private set
    var selectedLayerOnly=true; private set
    fun toggle(){enabled=!enabled}
    fun setBefore(value:Int){beforeFrames=value.coerceIn(0,12)}
    fun setAfter(value:Int){afterFrames=value.coerceIn(0,12)}
    fun setOpacity(value:Float){opacity=value.coerceIn(0f,1f)}
    fun setFade(value:Boolean){fade=value}
}

enum class GreaseTool { DRAW, ERASE, SELECT, LASSO, FILL, EYEDROPPER, LINE, RECTANGLE, CIRCLE, ARC, POLYLINE, CURVE, ANNOTATE, MOVE, ROTATE, SCALE, MIRROR, PAN, SCULPT }

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
    val history=HistoryController(native)
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
    val sculpt=SculptController(native)
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
    }
    fun detachRenderer(){animation.stop();rendererHandle=0L;native.detach()}
    fun resetDocument():Boolean {
        if (rendererHandle == 0L) return false
        val ok=GPNative.nativeResetDocumentEgl(rendererHandle)
        if(ok){
            reapplyOnion()
            selectedLayer=0
            animation.setSceneEnd(0)
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
        return tools.select(tool)
    }
    private var sculptGestureChanged = false
    private fun applySculptPoint(x:Float,y:Float,pressure:Float=1f):Boolean {
        if (rendererHandle == 0L) return false
        val radius = (brushes.size * 2.0f).coerceIn(8f, 180f)
        sculpt.setStrength(brushes.strength)
        sculpt.setPressureCurve(brushes.pressureCurve)
        val ok = sculpt.update(x, y, pressure)
        if (ok) {
            sculptGestureChanged = true
            document.markDirty()
        }
        return ok
    }
    fun beginSculpt(x:Float,y:Float,pressure:Float=1f):Boolean {
        if (rendererHandle == 0L) return false
        val radius = (brushes.size * 2.0f).coerceIn(8f, 180f)
        sculpt.setStrength(brushes.strength)
        sculpt.setPressureCurve(brushes.pressureCurve)
        val ok = sculpt.begin(x, y, radius, pressure)
        if (ok) {
            sculptGestureChanged = true
            document.markDirty()
        }
        return ok
    }
    fun sculptAt(x:Float,y:Float,pressure:Float=1f):Boolean = applySculptPoint(x,y,pressure)
    fun endSculpt() {
        sculpt.end()
        if (sculptGestureChanged) history.markEdit()
        sculptGestureChanged = false
    }
    fun selectBrush(preset:BrushPreset) {
        brushes.select(preset)
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
    private val pendingLassoPoints = mutableListOf<Pair<Float,Float>>()
    private var pendingShapeTool: GreaseTool? = null
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
    private val brushStrokeEngine = LegacyGpBrushStrokeEngine()

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

    private fun sendBrushPoints(points:List<LegacyGpBrushStrokeEngine.StrokePoint>) {
        points.forEach { point ->
            GPNative.nativeAddPointEglRenderer(
                rendererHandle, point.x, point.y, 0f,
                point.pressure.coerceAtLeast(TouchInputRules.MIN_NATIVE_PRESSURE),
                point.strength.coerceAtLeast(0f),
                point.time
            )
        }
    }

    private fun beginLegacyBrushStroke() {
        brushStrokeEngine.begin(
            LegacyGpBrushStrokeEngine.Settings(
                drawStrength=brushes.strength,
                usePressure=true,
                useStrengthPressure=false,
                pressureCurve=brushes.pressureCurve,
                inputSamples=legacyInputSamples,
                lazyEnabled=legacyLazyEnabled,
                smoothStrokeRadius=legacyLazyRadius,
                smoothStrokeFactor=legacyLazyFactor,
                disableStabilizer=legacyDisableStabilizer,
                manhattanThreshold=legacyManhattanThreshold,
                euclideanThreshold=legacyEuclideanThreshold,
                activeSmooth=legacyActiveSmooth,
                jitter=legacyJitter,
                drawAngleFactor=legacyDrawAngleFactor,
                drawAngle=legacyDrawAngle
            )
        )
    }

    fun beginStroke():Boolean {
        if (rendererHandle == 0L) return false
        return when (tools.activeTool) {
            GreaseTool.DRAW -> {
                pendingShapePoints.clear()
                pendingShapeTool = null
                beginLegacyBrushStroke()
                GPNative.nativeBeginStrokeEglRenderer(rendererHandle, materials.activeMaterial, materials.thickness)
            }
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
        if (tools.activeTool == GreaseTool.LASSO) {
            val snapped=view.snapPoint(x,y)
            pendingLassoPoints += snapped.first to snapped.second
        } else if (tools.activeTool == GreaseTool.DRAW) {
            val snapped=view.snapPoint(x,y)
            val emitted = brushStrokeEngine.add(
                LegacyGpBrushStrokeEngine.InputEvent(
                    snapped.first, snapped.second, pressure.coerceIn(0f,1f), timeSeconds
                )
            )
            sendBrushPoints(emitted)
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
        if (tools.activeTool == GreaseTool.LASSO) {
            val noose = pendingLassoPoints.toList()
            pendingLassoPoints.clear()
            runSelectCommand(ProjectGreaseSelect.lasso(selectOp, ProjectGreaseSelect.areaMode(selectMode), noose))
            return
        }
        if (tools.activeTool == GreaseTool.DRAW) {
            // The engine holds back the newest points (Blender edits them in place); flush them
            // into the native buffer before the stroke is committed.
            sendBrushPoints(brushStrokeEngine.end())
            if (GPNative.nativeEndStrokeEglRenderer(rendererHandle)) {
                history.markEdit(); document.markDirty()
            }
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
            commitPrimitivePoints(finalPoints, ProjectGreasePrimitive.isCyclic(type))
        }
    }

    fun cancelStroke(){
        if (rendererHandle != 0L && tools.activeTool == GreaseTool.DRAW) {
            brushStrokeEngine.cancel()
            GPNative.nativeCancelStrokeEglRenderer(rendererHandle)
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
    }
    fun setOnionSkin(enabled:Boolean,before:Int=2,after:Int=2,opacity:Float=0.35f):Boolean {
        val ok = native.setOnionSkin(enabled,before,after,opacity)
        if (ok) { if (enabled != onion.enabled) onion.toggle(); onion.setBefore(before); onion.setAfter(after); onion.setOpacity(opacity); render() }
        return ok
    }
    /** Vector pages (SVG/PDF export) for [frames]; the layer/frame selection is restored afterwards. */
    fun exportPages(frames:List<Int>, includeAnnotations:Boolean = false):List<VectorPage> {
        if (native.handle == 0L || frames.isEmpty()) return emptyList()
        val originalLayer = selectedLayer
        val originalFrame = animation.currentFrame
        var pages = VectorExport.pages(NativeDocumentAdapter(native), document.canvasWidth, document.canvasHeight, frames)
        if (includeAnnotations) {
            val dump = annotationDump()
            val style = annotationStyle()
            pages = pages.map { page ->
                val notes = AnnotationData.exportLayer(dump, style, page.frame)
                if (notes == null) page else VectorPage(page.frame, page.width, page.height, page.layers + notes)
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
            document.canvasWidth, document.canvasHeight, animation.fps, originalFrame, animation.sceneEnd
        )
        if (native.layerCount() > 0) {
            native.selectLayer(originalLayer.coerceIn(0, native.layerCount() - 1))
            native.selectFrameOrHold(originalFrame)
        }
        render()
        // Annotations are separate data with their own key (older files have none).
        val notes = AnnotationData.toJson(annotationDump(), annotationStyle()) ?: return json
        return runCatching { org.json.JSONObject(json).put("annotations", notes).toString() }.getOrDefault(json)
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
        if (!ProjectDocumentCodec.restore(parsed, NativeDocumentAdapter(native), brushes.size)) return false
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
    // Fill tool options (Blender fill brush): leak size, dilate (negative contracts), boundary source.
    var fillLeak = 3; private set
    var fillDilate = 1; private set
    var fillBoundary = FILL_BOUNDARY_ALL; private set
    fun setFillOptions(leak:Int = fillLeak, dilate:Int = fillDilate, boundary:Int = fillBoundary) {
        fillLeak = leak.coerceIn(1, 100)
        fillDilate = dilate.coerceIn(-40, 40)
        fillBoundary = boundary.coerceIn(FILL_BOUNDARY_ALL, FILL_BOUNDARY_EDIT_LINES)
    }
    fun fillAt(x: Float, y: Float): Boolean {
        if (rendererHandle == 0L) return false
        // Blender Legacy GP Fill creates a closed filled stroke using the active material.
        // Enable the material's Fill component only when the Fill tool is actually used.
        if (!materials.fillEnabled) {
            setMaterialFillEnabled(true)
        }
        GPNative.nativeSetFillOptionsEglRenderer(rendererHandle, fillLeak, fillDilate, fillBoundary)
        val ok = GPNative.nativeFillAtEglRenderer(rendererHandle, x.toInt(), y.toInt(), materials.activeMaterial, materials.thickness)
        if (ok) { history.markEdit(); document.markDirty() }
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
        val ok=native.setMaterialColors(materials.activeMaterial,stroke,fill)
        if(ok) pushMaterialColor()
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
     * Delete material slot `index` (edit4 pg_gp_material_slot_remove): its strokes are deleted, higher
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

    fun setMaterialFillEnabled(enabled:Boolean):Boolean {
        materials.setFillEnabled(enabled)
        return native.setMaterialFillEnabled(materials.activeMaterial,enabled)
    }
    fun pushMaterialColor(){
        if(rendererHandle==0L)return
        val c=colorToFloats(materials.colorArgb)
        // Strength is the active material alpha for the focused Android
        // presentation path. The previous implementation sent the palette
        // alpha unchanged, so the Strength control had no visible effect.
        GPNative.nativeSetStrokeColorEglRenderer(
            rendererHandle,
            c[0], c[1], c[2], c[3] * materials.opacity * brushes.strength
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
            GPNative.nativeRenderEgl(rendererHandle)
        }
    }
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
        if (native.handle == 0L || template.layers.isEmpty()) return false
        if (native.layerCount() == 0) {
            if (!native.createLayer(template.layers[0])) return false
        } else {
            native.renameLayer(0, template.layers[0])
        }
        native.selectLayer(0)
        if (native.frameCount() == 0) native.createFrame(1)
        for (name in template.layers.drop(1)) {
            if (!native.createLayer(name)) return false
            native.createFrame(1)
        }
        template.materials.forEachIndexed { i, m ->
            while (native.materialCount() <= i) if (!native.createMaterial()) return false
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
    fun selectFrame(frame:Int):Boolean {
        val ok = animation.ensureFrame(frame)
        if (ok) render()
        return ok
    }
    fun frameNumbers(): IntArray = native.frameNumbers()
    fun strokeCount() = native.strokeCount()
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
    fun eraserRadius():Float = brushes.size.coerceIn(8f, 96f)

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
    fun translateSelectedStroke(dx:Float,dy:Float):Boolean{if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.translate(dx,dy));val i=selection.selectedStroke;if(i<0)return false;val ok=native.translateStroke(i,dx,dy,0f);if(ok){history.markEdit();document.markDirty();render()};return ok}
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
    private var vertexPaintChanged = false
    fun setVertexPaintBrush(brush:Int) { if (brush in ProjectGreaseSelect.VPAINT_DRAW..ProjectGreaseSelect.VPAINT_REPLACE) vertexPaintBrush = brush }
    fun setVertexPaintTarget(target:Int) { if (target in ProjectGreaseSelect.PAINT_STROKE..ProjectGreaseSelect.PAINT_BOTH) vertexPaintTarget = target }
    /** [render] is false when the caller paints several samples and renders once afterwards. */
    fun vertexPaintDab(x:Float, y:Float, dx:Float=0f, dy:Float=0f, pressure:Float=1f, render:Boolean=true):Boolean {
        if (native.handle == 0L) return false
        val argb = materials.colorArgb
        val cmd = ProjectGreaseSelect.vertexPaint(vertexPaintBrush, x, y, brushes.size.coerceAtLeast(1f),
            (brushes.strength * pressure).coerceIn(0f, 1f),
            ((argb shr 16) and 0xFF) / 255f, ((argb shr 8) and 0xFF) / 255f, (argb and 0xFF) / 255f,
            vertexPaintTarget, dx, dy) ?: return false
        val changed = native.applyEditCommand(cmd.id, cmd.args)
        if (changed) { vertexPaintChanged = true; document.markDirty(); if (render) render() }
        return changed
    }
    fun endVertexPaint() { if (vertexPaintChanged) history.markEdit(); vertexPaintChanged = false }
    /** One dab of the active paint mode (Vertex Paint colors or Weight Paint weights). */
    fun paintModeDab(x:Float, y:Float, dx:Float=0f, dy:Float=0f, pressure:Float=1f, render:Boolean=true):Boolean = when (mode) {
        GreaseMode.VERTEX_PAINT -> vertexPaintDab(x, y, dx, dy, pressure, render)
        GreaseMode.WEIGHT_PAINT -> weightPaintDab(x, y, pressure, render)
        else -> false
    }
    /** Ends the drag of either paint mode: one undo step. */
    fun endPaintMode() { endVertexPaint(); endWeightPaint() }
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
    private var weightPaintChanged = false
    fun setWeightPaintGroup(group:Int) { if (group >= 0) weightPaintGroup = group }
    fun setWeightPaintValue(value:Float) { weightPaintValue = value.coerceIn(0f, 1f) }
    /** The group to paint: the document's active vertex group, created ("Group") when there is none. */
    private fun syncWeightPaintGroup() {
        if (native.handle == 0L) return
        if (native.vertexGroupCount() == 0) native.vertexGroupAdd("Group")
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
    fun weightPaintDab(x:Float, y:Float, pressure:Float=1f, render:Boolean=true):Boolean {
        if (native.handle == 0L) return false
        val cmd = ProjectGreaseSelect.weightPaint(weightPaintGroup, x, y, brushes.size.coerceAtLeast(1f),
            (brushes.strength * pressure).coerceIn(0f, 1f), weightPaintValue) ?: return false
        val changed = native.applyEditCommand(cmd.id, cmd.args)
        if (changed) { weightPaintChanged = true; document.markDirty(); if (render) render() }
        return changed
    }
    fun endWeightPaint() { if (weightPaintChanged) history.markEdit(); weightPaintChanged = false }
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
    fun rotateSelectedStrokeAround(radians:Float,centerX:Float,centerY:Float):Boolean{if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.rotate(radians,floatArrayOf(centerX,centerY)));val i=selection.selectedStroke;if(i<0)return false;val ok=native.rotateStrokeAbout(i,radians,centerX,centerY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun scaleSelectedStroke(scaleX:Float,scaleY:Float):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.scaleStroke(i,scaleX,scaleY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun scaleSelectedStrokeAround(scaleX:Float,scaleY:Float,centerX:Float,centerY:Float):Boolean{if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.scale(scaleX,scaleY,floatArrayOf(centerX,centerY)));val i=selection.selectedStroke;if(i<0)return false;val ok=native.scaleStrokeAbout(i,scaleX,scaleY,centerX,centerY);if(ok){history.markEdit();document.markDirty();render()};return ok}
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
    fun undo():Boolean {
        val ok = history.undo()
        if (ok) {
            reapplyOnion()
            selection.clear()
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
            selection.clear()
            animation.initialize()
            document.markDirty()
            render()
        }
        return ok
    }
}
