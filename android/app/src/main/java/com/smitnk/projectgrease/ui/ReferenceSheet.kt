package com.smitnk.projectgrease.ui

import android.content.Context
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController

/** Wireframe of the 3D reference over the canvas, mapped like the EGL surface's canvasPoint(). */
@Composable
internal fun ReferenceOverlay(controller: EditorController, tick: Int) {
    @Suppress("UNUSED_VARIABLE") val observed = tick
    val reference = controller.reference
    if (!reference.visible || reference.isEmpty) return
    val cw = controller.document.canvasWidth.coerceAtLeast(1)
    val ch = controller.document.canvasHeight.coerceAtLeast(1)
    val segments = reference.canvasSegments(cw, ch)
    Canvas(Modifier.fillMaxSize()) {
        val fit = minOf(size.width / cw, size.height / ch) * 0.92f * controller.view.zoom
        val ox = (size.width - cw * fit) * 0.5f + controller.view.panX
        val oy = (size.height - ch * fit) * 0.5f + controller.view.panY
        val color = Color(0x9926A69A)
        var i = 0
        while (i + 3 < segments.size) {
            drawLine(color, Offset(ox + segments[i] * fit, oy + segments[i + 1] * fit),
                Offset(ox + segments[i + 2] * fit, oy + segments[i + 3] * fit), 1.5f)
            i += 4
        }
    }
}

/**
 * 3D reference for Line Art (SPEC_LINE_ART_ARCHITECTURE batch 1): import OBJ meshes, set the
 * camera (perspective lens or orthographic scale, orbit, shift), see the mesh edges through
 * Line Art's camera. Line generation comes in later batches.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun ReferenceSheet(controller: EditorController, context: Context, onDismiss: () -> Unit, redraw: () -> Unit) {
    val reference = controller.reference
    val cw = controller.document.canvasWidth
    val ch = controller.document.canvasHeight
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val observed = tick
    var camera by remember { mutableStateOf(reference.camera) }
    var lineThickness by remember { mutableFloatStateOf(3f) }
    var includeHidden by remember { mutableStateOf(false) }
    fun apply(value: com.smitnk.projectgrease.editor.ReferenceCamera) {
        camera = value
        reference.setCamera(value, cw, ch)
        tick++
        redraw()
    }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        val bytes = runCatching { context.contentResolver.openInputStream(uri)?.use { it.readBytes() } }.getOrNull()
        val added = if (bytes == null) 0 else reference.importObj(bytes, cw, ch)
        toast(context, if (added > 0) "Imported $added objects" else "Not a usable OBJ file")
        tick++
        redraw()
    }
    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 20.dp)) {
            Text("3D reference (Line Art)", style = MaterialTheme.typography.headlineSmall)
            val s = reference.stats()
            Text("${s[0]} objects • ${s[1]} vertices • ${s[2]} triangles • ${s[3]} edges (${s[4]} loose)")
            Text("Preview Line Art's visible lines, then generate them as strokes on a new layer of the current frame.",
                style = MaterialTheme.typography.bodySmall)
            Row(Modifier.padding(vertical = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { picker.launch(arrayOf("model/obj", "text/plain", "application/octet-stream", "*/*")) }) { Text("Import OBJ") }
                OutlinedButton(onClick = { reference.clear(); tick++; redraw() }) { Text("Clear reference") }
            }
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text("Show over canvas", Modifier.weight(1f))
                Switch(reference.visible, { reference.setVisible(it); tick++; redraw() })
            }
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text("Line Art preview")
                    Text(if (reference.lineArtPreview) {
                        val (total, shown) = reference.lineArtCounts()
                        "Visible lines of Blender's Line Art: $shown of $total segments"
                    } else "Off: every mesh edge is shown", style = MaterialTheme.typography.bodySmall)
                }
                Switch(reference.lineArtPreview, { reference.setLineArtPreview(it, cw, ch); tick++; redraw() })
            }
            Text("Line Art strokes", Modifier.padding(top = 8.dp), style = MaterialTheme.typography.titleMedium)
            Text("Line thickness %.0f px".format(lineThickness))
            Slider(lineThickness, { lineThickness = it }, valueRange = 1f..20f)
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text("Include hidden lines (occlusion level 1)", Modifier.weight(1f))
                Switch(includeHidden, { includeHidden = it })
            }
            Button(onClick = {
                val n = controller.generateLineArt(lineThickness, includeHidden)
                toast(context, if (n > 0) "Line Art: $n strokes on layer \"Line Art\"" else "No Line Art lines in view")
                redraw()
            }, enabled = !reference.isEmpty) { Text("Generate Line Art strokes") }
            Text("Camera", Modifier.padding(top = 8.dp), style = MaterialTheme.typography.titleMedium)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(selected = !camera.orthographic, onClick = { apply(camera.copy(orthographic = false)) }, label = { Text("Perspective") })
                FilterChip(selected = camera.orthographic, onClick = { apply(camera.copy(orthographic = true)) }, label = { Text("Orthographic") })
            }
            if (camera.orthographic) {
                Text("Orthographic scale %.2f".format(camera.orthoScale))
                Slider(camera.orthoScale, { apply(camera.copy(orthoScale = it)) }, valueRange = 0.5f..50f)
            } else {
                Text("Focal length %.0f mm".format(camera.lens))
                Slider(camera.lens, { apply(camera.copy(lens = it)) }, valueRange = 10f..200f)
            }
            Text("Orbit %.0f°".format(Math.toDegrees(camera.yaw.toDouble())))
            Slider(camera.yaw, { apply(camera.copy(yaw = it)) }, valueRange = (-Math.PI).toFloat()..Math.PI.toFloat())
            Text("Elevation %.0f°".format(Math.toDegrees(camera.pitch.toDouble())))
            Slider(camera.pitch, { apply(camera.copy(pitch = it)) }, valueRange = -1.5f..1.5f)
            Text("Distance %.1f".format(camera.distance))
            Slider(camera.distance, { apply(camera.copy(distance = it)) }, valueRange = 1f..80f)
            Text("Shift X %.2f".format(camera.shiftX))
            Slider(camera.shiftX, { apply(camera.copy(shiftX = it)) }, valueRange = -1f..1f)
            Text("Shift Y %.2f".format(camera.shiftY))
            Slider(camera.shiftY, { apply(camera.copy(shiftY = it)) }, valueRange = -1f..1f)
            OutlinedButton(onClick = { apply(com.smitnk.projectgrease.editor.ReferenceCamera()) }) { Text("Reset camera") }
            Spacer(Modifier.height(24.dp))
        }
    }
}
