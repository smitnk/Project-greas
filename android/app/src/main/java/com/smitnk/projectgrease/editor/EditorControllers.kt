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
    fun duplicateFrame(sourceFrame:Int,targetFrame:Int)=handle != 0L && GPNative.nativeDuplicateFrame(handle,sourceFrame,targetFrame)
    fun deleteFrame(frameNumber:Int)=handle != 0L && GPNative.nativeDeleteFrame(handle,frameNumber)
    fun createFrame(frame: Int) = handle != 0L && GPNative.nativeCreateFrame(handle, frame)
    fun selectFrame(frame: Int) = handle != 0L && GPNative.nativeSelectFrame(handle, frame)
    fun strokeCount() = if (handle != 0L) GPNative.nativeStrokeCount(handle) else 0
    fun pointCount() = if (handle != 0L) GPNative.nativePointCount(handle) else 0
    fun selectStroke(index: Int) = handle != 0L && GPNative.nativeSelectStroke(handle, index)
    fun hitTestStroke(x: Float, y: Float, radius: Float) =
        if (handle != 0L) GPNative.nativeHitTestStroke(handle, x, y, radius) else -1
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
    fun initialize() { if (native.handle != 0L) { native.selectFrame(1); frameCount = native.frameCount().coerceAtLeast(1) } }
    fun setFrame(value: Int): Boolean {
        val target = value.coerceAtLeast(1)
        if (native.handle == 0L) return false
        if (!native.selectFrame(target)) return false
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        return true
    }
    fun ensureFrame(frameNumber: Int): Boolean {
        val target = frameNumber.coerceAtLeast(1)
        if (native.handle == 0L) return false
        if (native.selectFrame(target)) {
            currentFrame = target
            frameCount = native.frameCount().coerceAtLeast(1)
            return true
        }
        if (!native.createFrame(target)) return false
        currentFrame = target
        frameCount = native.frameCount().coerceAtLeast(1)
        return true
    }
    fun duplicateFrame(sourceFrame:Int,targetFrame:Int):Boolean {
        if (native.handle == 0L || targetFrame < 1) return false
        if (!native.duplicateFrame(sourceFrame,targetFrame)) return false
        currentFrame=targetFrame; frameCount=native.frameCount().coerceAtLeast(1); return true
    }
    fun deleteFrame(frameNumber:Int):Boolean {
        if (native.handle == 0L) return false
        if (!native.deleteFrame(frameNumber)) return false
        currentFrame=native.frameCount().let { if(it>0) minOf(currentFrame,it) else 1 }
        frameCount=native.frameCount().coerceAtLeast(1); return true
    }

    fun setFps(value:Int){fps=value.coerceIn(1,120)}
    fun togglePlayback(){playing=!playing}
    fun toggleLoop(){loop=!loop}
}

