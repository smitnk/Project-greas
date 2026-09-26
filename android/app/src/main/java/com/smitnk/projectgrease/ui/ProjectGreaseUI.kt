package com.smitnk.projectgrease.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

enum class GreaseTool { DRAW, ERASE, SELECT, LASSO, FILL, EYEDROPPER, SHAPE, PAN }
enum class ViewportMode { DRAW_2D, VIEW_3D }

data class GreaseUiState(
    val projectName: String = "Project Grease",
    val activeTool: GreaseTool = GreaseTool.DRAW,
    val viewportMode: ViewportMode = ViewportMode.DRAW_2D,
    val frame: Int = 1,
    val fps: Int = 12,
    val zoom: Float = 1f,
    val onionSkin: Boolean = false,
    val playing: Boolean = false,
    val loop: Boolean = true,
    val activeLayer: Int = 0,
    val activeMaterial: Int = 0,
    val strokeWidth: Float = 8f,
    val opacity: Float = 1f,
    val showLeftTools: Boolean = true,
    val showRightPanel: Boolean = true,
    val showTimeline: Boolean = true,
    val showTopBar: Boolean = true
)

@Composable
fun ProjectGreaseEditor(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit,
    blenderViewport: @Composable BoxScope.() -> Unit
) {
    Column(Modifier.fillMaxSize().background(Color(0xFF151515))) {
        if (state.showTopBar) {
            ProjectGreaseTopBar(state, onStateChange)
        }

        Row(Modifier.fillMaxWidth().weight(1f)) {
            if (state.showLeftTools) {
                GreaseToolPanel(state, onStateChange)
            }

            Box(
                Modifier.weight(1f).fillMaxHeight().background(Color(0xFF202020)),
                contentAlignment = Alignment.Center,
                content = blenderViewport
            )

            if (state.showRightPanel) {
                GreasePropertiesPanel(state, onStateChange)
            }
        }

        if (state.showTimeline) {
            ProjectGreaseTimeline(state, onStateChange)
        }
    }
}

@Composable
private fun ProjectGreaseTopBar(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    Row(
        Modifier.fillMaxWidth().height(58.dp).background(Color(0xFF252525)),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            "Project Grease",
            Modifier.padding(horizontal = 14.dp),
            style = MaterialTheme.typography.titleMedium
        )

        Spacer(Modifier.weight(1f))

        IconButton({ onStateChange(state.copy(zoom = (state.zoom - .1f).coerceAtLeast(.1f))) }) {
            Icon(Icons.Default.ZoomOut, "Zoom out")
        }
        Text((state.zoom * 100).toInt().toString() + "%")
        IconButton({ onStateChange(state.copy(zoom = (state.zoom + .1f).coerceAtMost(8f))) }) {
            Icon(Icons.Default.ZoomIn, "Zoom in")
        }

        IconButton({ onStateChange(state.copy(onionSkin = !state.onionSkin)) }) {
            Icon(Icons.Default.Layers, "Onion skin")
        }
        IconButton({}) { Icon(Icons.Default.Undo, "Undo") }
        IconButton({}) { Icon(Icons.Default.Redo, "Redo") }

        IconButton({
            val next = if (state.viewportMode == ViewportMode.DRAW_2D)
                ViewportMode.VIEW_3D else ViewportMode.DRAW_2D
            onStateChange(state.copy(viewportMode = next))
        }) {
            Icon(Icons.Default.ViewInAr, "2D Draw / 3D Viewport")
        }

        IconButton({
            onStateChange(state.copy(showLeftTools = !state.showLeftTools))
        }) { Icon(Icons.Default.MenuOpen, "Hide/show tools") }

        IconButton({
            onStateChange(state.copy(showRightPanel = !state.showRightPanel))
        }) { Icon(Icons.Default.Tune, "Hide/show properties") }

        IconButton({
            onStateChange(state.copy(showTimeline = !state.showTimeline))
        }) { Icon(Icons.Default.ViewTimeline, "Hide/show timeline") }
    }
}

