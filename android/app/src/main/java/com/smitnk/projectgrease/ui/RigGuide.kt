package com.smitnk.projectgrease.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController

/** Plain words, Blender names. Shown from the Rig panel "?" button and Menu > How to rig. */
internal val RIG_GUIDE_STEPS = listOf(
    "Draw the parts on separate layers. One layer for each part: arm, leg, head.",
    "Add Armature. Menu > Rig > Add Armature, or the Rig button on the rail.",
    "In Edit mode, tap Add a bone. Then Extrude bones: drag from the tip of a bone.",
    "Parent: select a layer, then a bone. Use Connected or Keep Offset.",
    "Bind: With Automatic Weights. The layer now follows the bone.",
    "Pose mode: tap a bone. Then use Move or Rotate on the rail.",
    "Insert Key. Then change the frame and pose again.",
    "Copy Pose, go to the next frame, then Paste Pose.",
)

@Composable internal fun RigGuide(onDismiss: () -> Unit) {
    AlertDialog(onDismissRequest = onDismiss, modifier = Modifier.testTag("rigGuide"),
        title = { Text("How to rig") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                RIG_GUIDE_STEPS.forEachIndexed { i, s -> Text("${i + 1}. $s", Modifier.padding(vertical = 4.dp)) }
            }
        },
        confirmButton = { TextButton(onDismiss, Modifier.testTag("rigGuideClose")) { Text("Close") } })
}

/** The same as the Rig panel's Add Armature (object.armature_add, Enter Edit Mode). */
internal fun addArmature(controller: EditorController, redraw: () -> Unit) {
    if (controller.rig.create()) { controller.setActiveObject(true); controller.modeToggle("GPENCIL_OT_editmode_toggle") }
    controller.history.markEdit(); controller.render(); redraw()
}
