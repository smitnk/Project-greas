package com.smitnk.projectgrease.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.ProjectSettings

/**
 * Project > Settings: canvas size, fps, frame range, background colour and transparency of this
 * project (ProjectSettings, saved in the project file). The canvas, timeline end and exports read it.
 */
@Composable
fun ProjectSettingsDialog(controller: EditorController, onDismiss: () -> Unit, onApplied: () -> Unit) {
    val current = controller.projectSettings
    var width by remember { mutableStateOf(current.width.toString()) }
    var height by remember { mutableStateOf(current.height.toString()) }
    var fps by remember { mutableStateOf(current.fps.toString()) }
    var start by remember { mutableStateOf(current.frameStart.toString()) }
    var end by remember { mutableStateOf(current.frameEnd.toString()) }
    var background by remember { mutableStateOf("#%06X".format(current.background and 0xFFFFFF)) }
    var transparent by remember { mutableStateOf(current.transparentBackground) }
    var error by remember { mutableStateOf<String?>(null) }

    @Composable
    fun number(label: String, value: String, tag: String, onChange: (String) -> Unit) = OutlinedTextField(
        value, { v -> onChange(v.filter { it.isDigit() }.take(5)) }, Modifier.fillMaxWidth().padding(top = 4.dp).testTag(tag),
        label = { Text(label) }, singleLine = true, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number)
    )

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Project settings") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                number("Width (px)", width, "settingsWidth") { width = it }
                number("Height (px)", height, "settingsHeight") { height = it }
                number("Frame rate (FPS)", fps, "settingsFps") { fps = it }
                number("Start frame", start, "settingsStart") { start = it }
                number("End frame", end, "settingsEnd") { end = it }
                OutlinedTextField(background, { background = it.take(7) }, Modifier.fillMaxWidth().padding(top = 4.dp),
                    label = { Text("Background (#RRGGBB)") }, singleLine = true)
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text("Transparent background", Modifier.weight(1f))
                    Switch(transparent, { transparent = it })
                }
                error?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(top = 8.dp)) }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                val s = ProjectSettings().apply {
                    this.width = width.toIntOrNull() ?: -1
                    this.height = height.toIntOrNull() ?: -1
                    this.fps = fps.toIntOrNull() ?: -1
                    this.frameStart = start.toIntOrNull() ?: -1
                    this.frameEnd = end.toIntOrNull() ?: -1
                    this.background = background.removePrefix("#").toLongOrNull(16)?.let { (0xFF000000 or it).toInt() } ?: current.background
                    this.transparentBackground = transparent
                }
                error = controller.applyProjectSettings(s)
                if (error == null) { onApplied(); onDismiss() }
            }, modifier = Modifier.testTag("settingsApply")) { Text("Apply") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } }
    )
}
