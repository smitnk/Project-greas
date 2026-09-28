package com.smitnk.projectgrease.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.FeatureId
import com.smitnk.projectgrease.editor.FeatureRegistry
import com.smitnk.projectgrease.editor.FeatureState
import com.smitnk.projectgrease.editor.GreaseTool

data class GreaseUiState(
    val projectName: String = "Project Grease",
    val frame: Int = 1,
    val showTools: Boolean = true,
    val showProperties: Boolean = false,
    val showTimeline: Boolean = true,
    val canvasFocus: Boolean = false,
    val showProjectMenu: Boolean = false,
    val showToolMenu: Boolean = false
)

@Composable
fun ProjectGreaseEditor(
    controller: EditorController,
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit,
    blenderViewport: @Composable BoxScope.() -> Unit
) {
    Column(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
        if (!state.canvasFocus) ProjectGreaseTopBar(controller, state, onStateChange)
        Row(Modifier.fillMaxWidth().weight(1f)) {
            if (!state.canvasFocus && state.showTools) GreaseToolRail(controller)
            Box(
                Modifier.weight(1f).fillMaxHeight().background(MaterialTheme.colorScheme.surfaceVariant),
                contentAlignment = Alignment.Center,
                content = blenderViewport
            )
            if (!state.canvasFocus && state.showProperties) {
                GreasePropertiesPanel(controller, state, onStateChange)
            }
        }
        if (!state.canvasFocus && state.showTimeline) ProjectGreaseTimeline(controller, state, onStateChange)
        if (state.canvasFocus) {
            IconButton(
                onClick = { onStateChange(state.copy(canvasFocus = false)) },
                modifier = Modifier.padding(8.dp)
            ) { Icon(Icons.Default.CloseFullscreen, "Exit canvas focus") }
        }
    }
}

