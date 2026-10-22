@file:OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)

package com.smitnk.projectgrease.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import com.smitnk.projectgrease.editor.SculptBrush

/** One operator: what it is called, what it does in a line, and the call. */
private class Op(val label: String, val help: String, val run: () -> Boolean)

/**
 * Blender's Legacy GP stroke operators, grouped like Blender's Edit-mode menus (Stroke, Points,
 * Cleanup, Paint, Arrange). They act on the selection, so without one the panel says how to make
 * one instead of showing buttons that seem to do nothing. Operators with a setting in Blender
 * (Simplify steps, Resample length, Merge distance, Normalize value, Array / Multiply counts) get it.
 */
@Composable
fun SelectionActionsPanel(controller: EditorController, redraw: () -> Unit) {
    var tick by remember { mutableIntStateOf(0) }
    var open by remember { mutableStateOf("Stroke") }
    var simplifySteps by remember { mutableIntStateOf(1) }
    var resample by remember { mutableFloatStateOf(8f) }
    var mergeDist by remember { mutableFloatStateOf(2f) }
    var normThick by remember { mutableFloatStateOf(4f) }
    var normOpacity by remember { mutableFloatStateOf(1f) }
    var copies by remember { mutableIntStateOf(3) }
    @Suppress("UNUSED_VARIABLE") val observed = tick
    val hasSelection = controller.selectionBounds() != null
    Column(Modifier.fillMaxWidth().padding(horizontal = 12.dp).testTag("selectionActions")) {
        Text("Selection actions", fontWeight = FontWeight.Bold)
        if (!hasSelection) {
            Text("Select strokes first: Lasso on the toolbar, or tap strokes in Edit mode. These actions change only the selection.",
                fontSize = 12.sp, modifier = Modifier.padding(vertical = 6.dp).testTag("selectionActionsHint"))
            return@Column
        }
        val groups = listOf(
            "Stroke" to listOf(
                Op("Flip direction", "Reverse the point order (where the stroke starts)") { controller.flipSelection() },
                Op("Toggle closed", "Close an open stroke or open a closed one") { controller.setSelectionCyclic(S.CYCLIC_TOGGLE) },
                Op("Toggle caps", "Round or flat stroke ends") { controller.toggleSelectionCaps() },
                Op("Set start point", "The selected point becomes the start of the closed stroke") { controller.setSelectionStartPoint() },
                Op("Join", "Connect the selected strokes into one") { controller.joinSelection() },
                Op("Join and copy", "A new stroke joining the selected ones; the originals stay") { controller.joinCopySelection() },
                Op("Curve edit (Bézier)", "Edit the selected strokes as curves: drag points and handles") { controller.beginBezierEdit() },
                Op("Merge shapes", "The selected strokes become one solid shape (outline traced, filled in the first stroke's colour)") { controller.mergeShapes() },
                Op("Merge as one line (Blender)", "Chain the selected points of open strokes into one new line (active material); the old strokes go") { controller.mergeSelectedStrokes() },
                Op("Merge by points", "One new line through the selected points in angle order around their centre (for scattered points, not whole shapes)") { controller.mergeSelectedStrokes(byPoint = true) },
                Op("Split", "Cut the selected points out as their own stroke") { controller.splitSelection() },
                Op("Extrude ends", "Add a point at the selected end, ready to drag") { controller.extrudeSelection() },
                Op("Outline", "Turn the stroke into the closed outline of its thick shape") { controller.outlineSelection() },
                Op("Dash (3 on / 2 off)", "Break the stroke into dashes") { controller.dashSelection() },
                Op("Trim at intersection", "Cut the ends past the first crossing (the last tapped stroke)") { controller.trimSelectedStrokeToIntersection() },
            ),
            "Points" to listOf(
                Op("Subdivide", "Add a point between every two points (the last tapped stroke)") { controller.subdivideSelectedStroke(1) },
                Op("Simplify ($simplifySteps step${if (simplifySteps > 1) "s" else ""})", "Remove every second point, this many times") { controller.simplifySelectionFixed(simplifySteps) },
                Op("Simplify (adaptive)", "Fewer points where the line keeps its shape") { controller.simplifySelectionAdaptive() },
                Op("Smooth", "Smooth the selected strokes' shape and thickness") { controller.smoothSelectedStroke() },
                Op("Resample (${resample.toInt()} px)", "Even point spacing of this length") { controller.sampleSelection(resample) },
                Op("Merge by distance (${"%.1f".format(mergeDist)} px)", "Join points closer than this") { controller.mergeSelectionByDistance(mergeDist) },
                Op("Dissolve points", "Delete the selected points, keep the stroke") { controller.dissolveSelection(S.DISSOLVE_POINTS) },
                Op("Dissolve between", "Delete the points between selected ones") { controller.dissolveSelection(S.DISSOLVE_BETWEEN) },
                Op("Dissolve unselected", "Keep only the selected points") { controller.dissolveSelection(S.DISSOLVE_UNSELECT) },
                Op("Snap to grid", "Move the selected points onto the grid") { controller.snapSelectionToGrid() },
            ),
            "Thickness & opacity" to listOf(
                Op("Normalize thickness (${normThick.toInt()})", "Same thickness on every selected point") { controller.normalizeSelection(S.NORMALIZE_THICKNESS, normThick) },
                Op("Normalize opacity (${(normOpacity * 100).toInt()}%)", "Same opacity on every selected point") { controller.normalizeSelection(S.NORMALIZE_OPACITY, normOpacity) },
                Op("Thickness x2 by weight", "Thicker where the vertex group weight is high") { controller.applyThicknessModifierWithWeights(2f) },
            ),
            "Colour" to listOf(
                Op("Assign active material", "Give the selection the current material") { controller.assignActiveMaterialToSelection() },
                Op("Set vertex colour", "Paint the selection with the current colour") { controller.setSelectionVertexColor() },
                Op("Reset vertex colour", "Back to the material colour") { controller.resetSelectionVertexColor() },
                Op("Invert vertex colour", "Opposite colours") { controller.invertSelectionVertexColor() },
                Op("Brighter", "Vertex colour brightness +10%") { controller.selectionVertexColorBrightnessContrast(0.1f, 0f) },
                Op("More contrast", "Vertex colour contrast +20%") { controller.selectionVertexColorBrightnessContrast(0f, 0.2f) },
                Op("Hue +30°", "Shift the vertex colour hue") { controller.selectionVertexColorHsv(h = 0.5f + 1f / 12f) },
                Op("Levels x0.8", "Darken the vertex colour (Levels, gain 0.8)") { controller.selectionVertexColorLevels(0f, 0.8f) },
                Op("Randomize colour", "A random colour per stroke") { controller.randomizeSelectedStrokeColor() },
                Op("Select same colour", "Select every stroke with this vertex colour") { controller.selectByVertexColor() },
            ),
            "Copies & order" to listOf(
                Op("Duplicate", "Copy the selection in place") { controller.duplicateSelection() },
                Op("Mirror copy X", "A mirrored copy, left-right") { controller.mirrorSelectionCopy(true, false) },
                Op("Mirror copy Y", "A mirrored copy, up-down") { controller.mirrorSelectionCopy(false, true) },
                Op("Mirror copy X+Y", "A copy mirrored both ways") { controller.mirrorSelectionCopy(true, true) },
                Op("Array ($copies copies)", "Copies in a row, 60 px apart") { controller.arraySelection(copies) },
                Op("Multiply ($copies copies)", "Copies stacked close together (sketchy lines)") { controller.multiplySelection(copies) },
                Op("Bring to front", "Draw above every other stroke") { controller.arrangeSelection(S.ARRANGE_TOP) },
                Op("Bring forward", "One step up") { controller.arrangeSelection(S.ARRANGE_UP) },
                Op("Send backward", "One step down") { controller.arrangeSelection(S.ARRANGE_DOWN) },
                Op("Send to back", "Draw below every other stroke") { controller.arrangeSelection(S.ARRANGE_BOTTOM) },
                Op("Separate to new layer", "Move the selection to its own layer") { controller.separateSelectionToLayer() },
                Op("Copy", "To the clipboard") { controller.copySelection() },
                Op("Paste", "From the clipboard") { controller.pasteStrokes() },
                Op("Paste by layer", "Each stroke back to the layer it was copied from") { controller.pasteStrokes(byLayer = true) },
                Op("Paste on back", "Behind all strokes") { controller.pasteStrokes(onBack = true) },
                Op("Delete", "Remove the selected points") { controller.deleteSelectedPoints() },
            ),
            "Select" to listOf(
                Op("Select random 50%", "Keep half of the selection, at random") { controller.selectRandom() },
                Op("Snap to 2D cursor", "Move the selected strokes so they start on the 2D cursor") { controller.snapSelectionToCursor() },
                Op("Snap points to 2D cursor", "Every selected point onto the 2D cursor") { controller.snapSelectionToCursor(keepShape = false) },
                Op("2D cursor to selection", "Put the 2D cursor at the selection's centre") { controller.snapCursorToSelection() },
                Op("Reset fill transform", "Fill texture offset, rotation and scale back to default") { controller.resetFillTransform() },
                Op("Smooth group weights", "Blend the active vertex group's weights along the strokes") { controller.smoothGroupWeights() },
                Op("Normalize all groups", "Each point's weights over every group sum to 1") { controller.normalizeAllGroups() },
                Op("Select same vertex colour", "Add points painted the same hue as the selected ones") { controller.selectSameVertexColor() },
                Op("Clean loose points", "Delete strokes of one or two points") { controller.cleanLoosePoints() },
                Op("Clean fill boundaries", "Delete the no-fill boundary strokes of this frame") { controller.cleanFillBoundaries() },
            ),
            "Materials" to listOf(
                Op("Select by material", "Select the strokes of the active material") { controller.selectByActiveMaterial() },
                Op("Pick material", "Make the selected stroke's material active") { controller.pickMaterialFromSelection() },
                Op("Hide material", "Hide the active material") { controller.hideActiveMaterial() },
                Op("Hide other materials", "Hide every material but the active one") { controller.hideActiveMaterial(others = true) },
                Op("Show all materials", "Reveal hidden materials") { controller.revealMaterials() },
                Op("Isolate material", "Lock and hide the other materials; again to restore") { controller.isolateMaterial() },
                Op("Lock all materials", "No stroke can be edited") { controller.lockAllMaterials() },
                Op("Unlock all materials", "Every material editable again") { controller.unlockAllMaterials() },
                Op("Lock unused materials", "Lock materials no selected stroke uses") { controller.lockUnusedMaterials() },
                Op("Lock materials unused by layer", "Lock and hide materials the active layer does not use here") { controller.lockMaterialsUnusedByLayer() },
                Op("Merge similar materials", "Strokes of near-identical materials share one") { controller.mergeSimilarMaterials() },
                Op("Materials to vertex colour", "Material colours become vertex colours and palette swatches; unused materials removed") { controller.materialsToVertexColor() },
                Op("Palette from vertex colours", "Every vertex colour in use becomes a palette swatch") { controller.extractPaletteFromVertexColors() > 0 },
            ),
            "Layers" to listOf(
                Op("Hide layer", "Hide the active layer") { controller.hideLayers() },
                Op("Hide other layers", "Hide every layer but the active one") { controller.hideLayers(others = true) },
                Op("Show all layers", "Reveal hidden layers") { controller.revealLayers() },
                Op("Merge all layers", "Combine every layer into the active one") { controller.mergeAllLayers() },
                Op("New layer", "Add a layer and make it active") { controller.changeLayer(-1) },
                Op("Apply layer thickness", "Bake the layer's thickness change into its strokes") { controller.applyLayerThickness() },
                Op("Delete breakdowns", "Remove the interpolated frames around this frame") { controller.deleteBreakdowns() },
                Op("Recalculate geometry", "Rebuild every stroke's fill and lengths") { controller.recalcGeometry() },
                Op("Delete this frame (all layers)", "Remove the drawing shown here from every editable layer") { controller.deleteActiveFramesAllLayers() },
            ),
        )
        for ((name, ops) in groups) {
            Row(Modifier.fillMaxWidth().clickable { open = if (open == name) "" else name }.padding(vertical = 6.dp).testTag("opGroup_$name"),
                verticalAlignment = Alignment.CenterVertically) {
                Text((if (open == name) "▾ " else "▸ ") + name, fontWeight = FontWeight.SemiBold, modifier = Modifier.weight(1f))
                Text("${ops.size}", fontSize = 11.sp)
            }
            if (open != name) continue
            when (name) {
                "Points" -> {
                    Text("Simplify steps $simplifySteps", fontSize = 11.sp)
                    Slider(simplifySteps.toFloat(), { simplifySteps = it.toInt() }, valueRange = 1f..5f, steps = 3)
                    Text("Resample length ${resample.toInt()} px", fontSize = 11.sp)
                    Slider(resample, { resample = it }, valueRange = 2f..50f)
                    Text("Merge distance ${"%.1f".format(mergeDist)} px", fontSize = 11.sp)
                    Slider(mergeDist, { mergeDist = it }, valueRange = 0.5f..20f)
                }
                "Thickness & opacity" -> {
                    Text("Thickness ${normThick.toInt()}", fontSize = 11.sp)
                    Slider(normThick, { normThick = it }, valueRange = 1f..50f)
                    Text("Opacity ${(normOpacity * 100).toInt()}%", fontSize = 11.sp)
                    Slider(normOpacity, { normOpacity = it }, valueRange = 0f..1f)
                }
                "Copies & order" -> {
                    Text("Copies $copies", fontSize = 11.sp)
                    Slider(copies.toFloat(), { copies = it.toInt() }, valueRange = 2f..10f, steps = 7)
                }
            }
            ops.forEach { op ->
                Row(Modifier.fillMaxWidth().padding(vertical = 2.dp), verticalAlignment = Alignment.CenterVertically) {
                    OutlinedButton(onClick = { if (op.run()) redraw(); tick++ }, Modifier.widthIn(min = 150.dp).testTag("op_${op.label}")) {
                        Text(op.label, fontSize = 12.sp)
                    }
                    Text(op.help, fontSize = 11.sp, modifier = Modifier.padding(start = 8.dp).weight(1f))
                }
            }
        }
    }
}

