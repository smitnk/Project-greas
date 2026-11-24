package com.smitnk.projectgrease.ui

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp

/** Tool-change feedback: Blender-fast, and inside the 300 ms budget of the design brief. */
internal const val TOOL_ANIM_MS = 150

/** Every mode of Blender's mode menu, in Blender's order. Object and Pose route to Blender's own
 *  operators; until the controller knows them, modeToggle() returns false and the chip says so. */
internal enum class StudioMode(val label: String, val op: String) {
    OBJECT("Object", "OBJECT_OT_mode_set"),
    EDIT("Edit", "GPENCIL_OT_editmode_toggle"),
    SCULPT("Sculpt", "GPENCIL_OT_sculptmode_toggle"),
    DRAW("Draw", "GPENCIL_OT_paintmode_toggle"),
    WEIGHT_PAINT("Weight Paint", "GPENCIL_OT_weightmode_toggle"),
    VERTEX_PAINT("Vertex Paint", "GPENCIL_OT_vertexmode_toggle"),
    POSE("Pose", "OBJECT_OT_posemode_toggle"),
}

internal fun studioModeIcon(m: StudioMode): ImageVector = when (m) {
    StudioMode.OBJECT -> PgIcons.ViewInAr; StudioMode.EDIT -> PgIcons.Edit; StudioMode.SCULPT -> PgIcons.PanTool
    StudioMode.DRAW -> PgIcons.Brush; StudioMode.WEIGHT_PAINT -> PgIcons.LineWeight
    StudioMode.VERTEX_PAINT -> PgIcons.FormatPaint; StudioMode.POSE -> PgIcons.Animation
}

/** Always-visible mode switcher (Blender header's mode menu laid out as chips). */
@Composable internal fun ModeSwitcher(current: String, onPick: (StudioMode) -> Unit, modifier: Modifier = Modifier) {
    Row(modifier.horizontalScroll(rememberScrollState()).testTag("modeSwitcher"), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        StudioMode.entries.forEach { m ->
            FilterChip(selected = m.name == current, onClick = { onPick(m) },
                label = { Text(m.label, maxLines = 1) },
                leadingIcon = { Icon(studioModeIcon(m), null, Modifier.size(18.dp)) },
                modifier = Modifier.heightIn(min = 48.dp).testTag("modeChip_" + m.name))
        }
    }
}

/** One rail button: 48 dp, labelled for TalkBack, the selection fades in within TOOL_ANIM_MS. */
@Composable internal fun RailButton(icon: ImageVector, label: String, selected: Boolean, tag: String = "studioTool_$label", onClick: () -> Unit) {
    val bg by animateColorAsState(if (selected) MaterialTheme.colorScheme.primary else Color.Transparent, tween(TOOL_ANIM_MS), label = "rail")
    val fg by animateColorAsState(if (selected) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface, tween(TOOL_ANIM_MS), label = "railFg")
    Box(Modifier.padding(horizontal = 4.dp, vertical = 1.dp).size(48.dp).background(bg, RoundedCornerShape(12.dp))
        .clickable(role = Role.Button, onClick = onClick).semantics { contentDescription = label }.testTag(tag),
        contentAlignment = Alignment.Center) { Icon(icon, null, tint = fg) }
}

/** Quick brush pop-up: Radius, Strength and Color in one card (Blender's F / Shift+F / color swatch). */
@Composable internal fun BrushQuickPanel(size: Float, strength: Float, colorArgb: Int,
                                       onSize: (Float) -> Unit, onStrength: (Float) -> Unit, onColor: () -> Unit) {
    Column(Modifier.padding(horizontal = 16.dp, vertical = 8.dp).widthIn(min = 240.dp).testTag("brushQuick")) {
        Text("Brush", fontWeight = FontWeight.Bold)
        Text("Radius ${size.toInt()} px"); Slider(size, onSize, valueRange = 1f..500f, modifier = Modifier.testTag("brushQuickSize"))
        Text("Strength ${(strength * 100).toInt()}%"); Slider(strength, onStrength, valueRange = 0f..1f, modifier = Modifier.testTag("brushQuickStrength"))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Color", Modifier.weight(1f))
            Box(Modifier.size(48.dp).background(Color(colorArgb), CircleShape).border(2.dp, MaterialTheme.colorScheme.outline, CircleShape)
                .clickable(role = Role.Button, onClick = onColor).semantics { contentDescription = "Color" }.testTag("brushQuickColor"))
        }
    }
}

// ---------- the three layout directions (static mock-ups, no native renderer needed) ----------