@Composable
private fun ProjectGreaseTopBar(
    controller: EditorController,
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    Surface(tonalElevation = 2.dp) {
        Row(
            Modifier.fillMaxWidth().height(58.dp).padding(horizontal = 4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            IconButton({ onStateChange(state.copy(showProjectMenu = true)) }) {
                Icon(Icons.Default.Menu, "Project")
            }
            Text(state.projectName, style = MaterialTheme.typography.titleMedium, maxLines = 1)
            Spacer(Modifier.weight(1f))
            IconButton(enabled = controller.history.canUndo, onClick = { controller.undo(); onStateChange(state) }) {
                Icon(Icons.Default.Undo, "Undo")
            }
            IconButton(enabled = controller.history.canRedo, onClick = { controller.redo(); onStateChange(state) }) {
                Icon(Icons.Default.Redo, "Redo")
            }
            IconButton({ controller.view.zoomBy(-0.1f); onStateChange(state) }) {
                Icon(Icons.Default.ZoomOut, "Zoom out")
            }
            Text((controller.view.zoom * 100).toInt().toString() + "%")
            IconButton({ controller.view.zoomBy(0.1f); onStateChange(state) }) {
                Icon(Icons.Default.ZoomIn, "Zoom in")
            }
            IconButton({ onStateChange(state.copy(showTools = !state.showTools)) }) {
                Icon(Icons.Default.MenuOpen, "Tools")
            }
            IconButton({ onStateChange(state.copy(showProperties = !state.showProperties)) }) {
                Icon(Icons.Default.Tune, "Properties")
            }
            IconButton({ onStateChange(state.copy(showTimeline = !state.showTimeline)) }) {
                Icon(Icons.Default.ViewTimeline, "Timeline")
            }
            IconButton({ onStateChange(state.copy(canvasFocus = true)) }) {
                Icon(Icons.Default.Fullscreen, "Canvas focus")
            }
            IconButton({ onStateChange(state.copy(showToolMenu = true)) }) {
                Icon(Icons.Default.MoreVert, "More")
            }
        }
    }

    if (state.showProjectMenu) {
        ModalBottomSheet(onDismissRequest = { onStateChange(state.copy(showProjectMenu = false)) }) {
            Text("Project", style = MaterialTheme.typography.titleLarge, modifier = Modifier.padding(20.dp))
            listOf(
                FeatureId.NEW_PROJECT to "New project",
                FeatureId.OPEN_PROJECT to "Open project",
                FeatureId.SAVE to "Save",
                FeatureId.SAVE_AS to "Save as",
                FeatureId.EXPORT to "Export",
                FeatureId.PROJECT_SETTINGS to "Project settings"
            ).forEach { pair ->
                CapabilityRow(pair.first)
                ListItem(
                    headlineContent = { Text(pair.second) },
                    modifier = Modifier.clickable { onStateChange(state.copy(showProjectMenu = false)) }
                )
            }
            Spacer(Modifier.height(24.dp))
        }
    }

    if (state.showToolMenu) {
        ModalBottomSheet(onDismissRequest = { onStateChange(state.copy(showToolMenu = false)) }) {
            Text("Tool settings", style = MaterialTheme.typography.titleLarge, modifier = Modifier.padding(20.dp))
            Text("Current: " + controller.tools.activeTool.name, modifier = Modifier.padding(horizontal = 20.dp))
            CapabilityRow(FeatureId.SMOOTHING)
            CapabilityRow(FeatureId.STABILIZATION)
            CapabilityRow(FeatureId.SPACING)
            CapabilityRow(FeatureId.PRESSURE_CURVE)
            Spacer(Modifier.height(24.dp))
        }
    }
}

private data class ToolEntry(val tool: GreaseTool, val icon: ImageVector, val label: String, val feature: FeatureId)

@Composable
private fun GreaseToolRail(controller: EditorController) {
    val entries = listOf(
        ToolEntry(GreaseTool.DRAW, Icons.Default.Edit, "Draw", FeatureId.FREEHAND),
        ToolEntry(GreaseTool.ERASE, Icons.Default.Clear, "Erase", FeatureId.ERASER),
        ToolEntry(GreaseTool.SELECT, Icons.Default.TouchApp, "Select", FeatureId.SELECT),
        ToolEntry(GreaseTool.LASSO, Icons.Default.Gesture, "Lasso", FeatureId.LASSO),
        ToolEntry(GreaseTool.FILL, Icons.Default.FormatColorFill, "Fill", FeatureId.ADVANCED_FILL),
        ToolEntry(GreaseTool.EYEDROPPER, Icons.Default.Colorize, "Pick", FeatureId.STROKE_COLOR),
        ToolEntry(GreaseTool.LINE, Icons.Default.Remove, "Line", FeatureId.LINE),
        ToolEntry(GreaseTool.RECTANGLE, Icons.Default.CropSquare, "Rect", FeatureId.RECTANGLE),
        ToolEntry(GreaseTool.CIRCLE, Icons.Default.Circle, "Circle", FeatureId.CIRCLE),
        ToolEntry(GreaseTool.ARC, Icons.Default.Timeline, "Arc", FeatureId.ARC),
        ToolEntry(GreaseTool.POLYLINE, Icons.Default.Timeline, "Polyline", FeatureId.POLYLINE),
        ToolEntry(GreaseTool.PAN, Icons.Default.PanTool, "Pan", FeatureId.PAN),
        ToolEntry(GreaseTool.SCULPT, Icons.Default.AutoFixHigh, "Sculpt", FeatureId.SCULPT)
    )
    Surface(Modifier.width(72.dp).fillMaxHeight(), tonalElevation = 1.dp) {
        Column(
            Modifier.fillMaxHeight().verticalScroll(rememberScrollState()).padding(vertical = 5.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            entries.forEach { entry ->
                val capability = FeatureRegistry.capability(entry.feature)
                NavigationRailItem(
                    selected = controller.tools.activeTool == entry.tool,
                    enabled = capability.state != FeatureState.NOT_IMPLEMENTED,
                    onClick = { controller.selectTool(entry.tool) },
                    icon = { Icon(entry.icon, entry.label) },
                    label = { Text(entry.label, maxLines = 1) }
                )
            }
        }
    }
}

@Composable
private fun GreasePropertiesPanel(
    controller: EditorController,
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    Surface(Modifier.widthIn(min = 230.dp, max = 300.dp).fillMaxHeight(), tonalElevation = 2.dp) {
        Column(Modifier.fillMaxHeight().verticalScroll(rememberScrollState()).padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Properties", style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.weight(1f))
                IconButton({ onStateChange(state.copy(showProperties = false)) }) {
                    Icon(Icons.Default.Close, "Close")
                }
            }
            Text("Brush", style = MaterialTheme.typography.titleSmall)
            Text("Thickness " + controller.materials.thickness.toInt() + " px")
            Slider(
                value = controller.materials.thickness,
                onValueChange = { controller.materials.setThickness(it); onStateChange(state) },
                valueRange = 1f..100f
            )
            Text("Opacity " + (controller.materials.opacity * 100).toInt() + "%")
            Slider(
                value = controller.materials.opacity,
                onValueChange = { controller.materials.setOpacity(it); onStateChange(state) },
                valueRange = 0f..1f
            )
            HorizontalDivider(Modifier.padding(vertical = 10.dp))
            Text("Onion Skin", style = MaterialTheme.typography.titleSmall)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Switch(
                    checked = controller.onion.enabled,
                    onCheckedChange = { controller.onion.toggle(); onStateChange(state) }
                )
                Text("Enabled")
            }
            if (controller.onion.enabled) {
                Text("Before " + controller.onion.beforeFrames + "  •  After " + controller.onion.afterFrames)
                Slider(
                    value = controller.onion.beforeFrames.toFloat(),
                    onValueChange = { controller.onion.setBefore(it.toInt()); onStateChange(state) },
                    valueRange = 0f..12f
                )
                Slider(
                    value = controller.onion.afterFrames.toFloat(),
                    onValueChange = { controller.onion.setAfter(it.toInt()); onStateChange(state) },
                    valueRange = 0f..12f
                )
                Slider(
                    value = controller.onion.opacity,
                    onValueChange = { controller.onion.setOpacity(it); onStateChange(state) },
                    valueRange = 0f..1f
                )
            }
            HorizontalDivider(Modifier.padding(vertical = 10.dp))
            Text("Layers", style = MaterialTheme.typography.titleSmall)
            LayerRow("Character", true, false)
            LayerRow("Ink", true, false)
            LayerRow("Color", true, true)
            TextButton({}) { Text("+ Add layer") }
            HorizontalDivider(Modifier.padding(vertical = 10.dp))
            Text("Capability status", style = MaterialTheme.typography.titleSmall)
            CapabilityRow(FeatureId.STABILIZATION)
            CapabilityRow(FeatureId.ADVANCED_FILL)
            CapabilityRow(FeatureId.STROKE_TEXTURES)
            CapabilityRow(FeatureId.MODIFIERS)
            CapabilityRow(FeatureId.SCULPT)
        }
    }
}

