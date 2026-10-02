package com.smitnk.projectgrease.editor

import kotlin.math.pow

import com.smitnk.projectgrease.nativebridge.GPNative

class NativeEditorBridge {
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
    fun interpolateFrame(sourceFrame:Int,targetFrame:Int,resultFrame:Int,factor:Float) = handle != 0L && GPNative.nativeInterpolateFrame(handle,sourceFrame,targetFrame,resultFrame,factor)
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
    private val handler = android.os.Handler(android.os.Looper.getMainLooper())
    private val tick = object : Runnable {
        override fun run() {
            if (!playing || native.handle == 0L) return
            val end = native.frameEnd().coerceAtLeast(1)
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
            timelineEnd = native.frameEnd().coerceAtLeast(1)
        }
    }
    fun setFrame(value: Int): Boolean {
        val target = value.coerceAtLeast(1)
        if (native.handle == 0L) return false
        if (!native.selectFrameOrHold(target)) return false
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        timelineEnd = native.frameEnd().coerceAtLeast(1)
        return true
    }
    fun ensureFrame(frameNumber: Int): Boolean {
        val target = frameNumber.coerceAtLeast(1)
        if (native.handle == 0L) return false
        if (native.selectFrame(target)) {
            currentFrame = target
            frameCount = native.frameCount().coerceAtLeast(1)
            timelineEnd = native.frameEnd().coerceAtLeast(1)
            return true
        }
        if (!native.createFrame(target)) return false
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        timelineEnd = native.frameEnd().coerceAtLeast(1)
        return true
    }
    fun duplicateFrame(sourceFrame:Int,targetFrame:Int):Boolean {
        if (native.handle == 0L || targetFrame < 1) return false
        if (!native.duplicateFrame(sourceFrame,targetFrame)) return false
        currentFrame=targetFrame; frameCount=native.frameCount().coerceAtLeast(1); timelineEnd=native.frameEnd().coerceAtLeast(1); return true
    }
    fun frameNumbers(): IntArray = native.frameNumbers()

