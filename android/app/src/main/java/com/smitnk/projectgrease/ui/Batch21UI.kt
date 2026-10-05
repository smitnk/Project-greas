@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)
package com.smitnk.projectgrease.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.smitnk.projectgrease.editor.CurvePoints
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.ModifierRecord
import com.smitnk.projectgrease.editor.ModifierSpecs
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.ParamKind
import com.smitnk.projectgrease.editor.ProjectGreaseSelect

private val Accent21 = Color(0xFFE84F7B)

/** Canvas units -> screen pixels of the overlay, as the EGL surface maps them (fit 0.92 * zoom + pan). */
private class OverlayMap(w: Float, h: Float, controller: EditorController) {
    private val cw = controller.document.canvasWidth.coerceAtLeast(1).toFloat()
    private val ch = controller.document.canvasHeight.coerceAtLeast(1).toFloat()
    private val fit = minOf(w / cw, h / ch) * 0.92f * controller.view.zoom
    private val ox = (w - cw * fit) * 0.5f + controller.view.panX
    private val oy = (h - ch * fit) * 0.5f + controller.view.panY
    fun screen(p: Pair<Float, Float>) = Offset(ox + p.first * fit, oy + p.second * fit)
    fun scale() = fit
}

/**
 * CurveMapping editor (Blender's curve widget): the curve through the points in a unit square,
 * drag a point to move it, tap empty space to add one, "Remove" deletes the selected point
 * (2 points stay). The curve itself is drawn piecewise-linearly between the points; the stroke uses
 * the native CurveMapping evaluation (auto handles).
 */
