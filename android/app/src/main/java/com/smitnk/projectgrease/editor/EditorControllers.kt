package com.smitnk.projectgrease.editor

import com.smitnk.projectgrease.nativebridge.GPNative

class NativeEditorBridge {
    var handle: Long = 0L
        private set
    fun attach(value: Long) { handle = value }
    fun detach() { handle = 0L }
    fun layerCount() = if (handle != 0L) GPNative.nativeLayerCount(handle) else 0
    fun createLayer(name: String) = handle != 0L && GPNative.nativeCreateLayer(handle, name)
    fun selectLayer(index: Int) = handle != 0L && GPNative.nativeSelectLayer(handle, index)
    fun frameCount() = if (handle != 0L) GPNative.nativeFrameCount(handle) else 0
    fun createFrame(frame: Int) = handle != 0L && GPNative.nativeCreateFrame(handle, frame)
    fun selectFrame(frame: Int) = handle != 0L && GPNative.nativeSelectFrame(handle, frame)
    fun strokeCount() = if (handle != 0L) GPNative.nativeStrokeCount(handle) else 0
    fun pointCount() = if (handle != 0L) GPNative.nativePointCount(handle) else 0
    fun selectStroke(index: Int) = handle != 0L && GPNative.nativeSelectStroke(handle, index)
    fun deleteStroke(index: Int) = handle != 0L && GPNative.nativeDeleteStroke(handle, index)
    fun deleteLastStroke() = handle != 0L && GPNative.nativeDeleteLastStroke(handle)
    fun duplicateStroke(index: Int) = handle != 0L && GPNative.nativeDuplicateStroke(handle, index)
    fun translateStroke(index: Int, dx: Float, dy: Float, dz: Float) = handle != 0L && GPNative.nativeTranslateStroke(handle, index, dx, dy, dz)
    fun flipStroke(index: Int) = handle != 0L && GPNative.nativeFlipStroke(handle, index)
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
    fun setFps(value:Int){fps=value.coerceIn(1,120)}
    fun togglePlayback(){playing=!playing}
    fun toggleLoop(){loop=!loop}
}

class MaterialController {
    var activeMaterial=0; private set
    var thickness=8f; private set
    var opacity=1f; private set
    fun select(index:Int){activeMaterial=index.coerceAtLeast(0)}
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
        animation.initialize()
        selectedLayer = 0
    }
    fun detachRenderer(){rendererHandle=0L;native.detach()}
    fun selectTool(tool:GreaseTool)=tools.select(tool)
    fun beginStroke():Boolean {
        if(rendererHandle==0L||tools.activeTool!=GreaseTool.DRAW)return false
        return GPNative.nativeBeginStrokeEglRenderer(rendererHandle,materials.activeMaterial,materials.thickness)
    }
    fun addStrokePoint(x:Float,y:Float,pressure:Float,timeSeconds:Float){
        if(rendererHandle!=0L) {
            GPNative.nativeAddPointEglRenderer(
                rendererHandle,
                x,
                y,
                0f,
                pressure.coerceAtLeast(0.01f),
                materials.opacity,
                timeSeconds
            )
        }
    }
    fun endStroke(){if(rendererHandle!=0L&&GPNative.nativeEndStrokeEglRenderer(rendererHandle)){history.markEdit();document.markDirty()}}
    fun cancelStroke(){if(rendererHandle!=0L)GPNative.nativeEndStrokeEglRenderer(rendererHandle)}
    fun render(){if(rendererHandle!=0L)GPNative.nativeRenderEgl(rendererHandle)}
    fun layerCount() = native.layerCount()
    fun createLayer(name:String):Boolean {
        val ok = native.createLayer(name)
        if (ok) {
            selectedLayer = (native.layerCount() - 1).coerceAtLeast(0)
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
    fun selectStroke(index:Int)=selection.selectStroke(index)
    fun deleteSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.deleteStroke(i);if(ok){selection.clear();history.markEdit();document.markDirty();render()};return ok}
    fun deleteLastStroke():Boolean{val ok=native.deleteLastStroke();if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun duplicateSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.duplicateStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun translateSelectedStroke(dx:Float,dy:Float):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.translateStroke(i,dx,dy,0f);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun flipSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.flipStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun subdivideSelectedStroke(level:Int=1):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.subdivideStroke(i,level);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun closeSelectedStroke():Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.closeStroke(i);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun trimSelectedStroke(from:Int,to:Int,keepSinglePoint:Boolean=true):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.trimStroke(i,from,to,keepSinglePoint);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun splitSelectedStroke(beforeIndex:Int):Boolean{val i=selection.selectedStroke;if(i<0)return false;val ok=native.splitStroke(i,beforeIndex);if(ok){history.markEdit();document.markDirty();render()};return ok}
    fun undo()=history.undo()
    fun redo()=history.redo()
}