class MaterialController {
    var activeMaterial=0; private set
    var thickness=8f; private set
    var opacity=1f; private set
    var colorArgb:Int=0xFFFFFFFF.toInt(); private set
    fun select(index:Int){activeMaterial=index.coerceAtLeast(0)}
    fun setColor(value:Int){colorArgb=value}
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
            GreaseTool.FILL->FeatureId.ADVANCED_FILL; GreaseTool.EYEDROPPER->FeatureId.STROKE_COLOR
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
    fun detachRenderer(){rendererHandle=0L;native.detach()}
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
        return when (pendingShapeTool) {
            GreaseTool.LINE -> listOf(first, last)
            GreaseTool.POLYLINE -> p.distinctBy { (it.x * 10f).toInt() to (it.y * 10f).toInt() }
            GreaseTool.RECTANGLE -> {
                val left=minOf(first.x,last.x); val right=maxOf(first.x,last.x)
                val top=minOf(first.y,last.y); val bottom=maxOf(first.y,last.y)
                listOf(
                    PendingPoint(left,top,first.pressure,first.time),
                    PendingPoint(right,top,last.pressure,last.time),
                    PendingPoint(right,bottom,last.pressure,last.time),
                    PendingPoint(left,bottom,first.pressure,first.time),
                    PendingPoint(left,top,first.pressure,first.time)
                )
            }
            GreaseTool.CIRCLE -> {
                val cx=(first.x+last.x)*0.5f; val cy=(first.y+last.y)*0.5f
                val rx=maxOf(1f,kotlin.math.abs(last.x-first.x)*0.5f)
                val ry=maxOf(1f,kotlin.math.abs(last.y-first.y)*0.5f)
                (0..48).map { i ->
                    val a=(2.0*Math.PI*i/48.0).toFloat()
                    PendingPoint(cx+rx*kotlin.math.cos(a.toDouble()).toFloat(),cy+ry*kotlin.math.sin(a.toDouble()).toFloat(),last.pressure,last.time)
                }
            }
            GreaseTool.ARC -> {
                val cx=(first.x+last.x)*0.5
                val cy=(first.y+last.y)*0.5
                val rx=maxOf(1.0,kotlin.math.abs(last.x-first.x)*0.5)
                val ry=maxOf(1.0,kotlin.math.abs(last.y-first.y)*0.5)
                val start=kotlin.math.atan2((first.y-cy)/ry,(first.x-cx)/rx)
                val end=kotlin.math.atan2((last.y-cy)/ry,(last.x-cx)/rx)
                var sweep=end-start
                if (sweep <= 0.0) sweep += 2.0*Math.PI
                (0..32).map { i ->
                    val a=start+sweep*i/32.0
                    PendingPoint(
                        (cx+rx*kotlin.math.cos(a)).toFloat(),
                        (cy+ry*kotlin.math.sin(a)).toFloat(),
                        last.pressure,
                        last.time
                    )
                }
            }
            else -> emptyList()
        }
    }

    fun endStroke(){
        if (rendererHandle == 0L) return
        if (tools.activeTool == GreaseTool.LASSO) {
            selectStrokeInLasso(pendingLassoPoints)
            pendingLassoPoints.clear()
            return
        }
        if (tools.activeTool == GreaseTool.DRAW) {
            if (GPNative.nativeEndStrokeEglRenderer(rendererHandle)) {
                history.markEdit(); document.markDirty()
            }
            return
        }
        val shape = generatedShapePoints()
        val shapeTool = pendingShapeTool
        GPNative.nativeClearPreviewStrokeEglRenderer(rendererHandle)
        pendingShapePoints.clear()
        pendingShapeTool = null
        if (shapeTool == null || shape.size < 2) return
        if (!GPNative.nativeBeginStrokeEglRenderer(rendererHandle, materials.activeMaterial, materials.thickness)) return
        var ok = true
        for (point in shape) {
            ok = ok && GPNative.nativeAddPointEglRenderer(
                rendererHandle, point.x, point.y, 0f,
                point.pressure, materials.opacity, point.time
            )
        }
        if (ok && GPNative.nativeEndStrokeEglRenderer(rendererHandle)) {
            history.markEdit(); document.markDirty()
        } else {
            GPNative.nativeEndStrokeEglRenderer(rendererHandle)
        }
    }

    fun cancelStroke(){
        if(rendererHandle!=0L) {
            if (tools.activeTool==GreaseTool.DRAW) {
                GPNative.nativeEndStrokeEglRenderer(rendererHandle)
            }
            GPNative.nativeClearPreviewStrokeEglRenderer(rendererHandle)
        }
        pendingShapePoints.clear()
        pendingShapeTool=null
        pendingLassoPoints.clear()
    }
    fun selectStrokeInLasso(points:List<Pair<Float,Float>>):Boolean {
        if(points.size<3) return false
        fun inside(x:Float,y:Float):Boolean {
            var hit=false
            var j=points.lastIndex
            for(i in points.indices){
                val xi=points[i].first; val yi=points[i].second
                val xj=points[j].first; val yj=points[j].second
                if(((yi>y)!=(yj>y)) && x < (xj-xi)*(y-yi)/(yj-yi+0.000001f)+xi) hit=!hit
                j=i
            }
            return hit
        }
        for(stroke in 0 until native.strokeCount()){
            for(point in 0 until 10000){
                val p=native.getPoint(stroke,point) ?: break
                if(p.size>=2 && inside(p[0],p[1])) return selection.selectStroke(stroke)
            }
        }
        return false
    }
    fun pushMaterialColor(){
        if(rendererHandle==0L) return
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
        val index = native.hitTestStroke(x, y, radius)
        if (index < 0) return false
        val ok = native.deleteStroke(index)
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