@Composable
fun CurveEditor(label: String, points: List<Pair<Float, Float>>, onChange: (List<Pair<Float, Float>>) -> Unit, tag: String = "curveEditor") {
    var selected by remember { mutableIntStateOf(-1) }
    var local by remember(points) { mutableStateOf(points) }
    Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 4.dp)) {
        Text(label, fontWeight = FontWeight.Bold, fontSize = 12.sp)
        Box(Modifier.fillMaxWidth().height(140.dp).border(1.dp, MaterialTheme.colorScheme.outline, RoundedCornerShape(4.dp)).testTag(tag)) {
            Canvas(Modifier.fillMaxSize()
                .pointerInput(points) {
                    detectTapGestures { o ->
                        val x = o.x / size.width; val y = 1f - o.y / size.height
                        val hit = CurvePoints.hit(local, x, y, 0.06f)
                        if (hit >= 0) selected = hit
                        else { local = CurvePoints.insert(local, x, y); selected = CurvePoints.hit(local, x, y, 0.06f); onChange(local) }
                    }
                }
                .pointerInput(points) {
                    detectDragGestures(
                        onDragStart = { o -> selected = CurvePoints.hit(local, o.x / size.width, 1f - o.y / size.height, 0.08f) },
                        onDragEnd = { onChange(local) }
                    ) { change, _ ->
                        val i = selected
                        if (i >= 0) {
                            val x = (change.position.x / size.width).coerceIn(0f, 1f)
                            val y = (1f - change.position.y / size.height).coerceIn(0f, 1f)
                            local = local.toMutableList().also { it[i] = x to y }
                        }
                    }
                }) {
                val path = Path()
                local.sortedBy { it.first }.forEachIndexed { i, (x, y) ->
                    val p = Offset(x * size.width, (1f - y) * size.height)
                    if (i == 0) path.moveTo(p.x, p.y) else path.lineTo(p.x, p.y)
                }
                drawPath(path, Accent21, style = Stroke(width = 3f))
                local.forEachIndexed { i, (x, y) ->
                    drawCircle(if (i == selected) Accent21 else Color.Gray, 9f, Offset(x * size.width, (1f - y) * size.height))
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            TextButton(onClick = { if (selected >= 0) { local = CurvePoints.remove(local, selected); selected = -1; onChange(local) } },
                enabled = selected >= 0 && local.size > 2) { Text("Remove point", fontSize = 11.sp) }
            TextButton(onClick = { local = listOf(0f to 0f, 1f to 1f); selected = -1; onChange(local) }) { Text("Reset (linear)", fontSize = 11.sp) }
        }
    }
}

/** Line / Box / Circle / Arc edit phase: handles, subdivisions -/+, extrude (line), confirm / cancel. */
@Composable
fun ShapeEditOverlay(controller: EditorController, tick: Int, redraw: () -> Unit) {
    @Suppress("UNUSED_VARIABLE") val observed = tick
    if (!controller.shapeEditing) return
    val handles = controller.shapeHandles()
    Box(Modifier.fillMaxSize().testTag("shapeEditOverlay")) {
        Canvas(Modifier.fillMaxSize()) {
            val m = OverlayMap(size.width, size.height, controller)
            handles.forEach { h -> drawCircle(Color.Black, 12f, m.screen(h)); drawCircle(Color.White, 9f, m.screen(h)) }
        }
        Row(Modifier.align(Alignment.BottomCenter).padding(8.dp).horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            // Confirm / Cancel first so they stay on screen on narrow devices.
            Button(onClick = { controller.confirmShape(); redraw() }, modifier = Modifier.testTag("shapeConfirm")) { Text("Confirm") }
            OutlinedButton(onClick = { controller.cancelShape(); redraw() }) { Text("Cancel") }
            OutlinedButton(onClick = { controller.changeShapeSubdivisions(-1); redraw() }) { Text("Subdiv -") }
            Text(if (controller.shapeSubdivisions() > 0) "${controller.shapeSubdivisions()}" else "default", Modifier.align(Alignment.CenterVertically))
            OutlinedButton(onClick = { controller.changeShapeSubdivisions(1); redraw() }) { Text("Subdiv +") }
            OutlinedButton(onClick = { controller.extrudeShape(); redraw() }) { Text("Extrude") }
        }
    }
}

/** Handles of the modifier being edited on the canvas (Hook centre/target, Lattice grid, point, pivot). */
@Composable
fun ModifierHandlesOverlay(controller: EditorController, tick: Int, redraw: () -> Unit) {
    @Suppress("UNUSED_VARIABLE") val observed = tick
    if (!controller.gizmoActive) return
    val handles = controller.gizmoHandles()
    Box(Modifier.fillMaxSize().testTag("modifierHandles")) {
        Canvas(Modifier.fillMaxSize()) {
            val m = OverlayMap(size.width, size.height, controller)
            if (handles.size == 2) drawLine(Color(0xAAFFFFFF), m.screen(handles[0]), m.screen(handles[1]), 2f)
            handles.forEachIndexed { i, h ->
                drawCircle(Color.Black, 12f, m.screen(h))
                drawCircle(if (i == 0) Color(0xFFFFB000) else Color(0xFF4FC3F7), 9f, m.screen(h))
            }
        }
        Button(onClick = { controller.editModifierHandles(null); redraw() }, Modifier.align(Alignment.BottomCenter).padding(8.dp)) {
            Text("Done editing handles")
        }
    }
}

/** Box select: the rectangle being dragged. */
@Composable
fun BoxSelectOverlay(controller: EditorController, tick: Int) {
    @Suppress("UNUSED_VARIABLE") val observed = tick
    val r = controller.boxSelectRect ?: return
    Canvas(Modifier.fillMaxSize()) {
        val m = OverlayMap(size.width, size.height, controller)
        val a = m.screen(minOf(r[0], r[2]) to minOf(r[1], r[3])); val b = m.screen(maxOf(r[0], r[2]) to maxOf(r[1], r[3]))
        drawRect(Color.Black, a, androidx.compose.ui.geometry.Size(b.x - a.x, b.y - a.y), style = Stroke(width = 3f))
        drawRect(Color.White, a, androidx.compose.ui.geometry.Size(b.x - a.x, b.y - a.y), style = Stroke(width = 1.5f))
    }
}

/** Edit-mode selection operators that had no button (Blender's Select menu) and the point-pick switch. */
@Composable
fun SelectOperatorsBar(controller: EditorController, redraw: () -> Unit) {
    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 8.dp, vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically) {
        Text("Select", fontWeight = FontWeight.Bold, fontSize = 10.sp, modifier = Modifier.padding(end = 6.dp))
        listOf(
            "All" to { controller.selectAll() }, "None" to { controller.deselectAll() }, "Invert" to { controller.invertSelection() },
            "Linked" to { controller.selectLinked() }, "Alternate" to { controller.selectAlternate() },
            "More" to { controller.selectMore() }, "Less" to { controller.selectLess() },
            "First" to { controller.selectFirstPoints() }, "Last" to { controller.selectLastPoints() },
            "Same layer" to { controller.selectGroupedByLayer() }, "Same material" to { controller.selectGroupedByMaterial() },
            "Delete points" to { controller.deleteSelectedPoints() }, "Join" to { controller.joinSelection() }
        ).forEach { (label, action) ->
            TextButton(onClick = { if (action()) redraw() }, modifier = Modifier.testTag("select_$label")) { Text(label, fontSize = 11.sp) }
        }
        FilterChip(selected = !controller.pickEntireStrokes, onClick = { controller.setPickEntireStrokes(!controller.pickEntireStrokes); redraw() },
            label = { Text("Pick points", fontSize = 10.sp) })
    }
}

/** Timeline marker letters of the key types (Blender draws them as differently shaped diamonds). */
fun keyTypeMark(type: Int) = when (type) {
    ProjectGreaseSelect.KEY_EXTREME -> "E"; ProjectGreaseSelect.KEY_BREAKDOWN -> "B"; ProjectGreaseSelect.KEY_JITTER -> "J"
    ProjectGreaseSelect.KEY_MOVEHOLD -> "MH"; else -> "K"
}
fun keyTypeColor(type: Int) = when (type) {
    ProjectGreaseSelect.KEY_EXTREME -> Color(0xFFE8B3CC); ProjectGreaseSelect.KEY_BREAKDOWN -> Color(0xFF54BFED)
    ProjectGreaseSelect.KEY_JITTER -> Color(0xFF61C042); ProjectGreaseSelect.KEY_MOVEHOLD -> Color(0xFF5C5656)
    else -> Color(0xFFE8E8E8)
}

