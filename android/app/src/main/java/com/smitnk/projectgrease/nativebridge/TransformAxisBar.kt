package com.smitnk.projectgrease.nativebridge

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.TransformKeys

/** Axis locks (Blender's X / Y / Z, Shift+axis = plane), the orientation and "Type value" (numeric
 *  input) of the Move / Rotate / Scale tools, during and after their drags. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun TransformAxisBar(controller: EditorController, modifier: Modifier = Modifier) {
    if (controller.transformBarMode < 0) return
    var shift by remember { mutableStateOf(false) }
    var keypad by remember { mutableStateOf(false) }
    val axes = controller.transformAxes
    val names = if (View3DUi.active) listOf("X", "Y", "Z") else listOf("X", "Y")
    val min = Modifier.defaultMinSize(minWidth = 48.dp, minHeight = 48.dp)
    Column(modifier.padding(6.dp).background(Color(0xAA303030), RoundedCornerShape(8.dp)).padding(4.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            names.forEachIndexed { i, n ->
                // one axis = lock to it; Shift = the plane of the other two (exclude it)
                val want = if (shift) 7 and (1 shl i).inv() else 1 shl i
                val on = axes == want
                val desc = if (shift) "Lock plane, exclude $n" else "Lock to $n axis"
                FilterChip(selected = on, onClick = { controller.setTransformConstraint(if (on) 0 else want) },
                    label = { Text(if (shift) "Shift $n" else n) },
                    modifier = min.testTag("axisLock_$n").semantics { contentDescription = desc })
            }
            FilterChip(selected = shift, onClick = { shift = !shift }, label = { Text("Shift") },
                modifier = min.testTag("axisShift").semantics { contentDescription = "Shift: plane lock" })
            val local = controller.transformOrient == TransformKeys.ORIENT_LOCAL
            TextButton(onClick = { controller.setTransformConstraint(axes, if (local) TransformKeys.ORIENT_GLOBAL else TransformKeys.ORIENT_LOCAL) },
                modifier = min.testTag("axisOrient").semantics { contentDescription = "Orientation" }) {
                Text(if (local) "Local" else "Global", color = Color.White)
            }
            TextButton(onClick = { keypad = !keypad }, modifier = min.testTag("typeValue").semantics { contentDescription = "Type value" }) {
                Text("Type value", color = Color.White)
            }
        }
        if (!keypad) return@Column
        Text(controller.transformNumText ?: "", color = Color.White, fontSize = 12.sp, modifier = Modifier.testTag("numInputText"))
        // Blender's numinput keys: '-' negates, '=' starts an expression, Tab goes to the next axis
        val rows = listOf(listOf("7", "8", "9", "⌫", "Tab"), listOf("4", "5", "6", "-", "="), listOf("1", "2", "3", "+", "*"),
            listOf("0", ".", "(", ")", "/"))
        rows.forEach { r ->
            Row(horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                r.forEach { k ->
                    val key = when (k) { "⌫" -> TransformKeys.BACKSPACE; "Tab" -> TransformKeys.TAB; else -> k[0].code }
                    val desc = when (k) { "⌫" -> "Backspace"; "Tab" -> "Next axis"; "-" -> "Negate"; "=" -> "Expression"; else -> k }
                    TextButton(onClick = { controller.transformNumKey(key) },
                        modifier = min.testTag("numKey_$k").semantics { contentDescription = desc }) { Text(k, color = Color.White) }
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(2.dp)) {
            TextButton(onClick = { controller.endTransform(cancel = true); keypad = false },
                modifier = min.testTag("numCancel").semantics { contentDescription = "Cancel" }) { Text("Cancel", color = Color.White) }
            TextButton(onClick = { controller.endTransform(); keypad = false },
                modifier = min.testTag("numConfirm").semantics { contentDescription = "Confirm" }) { Text("Confirm", color = Color.White) }
        }
    }
}