/** Sculpt mode's own options (Blender's Sculpt tool settings): brush, size, strength, invert, masks. */
@Composable
fun SculptOptions(controller: EditorController, redraw: () -> Unit) {
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val observed = tick
    Column(Modifier.fillMaxWidth().padding(horizontal = 12.dp).testTag("sculptOptions")) {
        Text("Sculpt brush", fontWeight = FontWeight.Bold)
        FlowRow(itemVerticalAlignment = Alignment.CenterVertically) {
            listOf(SculptBrush.SMOOTH to "Smooth", SculptBrush.THICKNESS to "Thickness", SculptBrush.STRENGTH to "Strength",
                SculptBrush.RANDOMIZE to "Randomize", SculptBrush.GRAB to "Grab", SculptBrush.PUSH to "Push",
                SculptBrush.TWIST to "Twist", SculptBrush.PINCH to "Pinch", SculptBrush.CLONE to "Clone").forEach { (b, l) ->
                FilterChip(selected = controller.sculpt.brush == b, onClick = { controller.sculpt.select(b); tick++; redraw() },
                    label = { Text(l) }, modifier = Modifier.padding(end = 4.dp).testTag("sculptBrush_${b.name}"))
            }
        }
        Text(when (controller.sculpt.brush) {
            SculptBrush.SMOOTH -> "Smooths wobbly lines under the brush"
            SculptBrush.THICKNESS -> "Makes lines thicker (Invert: thinner)"
            SculptBrush.STRENGTH -> "Makes lines more opaque (Invert: fainter)"
            SculptBrush.RANDOMIZE -> "Jitters the points for a rough look"
            SculptBrush.GRAB -> "Drags the points under the brush"
            SculptBrush.PUSH -> "Pushes points along the drag"
            SculptBrush.TWIST -> "Turns points around the brush (Invert: other way)"
            SculptBrush.PINCH -> "Pulls points together (Invert: apart)"
            SculptBrush.CLONE -> "Pastes the copied strokes where you touch"
        }, fontSize = 11.sp)
        if (controller.sculpt.brush == SculptBrush.SMOOTH || controller.sculpt.brush == SculptBrush.RANDOMIZE) {
            // the brush's Affect toggles (sculpt_mode_flag)
            FlowRow(itemVerticalAlignment = Alignment.CenterVertically) {
                Text("Affect", fontWeight = FontWeight.Bold, fontSize = 11.sp, modifier = Modifier.padding(end = 6.dp))
                listOf(com.smitnk.projectgrease.editor.ToolSession.AFFECT_POSITION to "Position",
                    com.smitnk.projectgrease.editor.ToolSession.AFFECT_STRENGTH to "Strength",
                    com.smitnk.projectgrease.editor.ToolSession.AFFECT_THICKNESS to "Thickness",
                    com.smitnk.projectgrease.editor.ToolSession.AFFECT_UV to "UV").forEach { (bit, l) ->
                    FilterChip(selected = controller.sculpt.affect and bit != 0, onClick = { controller.sculpt.toggleAffect(bit); tick++ },
                        label = { Text(l, fontSize = 11.sp) }, modifier = Modifier.padding(end = 4.dp).testTag("sculptAffect_$bit"))
                }
            }
        }
        Text("Size ${controller.sculptBrush.size.toInt()} px")
        Slider(controller.sculptBrush.size, { controller.sculptBrush.setSize(it); tick++ }, valueRange = 1f..500f, modifier = Modifier.testTag("sculptSize"))
        Text("Strength ${(controller.sculptBrush.strength * 100).toInt()}%")
        Slider(controller.sculptBrush.strength, { controller.sculptBrush.setStrength(it); tick++ }, valueRange = 0f..1f)
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Invert sculpt brush (thinner, weaker, twist back)", Modifier.weight(1f))
            Switch(controller.sculpt.invert, { controller.sculpt.setInvert(it); tick++; redraw() })
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Multiframe falloff (selected frames far from this one change less)", Modifier.weight(1f))
            Switch(controller.sculpt.frameFalloff, { controller.sculpt.frameFalloff = it; tick++; redraw() }, Modifier.testTag("sculptFrameFalloff"))
        }
        SculptMaskingSection(controller, redraw)
    }
}
