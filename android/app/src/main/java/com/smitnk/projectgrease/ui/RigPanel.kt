package com.smitnk.projectgrease.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.CanvasMapping
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.RigController
import com.smitnk.projectgrease.editor.RigController.RigIds

/*
 * Rigging on the phone: Blender's Properties > Object Data (Armature) / Bone / Bone Constraints, the
 * Pose menu, drawing swaps and the master slider, as one scrolling sheet. Every button is >= 48 dp,
 * labelled for TalkBack, and text wraps (FlowRow) so 200 % font scale stays usable.
 */

/** A 48 dp button with Blender's short label; [tag] is the test tag. */
@Composable private fun Op(label: String, tag: String, enabled: Boolean = true, onClick: () -> Unit) =
    OutlinedButton(onClick = onClick, enabled = enabled, modifier = Modifier.heightIn(min = 48.dp).testTag(tag)) { Text(label) }

@Composable private fun Pick(label: String, selected: Boolean, tag: String, onClick: () -> Unit) =
    FilterChip(selected = selected, onClick = onClick, label = { Text(label) }, modifier = Modifier.heightIn(min = 48.dp).testTag(tag))

@Composable private fun Group(title: String, content: @Composable () -> Unit) {
    Text(title, fontWeight = FontWeight.Bold, modifier = Modifier.padding(top = 12.dp, bottom = 4.dp))
    FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) { content() }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable internal fun RigSheet(controller: EditorController, onDismiss: () -> Unit, redraw: () -> Unit) {
    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp).testTag("rigSheet")) {
            RigPanel(controller, redraw)
        }
    }
}