    fun interpolateAt(frame:Int):Boolean {
        if (native.handle == 0L) return false
        val keys = native.frameNumbers().sorted()
        val previous = keys.lastOrNull { it < frame } ?: return false
        val next = keys.firstOrNull { it > frame } ?: return false
        val span = (next - previous).coerceAtLeast(1)
        val factor = (frame - previous).toFloat() / span.toFloat()
        if (!native.interpolateFrame(previous,next,frame,factor)) return false
        currentFrame=frame
        frameCount=native.frameCount().coerceAtLeast(1)
        timelineEnd=native.frameEnd().coerceAtLeast(1)
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
        timelineEnd = native.frameEnd().coerceAtLeast(1)
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
}

enum class GreaseTool { DRAW, ERASE, SELECT, LASSO, FILL, EYEDROPPER, LINE, RECTANGLE, CIRCLE, ARC, POLYLINE, MOVE, ROTATE, SCALE, MIRROR, PAN, SCULPT }

class ToolController {
    var activeTool=GreaseTool.DRAW; private set
    fun select(tool:GreaseTool):Boolean {
        val feature=when(tool){
            GreaseTool.DRAW->FeatureId.FREEHAND; GreaseTool.ERASE->FeatureId.ERASER
            GreaseTool.SELECT->FeatureId.SELECT; GreaseTool.LASSO->FeatureId.LASSO
            GreaseTool.FILL->FeatureId.FILL; GreaseTool.EYEDROPPER->FeatureId.STROKE_COLOR
            GreaseTool.LINE->FeatureId.LINE; GreaseTool.RECTANGLE->FeatureId.RECTANGLE
            GreaseTool.CIRCLE->FeatureId.CIRCLE; GreaseTool.ARC->FeatureId.ARC
            GreaseTool.POLYLINE->FeatureId.POLYLINE; GreaseTool.MOVE->FeatureId.MOVE; GreaseTool.ROTATE->FeatureId.ROTATE; GreaseTool.SCALE->FeatureId.SCALE; GreaseTool.MIRROR->FeatureId.MIRROR; GreaseTool.PAN->FeatureId.PAN
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
            selectedLayer=0
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
            GreaseMode.VERTEX_PAINT, GreaseMode.WEIGHT_PAINT -> false
        }
        if (!supported) return false
        mode = value
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
            runSelectCommand(ProjectGreaseSelect.lasso(selectOp, selectMode, noose))
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
        if (tools.activeTool == GreaseTool.POLYLINE) {
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
        runSelectCommand(ProjectGreaseSelect.lasso(selectOp, selectMode, points))

    // ---- Blender 3.6.23 Legacy GP selection (native/blender_gp/project_grease_blender_select.c) ----
    // Point vs. stroke select mode and the eSelectOp used by lasso/box/circle selection.
    var selectMode = ProjectGreaseSelect.MODE_POINT
        private set
    var selectOp = ProjectGreaseSelect.OP_SET
        private set
    fun setSelectMode(value:Int):Boolean {
        if (!ProjectGreaseSelect.isValidMode(value)) return false
        selectMode = value
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
        runSelectCommand(ProjectGreaseSelect.box(selectOp, selectMode, x0, y0, x1, y1))
    /** One dab of a circle-select gesture; only the first dab of a SET gesture replaces the selection. */
    fun selectCircleAt(x:Float, y:Float, radius:Float, isFirst:Boolean) =
        runSelectCommand(ProjectGreaseSelect.circle(selectOp, selectMode, x, y, radius, isFirst))

    fun fillSelectedStroke():Boolean {
        val i=selection.selectedStroke
        if(i<0) return false
        val ok=native.fillStroke(i)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }

    fun setOnionSkin(enabled:Boolean,before:Int=2,after:Int=2,opacity:Float=0.35f):Boolean {
        val ok = native.setOnionSkin(enabled,before,after,opacity)
        if (ok) { if (enabled != onion.enabled) onion.toggle(); onion.setBefore(before); onion.setAfter(after); onion.setOpacity(opacity); render() }
        return ok
    }
    fun saveDocumentJson():String? {
        if (native.handle == 0L) return null
        val originalLayer = selectedLayer
        val originalFrame = animation.currentFrame
        val json = ProjectDocumentCodec.encode(
            NativeDocumentAdapter(native),
            document.canvasWidth, document.canvasHeight, animation.fps, originalFrame
        )
        if (native.layerCount() > 0) {
            native.selectLayer(originalLayer.coerceIn(0, native.layerCount() - 1))
            native.selectFrameOrHold(originalFrame)
        }
        render()
        return json
    }

    fun loadDocumentJson(raw:String):Boolean {
        if (native.handle == 0L) return false
        val parsed = ProjectDocumentCodec.parse(raw) ?: return false
        if (!GPNative.nativeResetDocumentEgl(rendererHandle)) return false
        document.canvasWidth = (parsed.width ?: document.canvasWidth).coerceAtLeast(1)
        document.canvasHeight = (parsed.height ?: document.canvasHeight).coerceAtLeast(1)
        animation.setFps((parsed.fps ?: animation.fps).coerceIn(1,120))
        if (!ProjectDocumentCodec.restore(parsed, NativeDocumentAdapter(native), brushes.size)) return false
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
        val s = material.stroke
        fun channel(v:Float) = (v.coerceIn(0f,1f) * 255f + 0.5f).toInt()
        materials.setColor((channel(s[3]) shl 24) or (channel(s[0]) shl 16) or (channel(s[1]) shl 8) or channel(s[2]))
        materials.setFillEnabled(material.fillEnabled)
        pushMaterialColor()
    }

    /** Layer state as the native document holds it; null when there is no such layer. */
    fun layerState(index:Int = selectedLayer):LayerRecord? = NativeDocumentAdapter(native).layerRecord(index)

    fun setMultiframeEditing(enabled:Boolean):Boolean {
        val ok=native.setMultiframeEditing(enabled)
        if(ok){ multiframeEditing=enabled; render() }
        return ok
    }
    fun fillAt(x: Float, y: Float): Boolean {
        if (rendererHandle == 0L) return false
        // Blender Legacy GP Fill creates a closed filled stroke using the active material.
        // Enable the material's Fill component only when the Fill tool is actually used.
        if (!materials.fillEnabled) {
            setMaterialFillEnabled(true)
        }
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
            GPNative.nativeRenderEgl(rendererHandle)
        }
    }
    fun layerCount() = native.layerCount()
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
        val changed = runSelectCommand(ProjectGreaseSelect.pick(x, y, radiusSquared, flags, selectMode))
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
            selection.clear()
            animation.initialize()
            document.markDirty()
            render()
        }
        return ok
    }
}