/** Long-press menu of a timeline frame: key type, frame selection (multiframe), interpolate sequence. */
@Composable
fun KeyframeMenu(controller: EditorController, frame: Int, onDismiss: () -> Unit, redraw: () -> Unit) {
    DropdownMenu(expanded = true, onDismissRequest = onDismiss) {
        if (frame in controller.animation.keyframes) {
            ProjectGreaseSelect.KEY_TYPE_LABELS.forEachIndexed { type, label ->
                DropdownMenuItem(text = { Text("Key type: $label") }, onClick = { controller.setFrameKeyType(frame, type); onDismiss(); redraw() },
                    modifier = Modifier.testTag("keyType_$type"))
            }
            DropdownMenuItem(text = { Text(if (frame in controller.animation.selectedFrames) "Deselect frame" else "Select frame (multiframe)") },
                onClick = { controller.selectTimelineFrame(frame); onDismiss(); redraw() })
        }
        DropdownMenuItem(text = { Text("Deselect all frames") }, onClick = { controller.deselectTimelineFrames(); onDismiss(); redraw() })
        val cur = controller.animation.currentFrame
        DropdownMenuItem(text = { Text("Box select frames $frame..$cur") }, modifier = Modifier.testTag("framesBox"),
            onClick = { controller.boxSelectFrames(frame, cur, extend = true); onDismiss(); redraw() })
        DropdownMenuItem(text = { Text("Move selected frames +1") }, modifier = Modifier.testTag("framesMoveRight"),
            onClick = { controller.moveSelectedFrames(1); onDismiss(); redraw() })
        DropdownMenuItem(text = { Text("Move selected frames -1") }, modifier = Modifier.testTag("framesMoveLeft"),
            onClick = { controller.moveSelectedFrames(-1); onDismiss(); redraw() })
        DropdownMenuItem(text = { Text("Scale selected frames x2 (around current)") }, modifier = Modifier.testTag("framesScale2"),
            onClick = { controller.scaleSelectedFrames(2f); onDismiss(); redraw() })
        DropdownMenuItem(text = { Text("Scale selected frames x0.5 (around current)") },
            onClick = { controller.scaleSelectedFrames(0.5f); onDismiss(); redraw() })
        DropdownMenuItem(text = { Text("Copy selected frames") }, modifier = Modifier.testTag("framesCopy"),
            onClick = { controller.copySelectedFrames(); onDismiss() })
        DropdownMenuItem(text = { Text("Paste frames at $cur (overwrite)") }, modifier = Modifier.testTag("framesPaste"),
            onClick = { controller.pasteFrames(); onDismiss(); redraw() })
        DropdownMenuItem(text = { Text("Interpolate sequence (all in-betweens)") }, onClick = {
            val n = controller.animation.interpolateSequence(frame)
            if (n > 0) { controller.history.markEdit(); controller.document.markDirty() }; controller.render(); onDismiss(); redraw()
        })
    }
}

/** Layer blend mode, tint and thickness offset (saved with the layer). */
@Composable
fun LayerLookSection(controller: EditorController, layerKey: Any, redraw: () -> Unit) {
    val record = remember(layerKey) { controller.layerState() }
    var tick by remember { mutableIntStateOf(0) }
    val r = remember(layerKey, tick) { controller.layerState() } ?: record ?: return
    var factor by remember(layerKey) { mutableFloatStateOf(r.tint.getOrElse(3) { 0f }) }
    var line by remember(layerKey) { mutableFloatStateOf(r.lineChange.toFloat()) }
    fun changed(ok: Boolean) { if (ok) { tick++; redraw() } }
    Text("Blend", Modifier.padding(horizontal = 12.dp, vertical = 6.dp), fontWeight = FontWeight.Bold)
    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp)) {
        ProjectGreaseSelect.BLEND_LABELS.forEachIndexed { mode, label ->
            FilterChip(selected = r.blendMode == mode, onClick = { changed(controller.setLayerBlend(mode)) }, label = { Text(label, fontSize = 10.sp) },
                modifier = Modifier.padding(end = 4.dp).testTag("blend_$mode"))
        }
    }
    Text("Tint", Modifier.padding(horizontal = 12.dp, vertical = 6.dp), fontWeight = FontWeight.Bold)
    val tintArgb = (0xFF shl 24) or ((r.tint[0] * 255).toInt() shl 16) or ((r.tint[1] * 255).toInt() shl 8) or (r.tint[2] * 255).toInt()
    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        listOf(Color.Black, Color.White, Color(0xFFE53935), Color(0xFFFF9800), Color(0xFFFFEB3B), Color(0xFF4CAF50), Color(0xFF2196F3), Color(0xFF9C27B0)).forEach { c ->
            Box(Modifier.size(28.dp).background(c, CircleShape).border(2.dp, if (c.toArgb() == tintArgb) Accent21 else Color.Gray, CircleShape)
                .clickable { changed(controller.setLayerTint(c.toArgb(), factor)) })
        }
    }
    Text("Tint factor " + "%.2f".format(factor), Modifier.padding(horizontal = 12.dp))
    Slider(factor, { factor = it }, onValueChangeFinished = { changed(controller.setLayerTint(tintArgb, factor)) }, valueRange = 0f..1f,
        modifier = Modifier.padding(horizontal = 12.dp).testTag("tintFactor"))
    Text("Stroke thickness " + (if (line >= 0) "+" else "") + line.toInt() + " px", Modifier.padding(horizontal = 12.dp))
    Slider(line, { line = it }, onValueChangeFinished = { changed(controller.setLayerLineChange(Math.round(line))) }, valueRange = -50f..50f,
        modifier = Modifier.padding(horizontal = 12.dp))
}