@Composable internal fun ColumnScope.RigPanel(controller: EditorController, redraw: () -> Unit) {
    var tick by remember { mutableIntStateOf(0) }
    val rig = controller.rig
    /** an edit: history step, canvas redraw, panel refresh */
    fun done(ok: Boolean = true) { if (ok) controller.history.markEdit(); controller.render(); tick++; redraw() }
    tick // re-reads the native rig after each edit

    Text("Armature", style = MaterialTheme.typography.titleLarge)
    if (!rig.exists) {
        Op("Add Armature", "rigAdd") { // object.armature_add with Enter Edit Mode: the same mode switch as the mode chip (leaves Object mode)
            if (rig.create()) { controller.setActiveObject(true); controller.modeToggle("GPENCIL_OT_editmode_toggle") }; done() }
        return
    }
    val bones = rig.bones()
    val active = rig.activeBone
    val mode = rig.mode
    val activeName = bones.getOrNull(active)?.name

    Group("Active object") {
        Pick("Grease Pencil", !controller.rigActive, "rigActiveGp") { controller.setActiveObject(false); done(false) }
        Pick("Armature", controller.rigActive, "rigActiveArmature") { controller.setActiveObject(true); done(false) }
    }

    Group("Bones") {
        bones.forEach { b ->
            Pick(b.name, b.index == active, "rigBone_${b.name}") {
                if (mode == RigController.MODE_POSE) rig.poseSelect(b.index) else rig.editSelect(b.index)
                done()
            }
        }
    }

    // ---- Edit mode (Armature menu) ----
    if (mode == RigController.MODE_EDIT) {
        var name by remember(active, tick) { mutableStateOf(activeName ?: "") }
        Group("Bone") {
            Op("Add", "rigBoneAdd") {
                val cx = controller.document.canvasWidth / 2f; val cy = controller.document.canvasHeight / 2f
                rig.addBone(cx, cy, cx, cy - 100f); done()
            }
            // armature.extrude_move: armed here, the next canvas drag extrudes and puts the new tail under the finger
            Pick("Extrude", rig.extrudeNext, "rigExtrude") { rig.extrudeNext = !rig.extrudeNext; done(false) }
            Op("Delete", "rigDelete", active >= 0) { rig.deleteSelected(); done() }
            Op("Parent: Connected", "rigParentConnected", active >= 0) { rig.parentSet(RigController.PARENT_CONNECTED); done() }
            Op("Parent: Keep Offset", "rigParentOffset", active >= 0) { rig.parentSet(RigController.PARENT_KEEP_OFFSET); done() }
            Op("Clear Parent", "rigParentClear", active >= 0) { rig.parentClear(RigController.CLEAR_PARENT); done() }
        }
        if (active >= 0) Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(name, { name = it }, label = { Text("Name") }, singleLine = true,
                modifier = Modifier.weight(1f).testTag("rigBoneName"))
            Op("Rename", "rigRename") { rig.rename(active, name); done() }
        }
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.heightIn(min = 48.dp)) {
            Text("X-Axis Mirror", Modifier.padding(end = 8.dp))
            Switch(rig.mirrorX, { rig.setMirrorX(it); done() }, Modifier.testTag("rigMirrorX").semantics { contentDescription = "X-Axis Mirror" })
        }
        Text("Select a bone, then the parent last (it becomes active). Extrude: tap Extrude then drag, or long-press a tail joint and drag.",
            style = MaterialTheme.typography.bodySmall)
    }

    // ---- Pose mode (Pose menu) ----
    if (mode == RigController.MODE_POSE) Group("Pose") {
        Op("Insert Key", "rigInsertKey") { rig.insertKey(controller.animation.currentFrame); controller.animation.setFrame(controller.animation.currentFrame); done() }
        Op("Clear Pose", "rigClearPose") { rig.poseClear(RigController.CLEAR_LOCATION or RigController.CLEAR_ROTATION or RigController.CLEAR_SCALE); done() }
        Op("Copy Pose", "rigCopyPose") { rig.poseCopy(); done(false) }
        Op("Paste Pose", "rigPastePose") { rig.posePaste(false); done() }
        Op("Paste X-Flipped", "rigPasteFlipped") { rig.posePaste(true); done() }
    }
    if (mode != RigController.MODE_OBJECT) Group("Drag on canvas") {
        listOf(GreaseTool.MOVE to "Move", GreaseTool.ROTATE to "Rotate", GreaseTool.SCALE to "Scale").forEach { (t, l) ->
            Pick(l, controller.tools.activeTool == t, "rigTool_$l") { controller.selectTool(t); done(false) }
        }
    }
    if (mode == RigController.MODE_POSE) Group("Pivot Point") { // transform_pivot_point
        Pick("Median Point", rig.pivot == RigController.AROUND_MEDIAN, "rigPivotMedian") { rig.pivot = RigController.AROUND_MEDIAN; done(false) }
        Pick("Individual Origins", rig.pivot == RigController.AROUND_INDIVIDUAL, "rigPivotIndividual") { rig.pivot = RigController.AROUND_INDIVIDUAL; done(false) }
    }

    // ---- Object Data: Viewport Display, Parent (bind) ----
    Group("Display As") {
        val front = rig.showInFront
        Pick("Octahedral", rig.displayType == RigController.DISPLAY_OCTAHEDRAL, "rigDisplayOcta") { rig.setDisplay(RigController.DISPLAY_OCTAHEDRAL, front); done() }
        Pick("Stick", rig.displayType == RigController.DISPLAY_STICK, "rigDisplayStick") { rig.setDisplay(RigController.DISPLAY_STICK, front); done() }
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.heightIn(min = 48.dp)) {
            Text("In Front", Modifier.padding(end = 8.dp))
            Switch(front, { rig.setDisplay(rig.displayType, it); done() }, Modifier.testTag("rigInFront").semantics { contentDescription = "In Front" })
        }
    }
    Group("Parent (bind)") {
        Op("With Empty Groups", "rigBindEmpty") { rig.bind(RigController.BIND_EMPTY_GROUPS); done() }
        Op("With Automatic Weights", "rigBindAuto") { rig.bind(RigController.BIND_AUTOMATIC); done() }
        Op("With Envelope Weights", "rigBindEnvelope") { rig.bind(RigController.BIND_ENVELOPE); done() }
    }

    if (active >= 0) ConstraintsPanel(controller, active, bones.map { it.name }, ::done)
    DopeKeysPanel(controller, tick, ::done)
    SwapPanel(controller, tick, ::done)
    SliderPanel(controller, bones.map { it.name }, ::done)
}

