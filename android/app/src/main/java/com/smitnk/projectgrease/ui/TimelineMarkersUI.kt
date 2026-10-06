package com.smitnk.projectgrease.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.TimelineRules

val MarkerColor = Color(0xFFFFFFFF)
val MarkerSelectedColor = Color(0xFFFF8500)
/** Preview-range tint of the timeline (Blender darkens frames outside it; we tint the frames inside). */
val PreviewRangeColor = Color(0x553D6FB5)

/**
 * Scrub strip above the frame cells (time_scrub_ui): drag or tap to change the current frame, snapped
 * to whole frames (or the nearest keyframe when "Snap to keys" is on). Markers are drawn as triangles,
 * the preview range as a band and the current frame as a line. It follows the cell strip's scrolling.
 */
@Composable
fun TimelineScrubStrip(controller: EditorController, listState: LazyListState, cellWidth: Dp, redraw: () -> Unit) {
    val cellPx = with(LocalDensity.current) { cellWidth.toPx() }
    val anim = controller.animation
    fun position(x: Float) = TimelineRules.framePosition(x, cellPx,
        listState.firstVisibleItemIndex + listState.firstVisibleItemScrollOffset / cellPx)
    Canvas(
        Modifier.fillMaxWidth().height(22.dp).testTag("timelineScrub")
            .pointerInput(cellPx) {
                detectTapGestures(onTap = { if (controller.scrubTo(position(it.x))) redraw() })
            }
            .pointerInput(cellPx) {
                detectHorizontalDragGestures(
                    onDragStart = { if (controller.scrubTo(position(it.x))) redraw() },
                    onHorizontalDrag = { change, _ -> change.consume(); if (controller.scrubTo(position(change.position.x))) redraw() }
                )
            }
    ) {
        // Frame f occupies [f - 1, f) cells from the strip start; first visible cell sits at -offset.
        val origin = -(listState.firstVisibleItemIndex * cellPx + listState.firstVisibleItemScrollOffset)
        fun centerX(frame: Int) = origin + (frame - 0.5f) * cellPx
        val preview = anim.timeline.preview
        if (preview.enabled) {
            val left = origin + (preview.start - 1) * cellPx
            val right = origin + preview.end * cellPx
            drawRect(PreviewRangeColor, Offset(left, 0f), Size(right - left, size.height))
        }
        for (m in anim.timeline.markers) {
            val x = centerX(m.frame)
            if (x < -cellPx || x > size.width + cellPx) continue
            val path = androidx.compose.ui.graphics.Path().apply {
                moveTo(x - 6f, 0f); lineTo(x + 6f, 0f); lineTo(x, 12f); close()
            }
            drawPath(path, if (m.selected) MarkerSelectedColor else MarkerColor)
        }
        val cx = centerX(anim.currentFrame)
        drawLine(Color(0xFF4772B3), Offset(cx, 0f), Offset(cx, size.height), strokeWidth = 4f)
    }
}

/** Marker list: select, rename (MARKER_OT_rename), move (MARKER_OT_move), delete (MARKER_OT_delete). */
@Composable
fun MarkersDialog(controller: EditorController, onDismiss: () -> Unit, redraw: () -> Unit) {
    val markers = controller.animation.timeline.markers
    val active = markers.firstOrNull { it.selected }
    var name by remember(active?.frame, active?.name) { mutableStateOf(active?.name ?: "") }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Markers") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                if (markers.isEmpty()) Text("No markers. Use \"Add marker\" to mark the current frame.")
                for (m in markers) {
                    Row(Modifier.fillMaxWidth().clickable { controller.selectMarker(m.frame, extend = true); redraw() }
                        .padding(vertical = 6.dp).testTag("marker_${m.frame}")) {
                        Text(if (m.selected) "▼" else "▽", color = if (m.selected) MarkerSelectedColor else Color.Unspecified)
                        Text(" ${m.frame}  ${m.name}", fontWeight = if (m.selected) FontWeight.Bold else FontWeight.Normal)
                    }
                }
                if (active != null) {
                    OutlinedTextField(name, { name = it.take(TimelineRules.MAX_MARKER_NAME) }, label = { Text("Name") }, singleLine = true,
                        modifier = Modifier.fillMaxWidth().testTag("markerName"))
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        TextButton(onClick = { controller.renameMarker(name); redraw() }) { Text("Rename") }
                        TextButton(onClick = { controller.moveSelectedMarkers(-1); redraw() }) { Text("-1") }
                        TextButton(onClick = { controller.moveSelectedMarkers(1); redraw() }) { Text("+1") }
                        TextButton(onClick = { controller.deleteSelectedMarkers(); redraw() }, modifier = Modifier.testTag("markerDelete")) { Text("Delete") }
                    }
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text("Close") } },
        dismissButton = {
            TextButton(onClick = { controller.addMarker(); redraw() }) { Text("Add at ${controller.animation.currentFrame}") }
        }
    )
}

/** ANIM_OT_previewrange_set / ANIM_OT_previewrange_clear: start and end frames; playback loops inside. */
@Composable
fun PreviewRangeDialog(controller: EditorController, onDismiss: () -> Unit, redraw: () -> Unit) {
    val anim = controller.animation
    val preview = anim.timeline.preview
    val sel = anim.selectedFrames
    var start by remember { mutableStateOf((if (preview.enabled) preview.start else sel.minOrNull() ?: 1).toString()) }
    var end by remember { mutableStateOf((if (preview.enabled) preview.end else sel.maxOrNull() ?: anim.timelineEnd).toString()) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Preview range") },
        text = {
            Column {
                OutlinedTextField(start, { start = it.filter(Char::isDigit).take(7) }, label = { Text("Start") }, singleLine = true,
                    modifier = Modifier.testTag("previewStart"))
                OutlinedTextField(end, { end = it.filter(Char::isDigit).take(7) }, label = { Text("End") }, singleLine = true,
                    modifier = Modifier.testTag("previewEnd"))
            }
        },
        confirmButton = {
            TextButton(onClick = {
                controller.setPreviewRange(start.toIntOrNull() ?: 1, end.toIntOrNull() ?: 1); redraw(); onDismiss()
            }) { Text("Set") }
        },
        dismissButton = {
            TextButton(enabled = preview.enabled, onClick = { controller.clearPreviewRange(); redraw(); onDismiss() }) { Text("Clear") }
        }
    )
}
