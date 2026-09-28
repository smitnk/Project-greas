
package com.smitnk.projectgrease.editor

import com.smitnk.projectgrease.nativebridge.GPNative

class HistoryController {
    var canUndo = false
        private set
    var canRedo = false
        private set
    fun markEdit() { canUndo = true; canRedo = false }
    fun undo() { if (canUndo) canRedo = true }
    fun redo() { if (canRedo) canUndo = true }
}

class DocumentController {
    var projectName = "Project Grease"
    var dirty = false
        private set
    fun markDirty() { dirty = true }
    fun markSaved() { dirty = false }
}

class AnimationController {
    var currentFrame = 1
        private set
    var fps = 12
        private set
    var playing = false
        private set
    var loop = true
        private set
    fun setFrame(value: Int) { currentFrame = value.coerceAtLeast(1) }
    fun setFps(value: Int) { fps = value.coerceIn(1, 120) }
    fun togglePlayback() { playing = !playing }
    fun toggleLoop() { loop = !loop }
}

class MaterialController {
    var activeMaterial = 0
        private set
    var thickness = 8f
        private set
    var opacity = 1f
        private set
    fun select(index: Int) { activeMaterial = index.coerceAtLeast(0) }
    fun setThickness(value: Float) { thickness = value.coerceIn(0.5f, 100f) }
    fun setOpacity(value: Float) { opacity = value.coerceIn(0f, 1f) }
}

class ViewController {
    var zoom = 1f
        private set
    var showGrid = false
        private set
    fun zoomBy(delta: Float) { zoom = (zoom + delta).coerceIn(0.1f, 8f) }
    fun reset() { zoom = 1f }
    fun toggleGrid() { showGrid = !showGrid }
}

class SelectionController {
    var hasSelection = false
        private set
    fun clear() { hasSelection = false }
    fun markSelected() { hasSelection = true }
}

class ModifierController {
    val modifiers = mutableListOf<String>()
    fun add(name: String) { modifiers += name }
    fun removeAt(index: Int) { if (index in modifiers.indices) modifiers.removeAt(index) }
}

class SculptController {
    fun isAvailable() = FeatureRegistry.capability(FeatureId.SCULPT).state == FeatureState.AVAILABLE
}

class OnionSkinController {
    var enabled = false
        private set
    var beforeFrames = 2
        private set
    var afterFrames = 2
        private set
    var opacity = 0.35f
        private set
    var fade = true
        private set
    var selectedLayerOnly = true
        private set

    fun toggle() { enabled = !enabled }
    fun setBefore(value: Int) { beforeFrames = value.coerceIn(0, 12) }
    fun setAfter(value: Int) { afterFrames = value.coerceIn(0, 12) }
    fun setOpacity(value: Float) { opacity = value.coerceIn(0f, 1f) }
}

enum class GreaseTool {
    DRAW, ERASE, SELECT, LASSO, FILL, EYEDROPPER, LINE, RECTANGLE, CIRCLE, ARC, POLYLINE, PAN, SCULPT
}

class ToolController {
    var activeTool = GreaseTool.DRAW
        private set

    fun select(tool: GreaseTool): Boolean {
        val feature = when (tool) {
            GreaseTool.DRAW -> FeatureId.FREEHAND
            GreaseTool.ERASE -> FeatureId.ERASER
            GreaseTool.SELECT -> FeatureId.SELECT
            GreaseTool.LASSO -> FeatureId.LASSO
            GreaseTool.FILL -> FeatureId.ADVANCED_FILL
            GreaseTool.EYEDROPPER -> FeatureId.STROKE_COLOR
            GreaseTool.LINE -> FeatureId.LINE
            GreaseTool.RECTANGLE -> FeatureId.RECTANGLE
            GreaseTool.CIRCLE -> FeatureId.CIRCLE
            GreaseTool.ARC -> FeatureId.ARC
            GreaseTool.POLYLINE -> FeatureId.POLYLINE
            GreaseTool.PAN -> FeatureId.PAN
            GreaseTool.SCULPT -> FeatureId.SCULPT
        }
        if (FeatureRegistry.capability(feature).state == FeatureState.NOT_IMPLEMENTED) return false
        activeTool = tool
        return true
    }
}

class EditorController {
    val tools = ToolController()
    val document = DocumentController()
    val history = HistoryController()
    val animation = AnimationController()
    val materials = MaterialController()
    val view = ViewController()
    val selection = SelectionController()
    val modifiers = ModifierController()
    val sculpt = SculptController()
    val onion = OnionSkinController()

    private var rendererHandle = 0L

    fun attachRenderer(handle: Long) { rendererHandle = handle }
    fun detachRenderer() { rendererHandle = 0L }
    fun selectTool(tool: GreaseTool): Boolean = tools.select(tool)

    fun beginStroke(): Boolean {
        if (rendererHandle == 0L || tools.activeTool != GreaseTool.DRAW) return false
        return GPNative.nativeBeginStrokeEglRenderer(rendererHandle, materials.activeMaterial, materials.thickness)
    }

    fun addStrokePoint(x: Float, y: Float, pressure: Float, timeSeconds: Float) {
        if (rendererHandle == 0L) return
        GPNative.nativeAddPointEglRenderer(
            rendererHandle, x, y, 0f, pressure.coerceAtLeast(0.01f), 1f, timeSeconds
        )
    }

    fun endStroke() {
        if (rendererHandle == 0L) return
        GPNative.nativeEndStrokeEglRenderer(rendererHandle)
        history.markEdit()
        document.markDirty()
    }

    fun cancelStroke() {
        if (rendererHandle == 0L) return
        GPNative.nativeEndStrokeEglRenderer(rendererHandle)
    }

    fun render() {
        if (rendererHandle != 0L) GPNative.nativeRenderEgl(rendererHandle)
    }

    fun undo() = history.undo()
    fun redo() = history.redo()
}
