package com.smitnk.projectgrease.editor

import com.smitnk.projectgrease.nativebridge.GPNative

class NativeEditorBridge {
    var handle: Long = 0L
        private set
    fun attach(value: Long) { handle = value }
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
    fun selectFrameOrHold(frame:Int) = handle != 0L && GPNative.nativeSelectFrameOrHold(handle,frame)
    fun render() = handle != 0L && GPNative.nativeRender(handle)
    fun duplicateFrame(sourceFrame:Int,targetFrame:Int)=handle != 0L && GPNative.nativeDuplicateFrame(handle,sourceFrame,targetFrame)
    fun deleteFrame(frameNumber:Int)=handle != 0L && GPNative.nativeDeleteFrame(handle,frameNumber)
    fun createFrame(frame: Int) = handle != 0L && GPNative.nativeCreateFrame(handle, frame)
    fun selectFrame(frame: Int) = handle != 0L && GPNative.nativeSelectFrame(handle, frame)
    fun strokeCount() = if (handle != 0L) GPNative.nativeStrokeCount(handle) else 0
    fun pointCount() = if (handle != 0L) GPNative.nativePointCount(handle) else 0
    fun selectStroke(index: Int) = handle != 0L && GPNative.nativeSelectStroke(handle, index)
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
    fun scaleStroke(index: Int, scaleX: Float, scaleY: Float) = handle != 0L && GPNative.nativeScaleStroke(handle, index, scaleX, scaleY)
    fun mirrorStroke(index: Int, mirrorX: Boolean, mirrorY: Boolean) = handle != 0L && GPNative.nativeMirrorStroke(handle, index, mirrorX, mirrorY)
    fun subdivideStroke(index: Int, level: Int) = handle != 0L && GPNative.nativeSubdivideStroke(handle, index, level)
    fun closeStroke(index: Int) = handle != 0L && GPNative.nativeCloseStroke(handle, index)
    fun trimStroke(index: Int, from: Int, to: Int, keepSinglePoint: Boolean) = handle != 0L && GPNative.nativeTrimStroke(handle, index, from, to, keepSinglePoint)
    fun splitStroke(index: Int, beforeIndex: Int) = handle != 0L && GPNative.nativeSplitStroke(handle, index, beforeIndex)
    fun getPoint(stroke: Int, point: Int) = if (handle != 0L) GPNative.nativeGetPoint(handle, stroke, point) else null
    fun fillStroke(index: Int) = handle != 0L && GPNative.nativeFillStroke(handle, index)
    fun materialCount() = if (handle != 0L) GPNative.nativeMaterialCount(handle) else 0
    fun createMaterial() = handle != 0L && GPNative.nativeCreateMaterial(handle)
    fun setMaterialColors(index:Int, stroke:FloatArray, fill:FloatArray) = handle != 0L && GPNative.nativeSetMaterialColors(handle,index,stroke,fill)
    fun setMaterialVisibility(index:Int, visible:Boolean) = handle != 0L && GPNative.nativeSetMaterialVisibility(handle,index,visible)
    fun setMaterialFillEnabled(index:Int, enabled:Boolean) = handle != 0L && GPNative.nativeSetMaterialFillEnabled(handle,index,enabled)
    fun smoothStroke(index:Int,influence:Float=0.5f,iterations:Int=2) = handle != 0L && GPNative.nativeSmoothStroke(handle,index,influence,iterations)
    fun setOnionSkin(enabled:Boolean,before:Int,after:Int,opacity:Float) = handle != 0L && GPNative.nativeSetOnionSkin(handle,enabled,before,after,opacity)
    fun setMultiframeEditing(enabled:Boolean) = handle != 0L && GPNative.nativeSetMultiframeEditing(handle,enabled)
}

class HistoryController {
    var canUndo = false; private set
    var canRedo = false; private set
    fun markEdit() { canUndo = true; canRedo = false }
    fun undo() { if (canUndo) canRedo = true }
    fun redo() { if (canRedo) canUndo = true }
}

class DocumentController {
    var projectName = "Project Grease"
    var dirty = false; private set
    fun markDirty() { dirty = true }
    fun markSaved() { dirty = false }
}