@Composable
private fun LayerRow(name: String, visible: Boolean, locked: Boolean) {
    Row(Modifier.fillMaxWidth().padding(vertical = 5.dp), verticalAlignment = Alignment.CenterVertically) {
        Icon(if (visible) Icons.Default.Visibility else Icons.Default.VisibilityOff, "Visibility")
        Spacer(Modifier.width(6.dp))
        Text(name, Modifier.weight(1f))
        Icon(if (locked) Icons.Default.Lock else Icons.Default.LockOpen, "Lock")
    }
}

@Composable
private fun CapabilityRow(id: FeatureId) {
    val c = FeatureRegistry.capability(id)
    Row(Modifier.fillMaxWidth().padding(vertical = 3.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(c.label, Modifier.weight(1f))
        AssistChip(onClick = {}, label = { Text(c.state.name.replace('_', ' ')) })
    }
}

@Composable
private fun ProjectGreaseTimeline(
    controller: EditorController,
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    Surface(tonalElevation = 3.dp) {
        Column(Modifier.fillMaxWidth().heightIn(min = 126.dp, max = 205.dp)) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 5.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton({
                    controller.animation.setFrame(controller.animation.currentFrame - 1)
                    onStateChange(state.copy(frame = controller.animation.currentFrame))
                }) { Icon(Icons.Default.SkipPrevious, "Previous frame") }
                IconButton({
                    controller.animation.togglePlayback()
                    onStateChange(state)
                }) {
                    Icon(
                        if (controller.animation.playing) Icons.Default.Pause else Icons.Default.PlayArrow,
                        "Play"
                    )
                }
                IconButton({
                    controller.animation.setFrame(controller.animation.currentFrame + 1)
                    onStateChange(state.copy(frame = controller.animation.currentFrame))
                }) { Icon(Icons.Default.SkipNext, "Next frame") }
                Text("Frame " + controller.animation.currentFrame)
                Spacer(Modifier.width(10.dp))
                Text(controller.animation.fps.toString() + " FPS")
                Spacer(Modifier.weight(1f))
                FilterChip(
                    selected = controller.animation.loop,
                    onClick = { controller.animation.toggleLoop(); onStateChange(state) },
                    label = { Text("Loop") }
                )
                TextButton({}) { Text("+ Frame") }
            }
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(6.dp)) {
                for (frame in 1..60) {
                    val selected = frame == controller.animation.currentFrame
                    Surface(
                        modifier = Modifier.width(58.dp).height(58.dp).padding(2.dp).clickable {
                            controller.animation.setFrame(frame)
                            onStateChange(state.copy(frame = frame))
                        },
                        shape = RoundedCornerShape(8.dp),
                        tonalElevation = if (selected) 4.dp else 0.dp
                    ) {
                        Box(contentAlignment = Alignment.Center) { Text(frame.toString()) }
                    }
                }
            }
        }
    }
}