/** Material slots: names, order, lock / hide / solo, line type (line, dots, squares) and pass index. */
@Composable
fun MaterialSlotsSection(controller: EditorController, redraw: () -> Unit) {
    var tick by remember { mutableIntStateOf(0) }
    var renameSlot by remember { mutableIntStateOf(-1) }
    var renameText by remember { mutableStateOf("") }
    fun changed(ok: Boolean) { if (ok) { tick++; redraw() } }
    val count = remember(tick) { controller.materialCount() }
    Text("Material slots", Modifier.padding(horizontal = 20.dp, vertical = 8.dp), fontWeight = FontWeight.Bold)
    for (slot in 0 until count) {
        val rec = remember(tick, slot) { controller.materialRecord(slot) }
        if (rec != null) MaterialSlotRow(controller, slot, count, rec, tick, { changed(it) }) { renameSlot = slot; renameText = controller.materialName(slot) }
    }
    if (renameSlot >= 0) {
        AlertDialog(onDismissRequest = { renameSlot = -1 }, title = { Text("Rename material") },
            text = { OutlinedTextField(renameText, { renameText = it }, singleLine = true) },
            confirmButton = { TextButton(onClick = { changed(controller.renameMaterial(renameSlot, renameText)); renameSlot = -1 }) { Text("Rename") } },
            dismissButton = { TextButton(onClick = { renameSlot = -1 }) { Text("Cancel") } })
    }
}