private val CON_TYPES = listOf(RigIds.CON_IK to "IK", RigIds.CON_COPY_ROTATION to "Copy Rotation",
    RigIds.CON_COPY_LOCATION to "Copy Location", RigIds.CON_DAMPED_TRACK to "Damped Track",
    RigIds.CON_LIMIT_ROTATION to "Limit Rotation", RigIds.CON_CHILD_OF to "Child Of")
private val SPACES = listOf(RigIds.SPACE_WORLD to "World", RigIds.SPACE_POSE to "Pose",
    RigIds.SPACE_LOCAL_WITH_PARENT to "Local With Parent", RigIds.SPACE_LOCAL to "Local")

/** Bone Constraints tab: add, then target / pole, spaces, influence, IK chain length, remove. */
@Composable private fun ConstraintsPanel(controller: EditorController, bone: Int, names: List<String>, done: (Boolean) -> Unit) {
    val rig = controller.rig
    Group("Add Bone Constraint") {
        CON_TYPES.forEach { (t, l) -> Op(l, "rigConAdd_$t") {
            val i = rig.addConstraint(bone, t)
            // Blender's defaults for a 2D rig: Limit Rotation limits Z (the screen axis)
            if (i >= 0 && t == RigIds.CON_LIMIT_ROTATION) rig.constraint(bone, i).let { v ->
                if (v.size > 6) { val s = v.copyOfRange(1, v.size); s[5] = RigIds.LIMIT_Z.toFloat(); rig.setConstraint(bone, i, s) } }
            done(i >= 0) } }
    }
    for (i in 0 until rig.constraintCount(bone)) {
        val v = rig.constraint(bone, i)
        if (v.size < 6) continue
        val type = v[0].toInt()
        // Child Of returns its inverse matrix after the flag: set takes the settings only
        val settings = v.copyOfRange(1, if (type == RigIds.CON_CHILD_OF) minOf(v.size, 7) else v.size)
        fun set(k: Int, x: Float) { settings[k] = x; done(rig.setConstraint(bone, i, settings)) }
        val label = CON_TYPES.firstOrNull { it.first == type }?.second ?: "Constraint"
        Column(Modifier.fillMaxWidth().padding(top = 8.dp).background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(8.dp)).padding(8.dp)
            .testTag("rigCon_$i")) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(label, fontWeight = FontWeight.Bold, modifier = Modifier.weight(1f))
                Op("Remove", "rigConRemove_$i") { done(rig.removeConstraint(bone, i)) }
            }
            Group("Target") {
                Pick("Armature", false, "rigConTarget_${i}_obj") { done(rig.setConstraintTarget(bone, i, "")) }
                names.filterIndexed { k, _ -> k != bone }.forEach { n -> Pick(n, false, "rigConTarget_${i}_$n") { done(rig.setConstraintTarget(bone, i, n)) } }
            }
            if (type == RigIds.CON_IK) {
                Group("Pole Target") {
                    Pick("None", false, "rigConPole_${i}_none") { done(rig.setConstraintTarget(bone, i, null, pole = true)) }
                    names.filterIndexed { k, _ -> k != bone }.forEach { n -> Pick(n, false, "rigConPole_${i}_$n") { done(rig.setConstraintTarget(bone, i, n, pole = true)) } }
                }
                Group("Chain Length ${settings[6].toInt()}") {
                    Op("-", "rigConChainLess_$i", settings[6] > 0f) { set(6, settings[6] - 1f) }
                    Op("+", "rigConChainMore_$i") { set(6, minOf(255f, settings[6] + 1f)) }
                }
            }
            Group("Owner Space") { SPACES.forEach { (s, l) -> Pick(l, settings[1].toInt() == s, "rigConOwner_${i}_$s") { set(1, s.toFloat()) } } }
            Group("Target Space") { SPACES.forEach { (s, l) -> Pick(l, settings[2].toInt() == s, "rigConTargetSpace_${i}_$s") { set(2, s.toFloat()) } } }
            Text("Influence %.2f".format(settings[0]))
            Slider(settings[0], { settings[0] = it; rig.setConstraint(bone, i, settings); controller.render() },
                onValueChangeFinished = { done(true) },
                modifier = Modifier.heightIn(min = 48.dp).testTag("rigConInfluence_$i").semantics { contentDescription = "$label influence" })
        }
    }
}

