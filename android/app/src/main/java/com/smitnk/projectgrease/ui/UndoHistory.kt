package com.smitnk.projectgrease.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController

/**
 * Blender's Edit > Undo History (ED_undo_history_menu / ED_OT_undo_history): every step by name,
 * the current one checked; tapping a step loads it (BKE_undosys_step_load_from_index) and closes.
 */
@Composable fun UndoHistoryDialog(controller: EditorController, onDismiss: () -> Unit) {
    val names = remember { controller.history.stepNames }
    val active = remember { controller.history.stepIndex }
    AlertDialog(onDismissRequest = onDismiss, title = { Text("Undo History") },
        modifier = Modifier.testTag("undoHistory"),
        text = {
            LazyColumn(Modifier.heightIn(max = 420.dp)) {
                itemsIndexed(names) { i, name ->
                    ListItem(headlineContent = { Text(name) },
                        leadingContent = { if (i == active) Icon(PgIcons.Check, "Current step") },
                        modifier = Modifier.clickable { controller.undoHistoryJump(i); onDismiss() }.testTag("undoStep_$i"))
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text("Close") } })
}