@Composable
private fun MaterialSlotRow(controller: EditorController, slot: Int, count: Int, rec: com.smitnk.projectgrease.editor.MaterialRecord, tick: Int,
                            changed: (Boolean) -> Unit, onRename: () -> Unit) {
    run {
        val active = controller.materials.activeMaterial == slot
        Column(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 3.dp)
            .border(if (active) 2.dp else 1.dp, if (active) Accent21 else MaterialTheme.colorScheme.outline, RoundedCornerShape(8.dp)).padding(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(18.dp).background(Color(rec.stroke[0], rec.stroke[1], rec.stroke[2], 1f), CircleShape))
                Text(controller.materialName(slot), Modifier.weight(1f).padding(start = 8.dp).clickable { controller.selectMaterial(slot); changed(true) }, fontWeight = if (active) FontWeight.Bold else FontWeight.Normal)
                TextButton(onClick = onRename) { Text("Rename", fontSize = 11.sp) }
            }
            Row(Modifier.horizontalScroll(rememberScrollState()), verticalAlignment = Alignment.CenterVertically) {
                TextButton(onClick = { changed(controller.moveMaterial(slot, -1)) }, enabled = slot > 0) { Text("Up", fontSize = 11.sp) }
                TextButton(onClick = { changed(controller.moveMaterial(slot, 1)) }, enabled = slot < count - 1) { Text("Down", fontSize = 11.sp) }
                FilterChip(selected = rec.locked, onClick = { changed(controller.setMaterialLocked(slot, !rec.locked)) }, label = { Text("Lock", fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp))
                FilterChip(selected = !rec.visible, onClick = { changed(controller.setMaterialHidden(slot, rec.visible)) }, label = { Text("Hide", fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp))
                TextButton(onClick = { changed(controller.soloMaterial(slot)) }) { Text("Solo", fontSize = 11.sp) }
            }
            Row(Modifier.horizontalScroll(rememberScrollState()), verticalAlignment = Alignment.CenterVertically) {
                Text("Line type", fontSize = 10.sp, modifier = Modifier.padding(end = 4.dp))
                ProjectGreaseSelect.LINE_TYPE_LABELS.forEachIndexed { mode, label ->
                    FilterChip(selected = rec.mode == mode, onClick = { changed(controller.setMaterialLineType(mode, rec.alignment, rec.rotation, slot)) },
                        label = { Text(label, fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp).testTag("lineType_${slot}_$mode"))
                }
                if (rec.mode != 0) {
                    Text("Align", fontSize = 10.sp, modifier = Modifier.padding(horizontal = 4.dp))
                    ProjectGreaseSelect.ALIGNMENT_LABELS.forEachIndexed { a, label ->
                        FilterChip(selected = rec.alignment == a, onClick = { changed(controller.setMaterialLineType(rec.mode, a, rec.rotation, slot)) },
                            label = { Text(label, fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp))
                    }
                }
            }
            if (rec.mode != 0) {
                var rot by remember(slot, tick) { mutableFloatStateOf(rec.rotation) }
                Text("Rotation " + Math.round(Math.toDegrees(rot.toDouble())) + "°", fontSize = 11.sp)
                Slider(rot, { rot = it }, onValueChangeFinished = { changed(controller.setMaterialLineType(rec.mode, rec.alignment, rot, slot)) },
                    valueRange = -3.1415927f..3.1415927f)
            }
            Row(Modifier.horizontalScroll(rememberScrollState()), verticalAlignment = Alignment.CenterVertically) {
                FilterChip(selected = rec.strokeHoldout, onClick = { changed(controller.setMaterialOptions(slot, !rec.strokeHoldout, rec.fillHoldout, rec.selfOverlap)) },
                    label = { Text("Stroke holdout", fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp).testTag("strokeHoldout_$slot"))
                FilterChip(selected = rec.fillHoldout, onClick = { changed(controller.setMaterialOptions(slot, rec.strokeHoldout, !rec.fillHoldout, rec.selfOverlap)) },
                    label = { Text("Fill holdout", fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp))
                FilterChip(selected = rec.selfOverlap, onClick = { changed(controller.setMaterialOptions(slot, rec.strokeHoldout, rec.fillHoldout, !rec.selfOverlap)) },
                    label = { Text("Self overlap", fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp).testTag("selfOverlap_$slot"))
            }
            MaterialGradientRow(controller, slot, rec, tick, changed)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Pass index " + rec.passIndex, fontSize = 11.sp, modifier = Modifier.weight(1f))
                TextButton(onClick = { changed(controller.setMaterialPass((rec.passIndex - 1).coerceAtLeast(0), slot)) }) { Text("-") }
                TextButton(onClick = { changed(controller.setMaterialPass(rec.passIndex + 1, slot)) }) { Text("+") }
            }
        }
    }
}

/** Fill style Gradient: type, mix colour, mix factor, angle, scale, offset, flip (material fill panel). */
@Composable
private fun MaterialGradientRow(controller: EditorController, slot: Int, rec: com.smitnk.projectgrease.editor.MaterialRecord, tick: Int,
                                changed: (Boolean) -> Unit) {
    val g = rec.gradient
    Row(Modifier.horizontalScroll(rememberScrollState()), verticalAlignment = Alignment.CenterVertically) {
        Text("Fill gradient", fontSize = 10.sp, modifier = Modifier.padding(end = 4.dp))
        FilterChip(selected = g == null, onClick = { changed(controller.setMaterialGradient(slot, null)) },
            label = { Text("Off", fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp))
        ProjectGreaseSelect.GRADIENT_TYPE_LABELS.forEachIndexed { type, label ->
            FilterChip(selected = g != null && g[0].toInt() == type, onClick = {
                val base = g ?: floatArrayOf(0f, 1f, 1f, 1f, 1f, 0f, 0f, 1f, 1f, 0f, 0f, 0f)
                changed(controller.setMaterialGradient(slot, base.copyOf().also { it[0] = type.toFloat() }))
            }, label = { Text(label, fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp).testTag("gradient_${slot}_$type"))
        }
        if (g != null) FilterChip(selected = g[11] != 0f, onClick = { changed(controller.setMaterialGradient(slot, g.copyOf().also { it[11] = if (g[11] != 0f) 0f else 1f })) },
            label = { Text("Flip", fontSize = 10.sp) })
    }
    if (g == null) return
    @Composable
    fun slider(label: String, index: Int, range: ClosedFloatingPointRange<Float>) {
        var v by remember(slot, tick, index) { mutableFloatStateOf(g[index]) }
        Text("$label ${"%.2f".format(v)}", fontSize = 11.sp)
        Slider(v, { v = it }, onValueChangeFinished = { changed(controller.setMaterialGradient(slot, g.copyOf().also { it[index] = v })) }, valueRange = range)
    }
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text("Mix colour", fontSize = 11.sp, modifier = Modifier.padding(end = 6.dp))
        listOf(Color.White, Color.Black, Color.Red, Color.Blue, Color.Yellow).forEach { c ->
            Box(Modifier.padding(2.dp).size(20.dp).background(c, CircleShape).border(1.dp, MaterialTheme.colorScheme.outline, CircleShape).clickable {
                changed(controller.setMaterialGradient(slot, g.copyOf().also { it[1] = c.red; it[2] = c.green; it[3] = c.blue; it[4] = 1f }))
            })
        }
    }
    slider("Mix factor", 5, 0f..1f)
    slider("Angle", 6, -3.1415927f..3.1415927f)
    slider("Scale X", 7, 0.01f..10f)
    slider("Scale Y", 8, 0.01f..10f)
    slider("Offset X", 9, -1f..1f)
    slider("Offset Y", 10, -1f..1f)
}

/** Onion skin keyframe-type filter and loop. */
@Composable
fun OnionFilterSection(controller: EditorController, redraw: () -> Unit) {
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val t = tick
    Text("Filter by type", Modifier.padding(horizontal = 20.dp, vertical = 4.dp))
    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 20.dp)) {
        (listOf(-1 to "All") + ProjectGreaseSelect.KEY_TYPE_LABELS.mapIndexed { i, l -> i to l }).forEach { (type, label) ->
            FilterChip(selected = controller.onion.keyTypeFilter == type, onClick = { controller.setOnionFilter(keyType = type); tick++; redraw() },
                label = { Text(label, fontSize = 10.sp) }, modifier = Modifier.padding(end = 4.dp))
        }
    }
    Row(Modifier.fillMaxWidth().padding(horizontal = 20.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) { Text("Loop"); Text("The first keyframes also show after the last one", fontSize = 11.sp) }
        Switch(checked = controller.onion.loop, onCheckedChange = { controller.setOnionFilter(loop = it); tick++; redraw() })
    }
}

/** Drawing guides of the Draw tool (circular, radial, parallel, grid, isometric) with snapping. */
@Composable
fun DrawingGuideSection(controller: EditorController, redraw: () -> Unit) {
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val t = tick
    val v = controller.view
    Text("Drawing guide (Draw tool, strokes snap to it)", Modifier.padding(horizontal = 20.dp, vertical = 6.dp), fontWeight = FontWeight.Bold)
    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 20.dp)) {
        listOf(-1 to "Off", 0 to "Circular", 1 to "Radial", 2 to "Parallel", 3 to "Grid", 4 to "Isometric").forEach { (type, label) ->
            FilterChip(selected = v.guideType == type, onClick = { v.setDrawingGuide(type); controller.render(); tick++; redraw() },
                label = { Text(label, fontSize = 10.sp) }, modifier = Modifier.padding(end = 4.dp).testTag("guide_$type"))
        }
    }
    if (v.guideType >= 0) {
        Text("Spacing " + v.guideSpacing.toInt() + " px", Modifier.padding(horizontal = 20.dp))
        Slider(v.guideSpacing, { v.setDrawingGuide(v.guideType, spacing = it); controller.render(); tick++ }, valueRange = 4f..400f, modifier = Modifier.padding(horizontal = 20.dp))
        Text("Angle " + Math.round(Math.toDegrees(v.guideAngle.toDouble())) + "°", Modifier.padding(horizontal = 20.dp))
        Slider(v.guideAngle, { v.setDrawingGuide(v.guideType, angle = it); controller.render(); tick++ }, valueRange = -3.1415927f..3.1415927f, modifier = Modifier.padding(horizontal = 20.dp))
        TextButton(onClick = {
            v.setDrawingGuide(v.guideType, controller.document.canvasWidth / 2f, controller.document.canvasHeight / 2f); controller.render(); tick++
        }, modifier = Modifier.padding(horizontal = 12.dp)) { Text("Center on the canvas") }
    }
}

/** Brush pressure / strength CurveMappings and their switches (curve_sensitivity / curve_strength). */
@Composable
fun BrushCurvesSection(controller: EditorController, redraw: () -> Unit) {
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val t = tick
    val b = controller.brushes
    Text("Brush: " + b.preset.label, Modifier.padding(horizontal = 20.dp, vertical = 6.dp), fontWeight = FontWeight.Bold)
    Row(Modifier.fillMaxWidth().padding(horizontal = 20.dp), verticalAlignment = Alignment.CenterVertically) {
        Text("Use pressure for thickness", Modifier.weight(1f)); Switch(b.usePressure, { b.setUsePressure(it); tick++; redraw() })
    }
    CurveEditor("Thickness pressure curve", b.pressureCurvePoints, { b.setPressureCurvePoints(it); tick++; redraw() }, "pressureCurve")
    Row(Modifier.fillMaxWidth().padding(horizontal = 20.dp), verticalAlignment = Alignment.CenterVertically) {
        Text("Use pressure for strength", Modifier.weight(1f)); Switch(b.useStrengthPressure, { b.setUseStrengthPressure(it); tick++; redraw() })
    }
    CurveEditor("Strength pressure curve", b.strengthCurvePoints, { b.setStrengthCurvePoints(it); tick++; redraw() }, "strengthCurve")
}

/** Influence filters and custom curve of one modifier entry, and its canvas handles. */
@Composable
fun ModifierInfluenceSection(controller: EditorController, index: Int, modifier: ModifierRecord, changed: (Boolean) -> Unit) {
    var open by remember(index) { mutableStateOf(false) }
    if (ModifierType.hasCanvasHandles(modifier.type)) {
        OutlinedButton(onClick = { controller.editModifierHandles(index); changed(true) }, modifier = Modifier.fillMaxWidth().testTag("editHandles_$index")) {
            Text("Edit handles on the canvas")
        }
    }
    TextButton(onClick = { open = !open }) { Text(if (open) "Hide influence" else "Influence (filters, custom curve)") }
    if (!open) return
    ModifierSpecs.filterSpecs().forEach { spec ->
        val value = modifier.params.getOrElse(spec.index) { 0f }
        if (spec.kind == ParamKind.BOOL) Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(spec.label, Modifier.weight(1f), fontSize = 12.sp); Switch(value != 0f, { changed(controller.setModifierParam(index, spec.index, if (it) 1f else 0f)) })
        } else Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(spec.label + ": " + value.toInt(), Modifier.weight(1f), fontSize = 12.sp)
            TextButton(onClick = { changed(controller.setModifierParam(index, spec.index, (value - 1).coerceAtLeast(spec.min))) }) { Text("-") }
            TextButton(onClick = { changed(controller.setModifierParam(index, spec.index, (value + 1).coerceAtMost(spec.max))) }) { Text("+") }
        }
    }
    val use = modifier.params.getOrElse(ModifierType.CURVE_BASE) { 0f } != 0f
    val pts = ModifierSpecs.curvePoints(modifier.params).ifEmpty { listOf(0f to 1f, 1f to 1f) }
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text("Use custom curve (along the stroke)", Modifier.weight(1f), fontSize = 12.sp)
        Switch(use, { changed(controller.setModifierCurve(index, it, pts)) })
    }
    if (use) CurveEditor("Influence along the stroke", pts, { changed(controller.setModifierCurve(index, true, it)) }, "modifierCurve_$index")
}

