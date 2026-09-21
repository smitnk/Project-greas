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
import androidx.compose.runtime.Composable
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
    val opacity: Float = 1f
)

/**
 * Project Grease control UI adapted from the supplied MotionCanvas reference.
 * No Compose Canvas stroke renderer is implemented here.
 * The viewport is supplied by the real Blender Android/GHOST/Draw Manager/GPU path.
 */
@Composable
fun ProjectGreaseApp(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit,
    blenderViewport: @Composable BoxScope.() -> Unit
) {
    Scaffold(topBar = { ProjectGreaseTopBar(state, onStateChange) }) { padding ->
        Column(Modifier.fillMaxSize().padding(padding)) {
            Row(Modifier.fillMaxWidth().weight(1f)) {
                GreaseToolBar(state, onStateChange)
                Box(
                    Modifier.fillMaxHeight().weight(1f).background(Color(0xFF202020)),
                    contentAlignment = Alignment.Center,
                    content = blenderViewport
                )
                GreaseInspector(state, onStateChange)
            }
            ProjectGreaseTimeline(state, onStateChange)
        }
    }
}

@Composable
private fun ProjectGreaseTopBar(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    TopAppBar(
        title = {
            Column {
                Text("Project Grease", style = MaterialTheme.typography.titleMedium)
                Text(state.projectName, style = MaterialTheme.typography.labelSmall)
            }
        },
        navigationIcon = { IconButton({}) { Icon(Icons.Default.Menu, "Menu") } },
        actions = {
            IconButton({
                onStateChange(state.copy(zoom = (state.zoom - .1f).coerceAtLeast(.1f)))
            }) { Icon(Icons.Default.ZoomOut, "Zoom out") }
            Text((state.zoom * 100).toInt().toString() + "%")
            IconButton({
                onStateChange(state.copy(zoom = (state.zoom + .1f).coerceAtMost(8f)))
            }) { Icon(Icons.Default.ZoomIn, "Zoom in") }
            IconButton({ onStateChange(state.copy(onionSkin = !state.onionSkin)) }) {
                Icon(Icons.Default.Layers, "Onion skin")
            }
            IconButton({}) { Icon(Icons.Default.Undo, "Undo") }
            IconButton({}) { Icon(Icons.Default.Redo, "Redo") }
            IconButton({
                val next = if (state.viewportMode == ViewportMode.DRAW_2D)
                    ViewportMode.VIEW_3D else ViewportMode.DRAW_2D
                onStateChange(state.copy(viewportMode = next))
            }) { Icon(Icons.Default.ViewInAr, "Toggle Blender 3D viewport") }
            IconButton({}) { Icon(Icons.Default.Settings, "Settings") }
        }
    )
}

@Composable
private fun GreaseToolBar(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    NavigationRail(Modifier.width(72.dp)) {
        ToolButton(state, onStateChange, GreaseTool.DRAW, Icons.Default.Edit, "Draw")
        ToolButton(state, onStateChange, GreaseTool.ERASE, Icons.Default.Clear, "Erase")
        ToolButton(state, onStateChange, GreaseTool.SELECT, Icons.Default.TouchApp, "Select")
        ToolButton(state, onStateChange, GreaseTool.LASSO, Icons.Default.Gesture, "Lasso")
        ToolButton(state, onStateChange, GreaseTool.FILL, Icons.Default.FormatColorFill, "Fill")
        ToolButton(state, onStateChange, GreaseTool.EYEDROPPER, Icons.Default.Colorize, "Eyedropper")
        ToolButton(state, onStateChange, GreaseTool.SHAPE, Icons.Default.Category, "Shape")
        ToolButton(state, onStateChange, GreaseTool.PAN, Icons.Default.PanTool, "Pan")
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
private fun GreaseInspector(
    state: GreaseUiState,
    onStateChange: (GreaseUiState) -> Unit
) {
    Column(
        Modifier.width(240.dp).fillMaxHeight()
            .verticalScroll(rememberScrollState()).padding(10.dp)
    ) {
        Text("Project Grease", style = MaterialTheme.typography.titleMedium)
        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Grease Pencil", style = MaterialTheme.typography.titleSmall)
        Text("Tool: " + state.activeTool.name)
        Text("Viewport: " + state.viewportMode.name)

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Stroke", style = MaterialTheme.typography.titleSmall)
        Text("Width " + state.strokeWidth.toInt() + " px")
        Slider(
            value = state.strokeWidth,
            onValueChange = { onStateChange(state.copy(strokeWidth = it)) },
            valueRange = 1f..100f
        )
        Text("Opacity " + (state.opacity * 100).toInt() + "%")
        Slider(
            value = state.opacity,
            onValueChange = { onStateChange(state.copy(opacity = it)) },
            valueRange = 0f..1f
        )

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Grease Pencil Layers", style = MaterialTheme.typography.titleSmall)
        InspectorRow("GP Layer 1", state.activeLayer == 0) {
            onStateChange(state.copy(activeLayer = 0))
        }
        InspectorRow("GP Layer 2", state.activeLayer == 1) {
            onStateChange(state.copy(activeLayer = 1))
        }
        TextButton({}) { Text("+ Add GP Layer") }

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("GP Materials", style = MaterialTheme.typography.titleSmall)
        InspectorRow("Grease Pencil Material", state.activeMaterial == 0) {
            onStateChange(state.copy(activeMaterial = 0))
        }
        TextButton({}) { Text("+ Add Material") }

        HorizontalDivider(Modifier.padding(vertical = 8.dp))
        Text("Blender Viewport", style = MaterialTheme.typography.titleSmall)
        TextButton({}) { Text("Frame Selected") }
        TextButton({}) { Text("Reset View") }
        TextButton({}) { Text("Orthographic / Perspective") }
    }
}

@Composable
private fun InspectorRow(title: String, selected: Boolean, onClick: () -> Unit) {
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
    Column(Modifier.fillMaxWidth().height(170.dp)) {
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
            TextButton({}) { Text("Duplicate") }
            TextButton({}) { Text("Delete") }
        }

        Row(
            Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(8.dp)
        ) {
            for (frame in 1..48) {
                val selected = frame == state.frame
                Surface(
                    Modifier.width(72.dp).height(72.dp).padding(2.dp).clickable {
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

typealias BlenderViewportSurface = @Composable BoxScope.() -> Unit