@Composable
private fun GreaseToolPanel(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    Column(
        Modifier.width(86.dp).fillMaxHeight().verticalScroll(rememberScrollState())
            .background(Color(0xFF242424)).padding(vertical = 6.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        ToolButton(state, onStateChange, GreaseTool.DRAW, Icons.Default.Edit, "Draw")
        ToolButton(state, onStateChange, GreaseTool.ERASE, Icons.Default.Clear, "Erase")
        ToolButton(state, onStateChange, GreaseTool.SELECT, Icons.Default.TouchApp, "Select")
        ToolButton(state, onStateChange, GreaseTool.LASSO, Icons.Default.Gesture, "Lasso")
        ToolButton(state, onStateChange, GreaseTool.FILL, Icons.Default.FormatColorFill, "Fill")
        ToolButton(state, onStateChange, GreaseTool.EYEDROPPER, Icons.Default.Colorize, "Eyedropper")
        ToolButton(state, onStateChange, GreaseTool.SHAPE, Icons.Default.Category, "Shape")
        ToolButton(state, onStateChange, GreaseTool.PAN, Icons.Default.PanTool, "Pan")
        HorizontalDivider(Modifier.padding(6.dp))
        IconButton({}) { Icon(Icons.Default.Undo, "Undo") }
        IconButton({}) { Icon(Icons.Default.Redo, "Redo") }
        Text("Tools", style = MaterialTheme.typography.labelSmall)
    }
}

@Composable
private fun ToolButton(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit,
    tool: GreaseTool,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String
) {
    NavigationRailItem(
        selected = state.activeTool == tool,
        onClick = { onStateChange(state.copy(activeTool = tool)) },
        icon = { Icon(icon, label) },
        label = { Text(label) }
    )
}

@Composable
private fun GreasePropertiesPanel(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    Column(
        Modifier.width(270.dp).fillMaxHeight()
            .verticalScroll(rememberScrollState())
            .background(Color(0xFF242424)).padding(12.dp)
    ) {
        Text("Project Grease", style = MaterialTheme.typography.titleMedium)
        HorizontalDivider(Modifier.padding(vertical = 8.dp))

        Text("Grease Pencil", style = MaterialTheme.typography.titleSmall)
        Text("Tool: " + state.activeTool.name)
        Text("Viewport: " + state.viewportMode.name)

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Stroke", style = MaterialTheme.typography.titleSmall)
        Text("Width: " + state.strokeWidth.toInt() + " px")
        Slider(
            value = state.strokeWidth,
            onValueChange = { onStateChange(state.copy(strokeWidth = it)) },
            valueRange = 1f..100f
        )
        Text("Opacity: " + (state.opacity * 100).toInt() + "%")
        Slider(
            value = state.opacity,
            onValueChange = { onStateChange(state.copy(opacity = it)) },
            valueRange = 0f..1f
        )

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Grease Pencil Layers", style = MaterialTheme.typography.titleSmall)
        PropertyButton("GP Layer 1", state.activeLayer == 0) {
            onStateChange(state.copy(activeLayer = 0))
        }
        PropertyButton("GP Layer 2", state.activeLayer == 1) {
            onStateChange(state.copy(activeLayer = 1))
        }
        TextButton({}) { Text("+ Add GP Layer") }

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Grease Pencil Materials", style = MaterialTheme.typography.titleSmall)
        PropertyButton("Grease Pencil Material", state.activeMaterial == 0) {
            onStateChange(state.copy(activeMaterial = 0))
        }
        TextButton({}) { Text("+ Add Material") }

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Blender Viewport", style = MaterialTheme.typography.titleSmall)
        TextButton({}) { Text("Frame Selected") }
        TextButton({}) { Text("Reset View") }
        TextButton({}) { Text("Orthographic / Perspective") }
        TextButton({}) { Text("Viewport Navigation") }

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Animation", style = MaterialTheme.typography.titleSmall)
        Row(verticalAlignment = Alignment.CenterVertically) {
            Checkbox(state.onionSkin, { onStateChange(state.copy(onionSkin = it)) })
            Text("Onion Skin")
        }
    }
}

@Composable
private fun PropertyButton(title: String, selected: Boolean, onClick: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().clickable(onClick = onClick).padding(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        RadioButton(selected, onClick)
        Text(title)
    }
}

@Composable
private fun ProjectGreaseTimeline(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    Column(
        Modifier.fillMaxWidth().height(180.dp).background(Color(0xFF252525))
    ) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Timeline", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.width(12.dp))
            Text("Frame " + state.frame)
            Spacer(Modifier.width(12.dp))
            Text("FPS " + state.fps)
            Spacer(Modifier.weight(1f))

            IconButton({ onStateChange(state.copy(playing = !state.playing)) }) {
                Icon(
                    if (state.playing) Icons.Default.Pause else Icons.Default.PlayArrow,
                    "Play"
                )
            }
            IconButton({ onStateChange(state.copy(loop = !state.loop)) }) {
                Icon(Icons.Default.Loop, "Loop")
            }
            TextButton({}) { Text("+ Frame") }
            TextButton({}) { Text("Insert") }
            TextButton({}) { Text("Duplicate") }
            TextButton({}) { Text("Delete") }
        }

        Row(
            Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(8.dp)
        ) {
            for (frame in 1..60) {
                val selected = frame == state.frame
                Surface(
                    Modifier.width(72.dp).height(70.dp).padding(2.dp).clickable {
                        onStateChange(state.copy(frame = frame))
                    },
                    tonalElevation = if (selected) 4.dp else 0.dp
                ) {
                    Box(contentAlignment = Alignment.Center) { Text(frame.toString()) }
                }
            }
        }
    }
}

@Composable
fun ProjectGreaseNewProject(
    onCreate: (GreaseUiState) -> Unit
) {
    var name by remember { mutableStateOf("Project Grease") }
    var fps by remember { mutableIntStateOf(12) }

    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text("New Project", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(18.dp))
        OutlinedTextField(
            value = name,
            onValueChange = { name = it },
            label = { Text("Project name") }
        )
        Spacer(Modifier.height(12.dp))
        Text("FPS: $fps")
        Slider(
            value = fps.toFloat(),
            onValueChange = { fps = it.toInt().coerceIn(1, 60) },
            valueRange = 1f..60f
        )
        Spacer(Modifier.height(18.dp))
        Button({
            onCreate(
                GreaseUiState(
                    projectName = name.ifBlank { "Project Grease" },
                    fps = fps
                )
            )
        }) {
            Text("Create Project")
        }
    }
}

typealias BlenderViewportSurface = @Composable BoxScope.() -> Unit