/** Dope sheet keys of the active bone: tap a key, move it a frame, set Key > Interpolation Mode. */
@Composable private fun DopeKeysPanel(controller: EditorController, tick: Int, done: (Boolean) -> Unit) {
    val rig = controller.rig
    val bone = rig.activeBone
    if (bone < 0) return
    val keys = controller.animation.boneKeys(bone)
    if (keys.isEmpty()) return
    var sel by remember(bone) { mutableIntStateOf(keys[0].toInt()) }
    Group("Keys") {
        keys.forEach { f -> val fr = f.toInt()
            Pick(fr.toString(), fr == sel, "rigKey_$fr") { sel = fr; controller.animation.setFrame(fr); done(false) } }
    }
    if (sel !in keys.map { it.toInt() }) return
    Group("Key $sel") {
        Op("Move -1", "rigKeyLeft", sel > 1) { if (rig.moveKey(bone, sel, sel - 1) > 0) { sel -= 1; done(true) } }
        Op("Move +1", "rigKeyRight") { if (rig.moveKey(bone, sel, sel + 1) > 0) { sel += 1; done(true) } }
        Op("Delete", "rigKeyDelete") { done(rig.deleteKey(bone, sel) > 0) }
        val ipo = rig.interpolation(bone, sel)
        listOf(RigController.IPO_CONSTANT to "Constant", RigController.IPO_LINEAR to "Linear", RigController.IPO_BEZIER to "Bezier").forEach { (k, l) ->
            Pick(l, ipo == k, "rigIpo_$l") { done(rig.setInterpolation(bone, sel, k) > 0) }
        }
    }
}

/** Drawing swap on the active layer: Time Offset (Fixed Frame) driven by the "Swap.<layer>" bone. */
@Composable private fun SwapPanel(controller: EditorController, tick: Int, done: (Boolean) -> Unit) {
    val rig = controller.rig
    tick // the swap bone is native state: recompose after each edit, or the chips never replace "Add Swap"
    val layer = controller.selectedLayer
    val name = controller.layerName(layer)
    val hasSwap = rig.findBone("Swap.$name") >= 0
    Group("Swap ($name)") {
        if (!hasSwap) Op("Add Swap", "rigSwapAdd") { done(addSwap(controller, layer) >= 0) }
        else (1..controller.animation.keyframes.size.coerceIn(1, 12)).forEach { n ->
            Op("$n", "rigSwap_$n") {
                // drawing n is the layer's n-th key frame (Fixed Frame shows that frame)
                val frame = controller.animation.keyframes.getOrNull(n - 1) ?: n
                done(rig.setSwap(name, frame))
            }
        }
    }
    if (hasSwap) Text("Key the swap: Pose mode, select Swap.$name, Insert Key.", style = MaterialTheme.typography.bodySmall)
}

/** Adds the Time Offset modifier in Fixed Frame mode and the swap driver; returns the driver or -1. */
internal fun addSwap(controller: EditorController, layer: Int): Int {
    val rig = controller.rig
    if (!controller.addModifier(ModifierType.TIME, layer)) return -1
    val mod = controller.modifiers(layer).indexOfLast { it.type == ModifierType.TIME }
    controller.setModifierParam(mod, 0, 2f, layer = layer) // Mode: Fixed Frame
    return rig.addSwap(controller.layerName(layer), mod, 80f, controller.document.canvasHeight - 80f)
}