class AnimationController(private val native: NativeEditorBridge) {
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
                native.render()
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
    fun deleteFrame(frameNumber:Int):Boolean {
        if (native.handle == 0L) return false
        if (!native.deleteFrame(frameNumber)) return false
        currentFrame=native.frameEnd().let { if(it>0) minOf(currentFrame,it) else 1 }
        frameCount=native.frameCount().coerceAtLeast(1); timelineEnd=native.frameEnd().coerceAtLeast(1); return true
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
    var fillEnabled=true; private set
    var activeMaterial=0; private set
    var thickness=8f; private set
    var opacity=1f; private set
    var colorArgb:Int=0xFFFFFFFF.toInt(); private set
    fun select(index:Int){activeMaterial=index.coerceAtLeast(0)}
    fun setColor(value:Int){colorArgb=value}
    fun setFillEnabled(value:Boolean){fillEnabled=value}
    fun setThickness(value:Float){thickness=value.coerceIn(0.5f,100f)}
    fun setOpacity(value:Float){opacity=value.coerceIn(0f,1f)}
}

class ViewController {
    var zoom=1f; private set
    var showGrid=false; private set
    fun zoomBy(delta:Float){zoom=(zoom+delta).coerceIn(0.1f,8f)}
    fun reset(){zoom=1f}
    fun toggleGrid(){showGrid=!showGrid}
}

class SelectionController(private val native: NativeEditorBridge) {
    var hasSelection=false; private set
    var selectedStroke=-1; private set
    fun clear(){hasSelection=false; selectedStroke=-1}
    fun selectStroke(index:Int):Boolean {
        val ok=native.selectStroke(index)
        if(ok){selectedStroke=index;hasSelection=true}
        return ok
    }
}

class ModifierController { val modifiers=mutableListOf<String>(); fun add(name:String){modifiers+=name}; fun removeAt(index:Int){if(index in modifiers.indices)modifiers.removeAt(index)} }
class SculptController { fun isAvailable()=FeatureRegistry.capability(FeatureId.SCULPT).state==FeatureState.AVAILABLE }

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

enum class GreaseTool { DRAW, ERASE, SELECT, LASSO, FILL, EYEDROPPER, LINE, RECTANGLE, CIRCLE, ARC, POLYLINE, PAN, SCULPT }

class ToolController {
    var activeTool=GreaseTool.DRAW; private set
    fun select(tool:GreaseTool):Boolean {
        val feature=when(tool){
            GreaseTool.DRAW->FeatureId.FREEHAND; GreaseTool.ERASE->FeatureId.ERASER
            GreaseTool.SELECT->FeatureId.SELECT; GreaseTool.LASSO->FeatureId.LASSO
            GreaseTool.FILL->FeatureId.FILL; GreaseTool.EYEDROPPER->FeatureId.STROKE_COLOR
            GreaseTool.LINE->FeatureId.LINE; GreaseTool.RECTANGLE->FeatureId.RECTANGLE
            GreaseTool.CIRCLE->FeatureId.CIRCLE; GreaseTool.ARC->FeatureId.ARC
            GreaseTool.POLYLINE->FeatureId.POLYLINE; GreaseTool.PAN->FeatureId.PAN
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
    val history=HistoryController()
    val animation=AnimationController(native)
    val materials=MaterialController()
    val view=ViewController()
    val selection=SelectionController(native)
    val modifiers=ModifierController()
    val sculpt=SculptController()
    val onion=OnionSkinController()
    private var rendererHandle=0L
    fun attachRenderer(handle:Long) {
        rendererHandle = handle
        native.attach(handle)
        pushMaterialColor()
        animation.initialize()
        selectedLayer = 0
    }
    fun detachRenderer(){animation.stop();rendererHandle=0L;native.detach()}
    fun selectTool(tool:GreaseTool)=tools.select(tool)
    private data class PendingPoint(val x:Float,val y:Float,val pressure:Float,val time:Float)
    private val pendingShapePoints = mutableListOf<PendingPoint>()
    private val pendingLassoPoints = mutableListOf<Pair<Float,Float>>()
    private var pendingShapeTool: GreaseTool? = null

    fun beginStroke():Boolean {
        if (rendererHandle == 0L) return false
        return when (tools.activeTool) {
            GreaseTool.DRAW -> {
                pendingShapePoints.clear()
                pendingShapeTool = null
                GPNative.nativeBeginStrokeEglRenderer(rendererHandle, materials.activeMaterial, materials.thickness)
            }
            GreaseTool.LASSO -> { pendingLassoPoints.clear(); true }
            GreaseTool.LINE, GreaseTool.RECTANGLE, GreaseTool.CIRCLE, GreaseTool.ARC, GreaseTool.POLYLINE -> {
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
            pendingLassoPoints += x to y
        } else if (tools.activeTool == GreaseTool.DRAW) {
            GPNative.nativeAddPointEglRenderer(
                rendererHandle, x, y, 0f,
                pressure.coerceAtLeast(0.01f),
                materials.opacity, timeSeconds
            )
        } else if (pendingShapeTool != null) {
            pendingShapePoints += PendingPoint(x, y, pressure.coerceAtLeast(0.01f), timeSeconds)
            val preview = generatedShapePoints()
            if (preview.isNotEmpty()) {
                val packed = FloatArray(preview.size * 3)
                preview.forEachIndexed { index, point ->
                    packed[index * 3] = point.x
                    packed[index * 3 + 1] = point.y
                    packed[index * 3 + 2] = point.pressure
                }
                GPNative.nativeSetPreviewStrokeEglRenderer(rendererHandle, packed, materials.thickness)
            }
        }
    }

    private fun generatedShapePoints(): List<PendingPoint> {
        val p = pendingShapePoints
        if (p.isEmpty()) return emptyList()
        val first = p.first()
        val last = p.last()
        val tool = pendingShapeTool ?: return emptyList()
        if (tool == GreaseTool.POLYLINE) return p

        val type = when (tool) {
            GreaseTool.LINE -> 0
            GreaseTool.RECTANGLE -> 1
            GreaseTool.CIRCLE -> 2
            GreaseTool.ARC -> 3
            else -> return emptyList()
        }

        // Preview geometry comes from the same native Blender-3.6.23-derived
        // primitive generator used by final stroke creation. Kotlin no longer
        // reimplements line/rectangle/circle/arc geometry.
        val packed = GPNative.nativeGeneratePrimitivePreview(
            type,
            first.x, first.y,
            last.x, last.y,
            0f, 6.2831855f,
            64
        ) ?: return emptyList()

        if (packed.size < 3 || packed.size % 3 != 0) return emptyList()
        return (0 until packed.size / 3).map { i ->
            PendingPoint(
                packed[i * 3],
                packed[i * 3 + 1],
                packed[i * 3 + 2],
                last.time
            )
        }
    }

    private fun shapeParameters(): FloatArray {
        if (pendingShapePoints.isEmpty()) return FloatArray(0)
        val first = pendingShapePoints.first()
        val last = pendingShapePoints.last()
        return floatArrayOf(first.x, first.y, last.x, last.y)
    }

    fun endStroke(){
        if (rendererHandle == 0L) return
        if (tools.activeTool == GreaseTool.LASSO) {
            if (pendingLassoPoints.size >= 3) {
                val packed = FloatArray(pendingLassoPoints.size * 2)
                pendingLassoPoints.forEachIndexed { i, p ->
                    packed[i * 2] = p.first
                    packed[i * 2 + 1] = p.second
                }
                val selected = GPNative.nativeLassoSelect(rendererHandle, packed, pendingLassoPoints.size, false)
                if (selected > 0) {
                    history.markEdit()
                    document.markDirty()
                }
            }
            pendingLassoPoints.clear()
            return
        }
        if (tools.activeTool == GreaseTool.DRAW) {
            if (GPNative.nativeEndStrokeEglRenderer(rendererHandle)) {
                history.markEdit(); document.markDirty()
            }
            return
        }
        val params = shapeParameters()
        val shapeTool = pendingShapeTool
        // Preserve the captured polyline points before clearing the preview state.
        // Previously pendingShapePoints was cleared before the POLYLINE branch read it,
        // making every polyline finish with zero points.
        val polylinePoints = pendingShapePoints.toList()
        GPNative.nativeClearPreviewStrokeEglRenderer(rendererHandle)
        pendingShapePoints.clear()
        pendingShapeTool = null
        if (shapeTool == null || (shapeTool != GreaseTool.POLYLINE && params.size < 4)) return
        val type = when (shapeTool) {
            GreaseTool.LINE -> 0
            GreaseTool.RECTANGLE -> 1
            GreaseTool.CIRCLE -> 2
            GreaseTool.ARC -> 3
            else -> -1
        }
        if (type >= 0) {
            val ok = GPNative.nativeCreatePrimitive(
                rendererHandle, type,
                params[0], params[1], params[2], params[3],
                0f, 6.2831855f, 64,
                materials.activeMaterial, materials.thickness
            )
            if (ok) {
                history.markEdit(); document.markDirty(); render()
            }
        } else if (shapeTool == GreaseTool.POLYLINE) {
            val points = polylinePoints
            if (points.size >= 2) {
                val packed = FloatArray(points.size * 2)
                points.forEachIndexed { i, p ->
                    packed[i * 2] = p.x
                    packed[i * 2 + 1] = p.y
                }
                if (GPNative.nativeCreatePolyline(rendererHandle, packed, points.size,
                        materials.activeMaterial, materials.thickness, false)) {
                    history.markEdit(); document.markDirty(); render()
                }
            }
        }
    }

    fun cancelStroke(){
        if(rendererHandle!=0L) {
            if (tools.activeTool==GreaseTool.DRAW) {
                GPNative.nativeCancelStrokeEglRenderer(rendererHandle)
            }
            GPNative.nativeClearPreviewStrokeEglRenderer(rendererHandle)
        }
        pendingShapePoints.clear()
        pendingShapeTool=null
        pendingLassoPoints.clear()
    }
    fun selectStrokeInLasso(points:List<Pair<Float,Float>>):Boolean {
        if (rendererHandle == 0L || points.size < 3) return false
        val packed = FloatArray(points.size * 2)
        points.forEachIndexed { i, p ->
            packed[i * 2] = p.first
            packed[i * 2 + 1] = p.second
        }
        val count = GPNative.nativeLassoSelect(rendererHandle, packed, points.size, false)
        if (count > 0) {
            history.markEdit()
            document.markDirty()
            render()
        }
        return count > 0
    }

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
    fun setMultiframeEditing(enabled:Boolean):Boolean {
        val ok=native.setMultiframeEditing(enabled)
        if(ok) render()
        return ok
    }
    fun fillAt(x: Float, y: Float): Boolean {
        if (rendererHandle == 0L) return false
        val ok = GPNative.nativeFillAtEglRenderer(rendererHandle, x.toInt(), y.toInt(), materials.activeMaterial, materials.thickness)
        if (ok) { history.markEdit(); document.markDirty() }
        return ok
    }

    fun smoothSelectedStroke(influence:Float=0.5f,iterations:Int=2):Boolean {
        val i=selection.selectedStroke
        if(i<0) return false
        val ok=native.smoothStroke(i,influence,iterations)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
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
        GPNative.nativeSetStrokeColorEglRenderer(rendererHandle,c[0],c[1],c[2],c[3])
    }
    private fun colorToFloats(argb:Int):FloatArray = floatArrayOf(
        ((argb ushr 16) and 255)/255f,
        ((argb ushr 8) and 255)/255f,
        (argb and 255)/255f,
        ((argb ushr 24) and 255)/255f
    )
    fun render(){if(rendererHandle!=0L)GPNative.nativeRenderEgl(rendererHandle)}
    fun layerCount() = native.layerCount()
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
    fun strokeCount() = native.strokeCount()
    fun selectStroke(index:Int)=selection.selectStroke(index)
    fun moveSelectedStroke(dx:Float,dy:Float):Boolean {
        val i=selection.selectedStroke
        if(i<0) return false
        val ok=native.translateStroke(i,dx,dy,0f)
        if(ok){history.markEdit();document.markDirty();render()}
        return ok
    }
    fun hitTestAndSelectStroke(x:Float, y:Float, radius:Float = 24f):Boolean {
        val index = native.hitTestStroke(x, y, radius)
        return index >= 0 && selection.selectStroke(index)
    }
    fun eraseAt(x:Float, y:Float, radius:Float = 24f):Boolean {
        val ok = native.eraseAt(x, y, radius)
        if (ok) {
            selection.clear()
            history.markEdit()
            document.markDirty()
            render()
        }
        return ok
    }
    fun deleteSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.deleteStroke(i);if(ok){selection.clear();history.markEdit();document.markDirty();render()};return ok}
    fun deleteLastStroke():Boolean{val ok=native.deleteLastStroke();if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun duplicateSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.duplicateStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun translateSelectedStroke(dx:Float,dy:Float):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.translateStroke(i,dx,dy,0f);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun flipSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.flipStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun rotateSelectedStroke(radians:Float):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.rotateStroke(i,radians);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun scaleSelectedStroke(scaleX:Float,scaleY:Float):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.scaleStroke(i,scaleX,scaleY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun mirrorSelectedStroke(mirrorX:Boolean,mirrorY:Boolean):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.mirrorStroke(i,mirrorX,mirrorY);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun subdivideSelectedStroke(level:Int=1):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.subdivideStroke(i,level);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun closeSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.closeStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun trimSelectedStroke(from:Int,to:Int,keepSinglePoint:Boolean=true):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.trimStroke(i,from,to,keepSinglePoint);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun splitSelectedStroke(beforeIndex:Int):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.splitStroke(i,beforeIndex);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun undo()=history.undo()
    fun redo()=history.redo()
}