private val railTools = listOf(PgIcons.Brush to "Draw", PgIcons.Clear to "Erase", PgIcons.Gesture to "Lasso",
    PgIcons.FormatColorFill to "Fill", PgIcons.Layers to "Layers", PgIcons.Undo to "Undo", PgIcons.Redo to "Redo")

@Composable private fun MockCanvas(modifier: Modifier) = Box(modifier.background(Color(0xFF3D3D3D)))

@Composable private fun MockTimeline(modifier: Modifier = Modifier) = Surface(modifier.fillMaxWidth(), tonalElevation = 3.dp) {
    Column {
        Box(Modifier.align(Alignment.CenterHorizontally).padding(6.dp).size(36.dp, 4.dp).background(MaterialTheme.colorScheme.outline, CircleShape))
        Row(Modifier.padding(horizontal = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            RailButton(PgIcons.PlayArrow, "Play", false) {}
            (1..8).forEach { f -> Box(Modifier.padding(2.dp).size(28.dp).background(
                if (f == 3) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(4.dp))) }
        }
    }
}

/** A: Blender rail. Collapsible left tool rail (T panel), mode chips on top, drag-up timeline. */
@Composable fun DirectionBlenderRail() = Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
    Column(Modifier.fillMaxSize()) {
        ModeSwitcher("DRAW", {}, Modifier.padding(6.dp))
        Row(Modifier.weight(1f)) {
            Surface(tonalElevation = 6.dp, shape = RoundedCornerShape(topEnd = 16.dp, bottomEnd = 16.dp)) {
                Column { RailButton(PgIcons.ChevronLeft, "Hide tools", false) {}; railTools.forEachIndexed { i, (ic, l) -> RailButton(ic, l, i == 0) {} } }
            }
            MockCanvas(Modifier.weight(1f).fillMaxHeight())
        }
        MockTimeline()
    }
}

/** B: Thumb dock. All tools in a bottom dock for one-hand reach, mode pill above it. */
@Composable fun DirectionThumbDock() = Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
    Column(Modifier.fillMaxSize()) {
        MockCanvas(Modifier.weight(1f).fillMaxWidth())
        MockTimeline()
        ModeSwitcher("DRAW", {}, Modifier.padding(4.dp))
        Surface(tonalElevation = 6.dp) {
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())) { railTools.forEachIndexed { i, (ic, l) -> RailButton(ic, l, i == 0) {} } }
        }
    }
}

/** C: Brush ring. Canvas-first; a ring of size/strength/color around the thumb, slim right rail. */
@Composable fun DirectionBrushRing() = Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
    MockCanvas(Modifier.fillMaxSize())
    Surface(Modifier.align(Alignment.TopCenter).padding(8.dp), shape = CircleShape, tonalElevation = 6.dp) {
        Row(Modifier.padding(horizontal = 12.dp).heightIn(min = 48.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(PgIcons.Brush, null); Spacer(Modifier.width(6.dp)); Text("Draw"); Icon(PgIcons.ArrowDropDown, "Change mode")
        }
    }
    Surface(Modifier.align(Alignment.CenterEnd).padding(6.dp), shape = RoundedCornerShape(24.dp), tonalElevation = 6.dp) {
        Column { railTools.take(4).forEachIndexed { i, (ic, l) -> RailButton(ic, l, i == 0) {} } }
    }
    Surface(Modifier.align(Alignment.BottomEnd).padding(bottom = 96.dp, end = 72.dp), shape = RoundedCornerShape(24.dp), tonalElevation = 8.dp) {
        BrushQuickPanel(24f, 0.8f, 0xFFE87D0D.toInt(), {}, {}, {})
    }
    MockTimeline(Modifier.align(Alignment.BottomCenter))
}

@Preview(name = "A Blender rail", widthDp = 360, heightDp = 760) @Composable private fun PreviewA() = ProjectGreaseTheme(ProjectGreaseThemeMode.DARK) { DirectionBlenderRail() }
@Preview(name = "B Thumb dock", widthDp = 360, heightDp = 760) @Composable private fun PreviewB() = ProjectGreaseTheme(ProjectGreaseThemeMode.DARK) { DirectionThumbDock() }
@Preview(name = "C Brush ring", widthDp = 360, heightDp = 760) @Composable private fun PreviewC() = ProjectGreaseTheme(ProjectGreaseThemeMode.DARK) { DirectionBrushRing() }
@Preview(name = "A light, 200% text", widthDp = 360, heightDp = 760, fontScale = 2f) @Composable private fun PreviewALight() = ProjectGreaseTheme(ProjectGreaseThemeMode.LIGHT) { DirectionBlenderRail() }