/** Master slider: a custom property on the armature driving bones and swaps. */
@Composable private fun SliderPanel(controller: EditorController, names: List<String>, done: (Boolean) -> Unit) {
    val rig = controller.rig
    var newName by remember { mutableStateOf("Smile") }
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(top = 12.dp)) {
        OutlinedTextField(newName, { newName = it }, label = { Text("Slider") }, singleLine = true, modifier = Modifier.weight(1f).testTag("rigSliderName"))
        Op("Add Slider", "rigSliderAdd", newName.isNotBlank()) { done(rig.addProperty(newName, value = 0f, min = 0f, max = 1f, default = 0f) >= 0) }
    }
    for (p in 0 until rig.propertyCount) {
        val info = rig.property(p)
        if (info.size < 5 || info[0].toInt() != -1) continue
        val pname = sliderName(controller, p) ?: continue
        Text("$pname %.2f".format(info[1]), fontWeight = FontWeight.Bold, modifier = Modifier.padding(top = 8.dp))
        Slider(info[1], { rig.setProperty(p, it); controller.overlayTick++; controller.render() },
            valueRange = info[2]..info[3].coerceAtLeast(info[2] + 1e-3f), onValueChangeFinished = { done(true) },
            modifier = Modifier.heightIn(min = 48.dp).testTag("rigSlider_$pname").semantics { contentDescription = "$pname slider" })
        var target by remember(p) { mutableStateOf(names.firstOrNull() ?: "") }
        var amount by remember(p) { mutableStateOf("1") }
        Group("Link $pname to") {
            names.forEach { n -> Pick(n, n == target, "rigLinkBone_${p}_$n") { target = n } }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(amount, { amount = it }, label = { Text("Amount at 1") }, singleLine = true,
                modifier = Modifier.widthIn(max = 160.dp).testTag("rigLinkAmount_$p"))
        }
        val k = amount.toFloatOrNull() ?: 1f
        Group("Channel") {
            Op("Loc X", "rigLinkLocX_$p") { done(rig.linkSlider(pname, rig.bonePath(target, "location"), 0, k) > 0) }
            Op("Loc Y", "rigLinkLocY_$p") { done(rig.linkSlider(pname, rig.bonePath(target, "location"), 1, k) > 0) }
            Op("Rotation", "rigLinkRot_$p") { done(rig.linkSlider(pname, rig.bonePath(target, "rotation_quaternion"), 0, k, rotation = true) > 0) }
            Op("Scale", "rigLinkScale_$p") { done(rig.linkSlider(pname, rig.bonePath(target, "scale"), 1, k) > 0) }
            if (target.startsWith("Swap.")) Op("Swap", "rigLinkSwap_$p") {
                val layer = target.removePrefix("Swap.")
                val mod = (0 until controller.layerCount()).firstOrNull { controller.layerName(it) == layer }
                    ?.let { l -> controller.modifiers(l).indexOfLast { it.type == ModifierType.TIME } } ?: -1
                done(mod >= 0 && rig.linkSlider(pname, rig.timeOffsetPath(layer, mod), 0, k) > 0)
            }
        }
    }
}

/** Property names come from the saved rig JSON (the native query returns numbers only). */
private fun sliderName(controller: EditorController, index: Int): String? = runCatching {
    org.json.JSONObject(controller.rig.json()).getJSONArray("props").getJSONObject(index).getString("name")
}.getOrNull()