/** Transform options of the header: pivot point, proportional editing (O), increment snapping. */
@Composable
fun TransformOptionsSection(controller: EditorController, redraw: () -> Unit) {
    val t = controller.transformSettings
    Text("Transform", Modifier.padding(horizontal = 12.dp, vertical = 4.dp), fontWeight = FontWeight.Bold)
    var pivotMenu by remember { mutableStateOf(false) }
    Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
        Text("Pivot", Modifier.weight(1f))
        Box {
            TextButton(onClick = { pivotMenu = true }, modifier = Modifier.testTag("pivotMenu")) { Text(ProjectGreaseSelect.PIVOT_LABELS[t.pivot]) }
            DropdownMenu(expanded = pivotMenu, onDismissRequest = { pivotMenu = false }) {
                ProjectGreaseSelect.PIVOT_LABELS.forEachIndexed { i, label ->
                    DropdownMenuItem(text = { Text(label) }, modifier = Modifier.testTag("pivot_$i"),
                        onClick = { controller.setPivot(i); pivotMenu = false; redraw() })
                }
            }
        }
    }
    if (t.pivot == ProjectGreaseSelect.PIVOT_CURSOR)
        TextButton(onClick = { controller.placingCursor2D = true }, modifier = Modifier.padding(horizontal = 12.dp).testTag("placeCursor")) {
            Text("Tap canvas to place 2D cursor")
        }
    Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
        Text("Proportional editing (O)", Modifier.weight(1f))
        Switch(checked = t.proportional, onCheckedChange = { controller.toggleProportional(); redraw() }, modifier = Modifier.testTag("proportionalToggle"))
    }
    if (t.proportional) {
        var falloffMenu by remember { mutableStateOf(false) }
        Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("Falloff", Modifier.weight(1f))
            Box {
                val idx = ProjectGreaseSelect.FALLOFF_VALUES.indexOf(t.falloff).coerceAtLeast(0)
                TextButton(onClick = { falloffMenu = true }, modifier = Modifier.testTag("falloffMenu")) { Text(ProjectGreaseSelect.FALLOFF_LABELS[idx]) }
                DropdownMenu(expanded = falloffMenu, onDismissRequest = { falloffMenu = false }) {
                    ProjectGreaseSelect.FALLOFF_VALUES.forEachIndexed { i, v ->
                        DropdownMenuItem(text = { Text(ProjectGreaseSelect.FALLOFF_LABELS[i]) },
                            onClick = { controller.setProportionalFalloff(v); falloffMenu = false; redraw() })
                    }
                }
            }
        }
        Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("Connected only", Modifier.weight(1f))
            Switch(checked = t.connected, onCheckedChange = { controller.setProportionalConnected(it); redraw() })
        }
        Text("Size ${"%.1f".format(t.size)} (pinch while transforming)", Modifier.padding(horizontal = 12.dp))
        Slider(value = t.size.coerceIn(1f, 1000f), onValueChange = { controller.setProportionalSize(it) }, valueRange = 1f..1000f,
            modifier = Modifier.padding(horizontal = 12.dp).testTag("proportionalSize"))
    }
    Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
        Text("Snap: increment (grid)", Modifier.weight(1f))
        Switch(checked = t.snapIncrement > 0f, onCheckedChange = { controller.setSnapIncrement(if (it) controller.view.gridSize else 0f); redraw() },
            modifier = Modifier.testTag("snapIncrement"))
    }
}

