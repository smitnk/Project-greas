package com.smitnk.projectgrease.ui

import android.content.Context
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController

/** Longest side of the image handed to the tracer; larger images are subsampled when decoded. */
private const val TRACE_MAX_SIDE = 1024

private fun decodeForTrace(context: Context, uri: Uri): Triple<IntArray, Int, Int>? = runCatching {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    context.contentResolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it, null, bounds) }
    var sample = 1
    while (maxOf(bounds.outWidth, bounds.outHeight) / sample > TRACE_MAX_SIDE) sample *= 2
    val options = BitmapFactory.Options().apply { inSampleSize = sample }
    val bitmap = context.contentResolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it, null, options) }
        ?: return@runCatching null
    val pixels = IntArray(bitmap.width * bitmap.height)
    bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
    Triple(pixels, bitmap.width, bitmap.height).also { bitmap.recycle() }
}.getOrNull()

/**
 * Project > Trace image (after Blender's "Trace Image to Grease Pencil", which uses potrace):
 * threshold, which side to trace and the simplification tolerance; each outline becomes a closed
 * filled stroke on a new "Trace" layer.
 */
@Composable
fun TraceImageDialog(controller: EditorController, context: Context, onDismiss: () -> Unit) {
    var threshold by remember { mutableFloatStateOf(0.5f) }
    var traceBright by remember { mutableStateOf(false) }
    var tolerance by remember { mutableFloatStateOf(0.6f) }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        val image = decodeForTrace(context, uri)
        val count = if (image == null) 0
        else controller.traceImage(image.first, image.second, image.third, threshold, traceBright, tolerance)
        toast(context, if (image == null) "Could not read the image" else if (count > 0) "Traced $count outlines" else "No outlines found")
        if (count > 0) onDismiss()
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Trace image") },
        text = {
            Column {
                Text("Threshold %.2f".format(threshold))
                Slider(threshold, { threshold = it }, valueRange = 0f..1f)
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Text("Trace bright areas", Modifier.weight(1f))
                    Switch(traceBright, { traceBright = it })
                }
                Text("Tolerance %.1f px".format(tolerance), Modifier.padding(top = 8.dp))
                Slider(tolerance, { tolerance = it }, valueRange = 0f..5f)
                Text("Outlines become filled strokes on a new \"Trace\" layer in the active color.", Modifier.padding(top = 8.dp))
            }
        },
        confirmButton = { TextButton(onClick = { picker.launch(arrayOf("image/*")) }) { Text("Pick image") } },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } }
    )
}