/** Pose / Edit handle: a ring at the active bone's head (the rotate / scale pivot) and a line to the tip. */
@Composable internal fun RigHandleOverlay(controller: EditorController, modifier: Modifier) {
    if (!controller.rigTouch) return
    Canvas(modifier.testTag("rigHandle")) {
        controller.overlayTick
        val rig = controller.rig
        val b = rig.bone(rig.activeBone) ?: return@Canvas
        val edit = rig.mode == RigController.MODE_EDIT
        val doc = controller.document
        fun toView(x: Float, y: Float) = CanvasMapping.toView(x, y, size.width, size.height, doc.canvasWidth, doc.canvasHeight,
            controller.view.zoom, controller.view.panX, controller.view.panY, controller.view.rotation)
        val (hx, hy) = if (edit) toView(b.headX, b.headY) else toView(b.poseHeadX, b.poseHeadY)
        val (tx, ty) = if (edit) toView(b.tailX, b.tailY) else toView(b.poseTailX, b.poseTailY)
        val c = Color(0xFF4FC3F7)
        drawCircle(c, 24.dp.toPx(), Offset(hx, hy), style = Stroke(width = 3f))
        drawLine(c, Offset(hx, hy), Offset(tx, ty), 2f)
        drawCircle(c, 6.dp.toPx(), Offset(tx, ty))
    }
}

/**
 * Dope sheet rows of the rig: one row per bone with its keys drawn like the GP keys (keyTypeColor /
 * keyTypeMark, Keyframe type). Tap a key: select it and go to its frame; then Move -1 / +1 (Grab) and
 * Key > Interpolation Mode.
 */
@Composable internal fun BoneKeyRows(controller: EditorController, redraw: () -> Unit) {
    val rig = controller.rig
    if (!rig.exists) return
    var tick by remember { mutableIntStateOf(0) }
    var selBone by remember { mutableIntStateOf(-1) }
    var selFrame by remember { mutableIntStateOf(-1) }
    tick
    fun done(ok: Boolean) { if (ok) controller.history.markEdit(); controller.render(); tick++; redraw() }
    Column(Modifier.fillMaxWidth().padding(horizontal = 5.dp).testTag("boneKeyRows")) {
        rig.bones().forEach { b ->
            val keys = controller.animation.boneKeys(b.index)
            if (keys.isEmpty()) return@forEach
            Row(Modifier.horizontalScroll(rememberScrollState()), verticalAlignment = Alignment.CenterVertically) {
                Text(b.name, Modifier.widthIn(min = 72.dp).padding(end = 6.dp), maxLines = 1)
                keys.forEach { f -> val fr = f.toInt(); val sel = b.index == selBone && fr == selFrame
                    FilterChip(selected = sel, onClick = { selBone = b.index; selFrame = fr; controller.animation.setFrame(fr); done(false) },
                        label = { Text("$fr " + keyTypeMark(0)) },
                        leadingIcon = { androidx.compose.foundation.layout.Box(Modifier.background(keyTypeColor(0), RoundedCornerShape(3.dp)).padding(4.dp)) },
                        modifier = Modifier.heightIn(min = 48.dp).padding(end = 4.dp).testTag("boneKey_${b.name}_$fr")
                            .semantics { contentDescription = "${b.name} key at frame $fr" })
                }
            }
        }
        if (selBone >= 0 && selFrame >= 0) Group("Key ${rig.bone(selBone)?.name ?: ""} $selFrame") {
            Op("Move -1", "boneKeyLeft", selFrame > 1) { if (rig.moveKey(selBone, selFrame, selFrame - 1) > 0) { selFrame -= 1; done(true) } }
            Op("Move +1", "boneKeyRight") { if (rig.moveKey(selBone, selFrame, selFrame + 1) > 0) { selFrame += 1; done(true) } }
            val ipo = rig.interpolation(selBone, selFrame)
            listOf(RigController.IPO_CONSTANT to "Constant", RigController.IPO_LINEAR to "Linear", RigController.IPO_BEZIER to "Bezier").forEach { (k, l) ->
                Pick(l, ipo == k, "boneKeyIpo_$l") { done(rig.setInterpolation(selBone, selFrame, k) > 0) }
            }
        }
    }
}