/** Dot Dash with a segment list (DashGpencilModifierData.segments): applied to the active frame. */
@Composable
fun DashSegmentsSection(controller: EditorController, redraw: () -> Unit) {
    val segments = remember { mutableStateListOf(3 to 2) }
    var offset by remember { mutableIntStateOf(0) }
    Text("Dash segments", Modifier.padding(horizontal = 12.dp, vertical = 4.dp), fontWeight = FontWeight.Bold)
    segments.forEachIndexed { i, (d, g) ->
        Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("#${i + 1} dash $d gap $g", Modifier.weight(1f))
            TextButton(onClick = { segments[i] = (d + 1) to g }) { Text("D+") }
            TextButton(onClick = { segments[i] = (d - 1).coerceAtLeast(1) to g }) { Text("D-") }
            TextButton(onClick = { segments[i] = d to g + 1 }) { Text("G+") }
            TextButton(onClick = { segments[i] = d to (g - 1).coerceAtLeast(0) }) { Text("G-") }
            if (segments.size > 1) TextButton(onClick = { segments.removeAt(i) }) { Text("x") }
        }
    }
    Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
        TextButton(onClick = { if (segments.size < 32) segments.add(1 to 1) }, modifier = Modifier.testTag("dashAddSegment")) { Text("Add segment") }
        TextButton(onClick = { offset-- }) { Text("Offset -") }
        Text("$offset")
        TextButton(onClick = { offset++ }) { Text("Offset +") }
        TextButton(onClick = { controller.setDashSegments(offset, segments.toList()); redraw() }, modifier = Modifier.testTag("dashApply")) { Text("Apply") }
    }
}

/** Sculpt auto-masking, selection mask and brush falloff curve (Blender's sculpt header / brush panel). */
@Composable
fun SculptMaskingSection(controller: EditorController, redraw: () -> Unit) {
    val sc = controller.sculpt
    var tick by remember { mutableIntStateOf(0) }
    key(tick) {
        Text("Auto-masking", Modifier.padding(horizontal = 20.dp, vertical = 4.dp), fontWeight = FontWeight.Bold)
        Row(Modifier.horizontalScroll(rememberScrollState()).padding(horizontal = 20.dp)) {
            listOf(
                com.smitnk.projectgrease.editor.ToolSession.AUTOMASK_STROKE to "Stroke",
                com.smitnk.projectgrease.editor.ToolSession.AUTOMASK_LAYER_STROKE to "Layer (stroke)",
                com.smitnk.projectgrease.editor.ToolSession.AUTOMASK_MATERIAL_STROKE to "Material (stroke)",
                com.smitnk.projectgrease.editor.ToolSession.AUTOMASK_LAYER_ACTIVE to "Active layer",
                com.smitnk.projectgrease.editor.ToolSession.AUTOMASK_MATERIAL_ACTIVE to "Active material"
            ).forEach { (bit, label) ->
                FilterChip(selected = sc.automask and bit != 0, onClick = { sc.toggleAutomask(bit); tick++; redraw() },
                    label = { Text(label, fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp).testTag("automask_$bit"))
            }
        }
        Text("Selection mask", Modifier.padding(horizontal = 20.dp, vertical = 4.dp), fontWeight = FontWeight.Bold)
        Row(Modifier.horizontalScroll(rememberScrollState()).padding(horizontal = 20.dp)) {
            listOf(0 to "Off", com.smitnk.projectgrease.editor.ToolSession.SELECT_MASK_POINT to "Points",
                com.smitnk.projectgrease.editor.ToolSession.SELECT_MASK_STROKE to "Strokes",
                com.smitnk.projectgrease.editor.ToolSession.SELECT_MASK_SEGMENT to "Segments").forEach { (m, label) ->
                FilterChip(selected = sc.selectMask == m, onClick = { sc.setSelectMask(m); tick++; redraw() },
                    label = { Text(label, fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp))
            }
        }
        Text("Falloff curve", Modifier.padding(horizontal = 20.dp, vertical = 4.dp), fontWeight = FontWeight.Bold)
        Row(Modifier.horizontalScroll(rememberScrollState()).padding(horizontal = 20.dp)) {
            com.smitnk.projectgrease.editor.ToolSession.CURVE_PRESETS.forEach { (preset, label) ->
                FilterChip(selected = sc.curvePreset == preset, onClick = { sc.setCurvePreset(preset); tick++; redraw() },
                    label = { Text(label, fontSize = 10.sp) }, modifier = Modifier.padding(end = 3.dp))
            }
        }
    }
}
