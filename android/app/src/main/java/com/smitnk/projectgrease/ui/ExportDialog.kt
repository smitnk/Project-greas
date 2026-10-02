package com.smitnk.projectgrease.ui

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import android.provider.DocumentsContract
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilterChip
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
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.VectorExport

/**
 * PNG export of the current frame: the presenter's own offscreen render at canvas size (what the
 * screen shows: modifiers, masks, effects; no annotations), optionally on a transparent background.
 */
internal fun writePng(context: Context, uri: Uri, argb: IntArray, width: Int, height: Int): Boolean = runCatching {
    val bitmap = Bitmap.createBitmap(argb, width, height, Bitmap.Config.ARGB_8888)
    val ok = context.contentResolver.openOutputStream(uri)?.use { bitmap.compress(Bitmap.CompressFormat.PNG, 100, it) } ?: false
    bitmap.recycle()
    ok
}.getOrDefault(false)

/**
 * SVG / PDF / PNG export through the Storage Access Framework: a single file for the current frame (SVG)
 * or any selection (PDF, one page per frame), or a folder with one SVG per frame for the whole
 * timeline. The files are built by [VectorExport]; see its notes for what is exported.
 */
@Composable
fun ExportDialog(controller: EditorController, context: Context, onDismiss: () -> Unit) {
    var pdf by remember { mutableStateOf(false) }
    var png by remember { mutableStateOf(false) }
    var transparent by remember { mutableStateOf(false) }
    var wholeTimeline by remember { mutableStateOf(false) }
    // Annotations are overlay notes, not part of the drawing: left out unless asked for.
    var includeAnnotations by remember { mutableStateOf(false) }
    val frames = if (wholeTimeline) (1..controller.animation.timelineEnd.coerceAtLeast(1)).toList()
    else listOf(controller.animation.currentFrame)
    val baseName = controller.document.projectName.ifBlank { "Project Grease" }

    fun toast(text: String) = Toast.makeText(context, text, Toast.LENGTH_SHORT).show()

    fun writeBytes(uri: Uri, bytes: ByteArray): Boolean = runCatching {
        context.contentResolver.openOutputStream(uri)?.use { it.write(bytes) } != null
    }.getOrDefault(false)

    val singleFile = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument(if (pdf) "application/pdf" else "image/svg+xml")
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        val pages = controller.exportPages(frames, includeAnnotations)
        val ok = pages.isNotEmpty() && writeBytes(
            uri,
            if (pdf) VectorExport.toPdf(pages) else VectorExport.toSvg(pages.first()).toByteArray(Charsets.UTF_8)
        )
        toast(if (ok) "Exported" else "Export failed")
        if (ok) onDismiss()
    }
    val pngFile = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("image/png")) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        val w = controller.document.canvasWidth
        val h = controller.document.canvasHeight
        val pixels = controller.renderCanvasPixels(transparent)
        val ok = pixels != null && writePng(context, uri, pixels, w, h)
        toast(if (ok) "Exported PNG ${w}×$h" else "PNG export failed")
        if (ok) onDismiss()
    }
    val folder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { tree ->
        if (tree == null) return@rememberLauncherForActivityResult
        val pages = controller.exportPages(frames, includeAnnotations)
        val parent = DocumentsContract.buildDocumentUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree))
        var written = 0
        for (page in pages) {
            val name = "%s_%04d.svg".format(baseName.replace(Regex("[^A-Za-z0-9._-]"), "_"), page.frame)
            val target = runCatching {
                DocumentsContract.createDocument(context.contentResolver, parent, "image/svg+xml", name)
            }.getOrNull() ?: continue
            if (writeBytes(target, VectorExport.toSvg(page).toByteArray(Charsets.UTF_8))) written++
        }
        toast(if (written == pages.size && pages.isNotEmpty()) "Exported $written files" else "Exported $written of ${pages.size} files")
        if (written > 0) onDismiss()
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Export") },
        text = {
            Column {
                Text("Format")
                Row {
                    FilterChip(selected = !pdf && !png, onClick = { pdf = false; png = false }, label = { Text("SVG") }, modifier = Modifier.padding(end = 6.dp))
                    FilterChip(selected = pdf, onClick = { pdf = true; png = false }, label = { Text("PDF") }, modifier = Modifier.padding(end = 6.dp))
                    FilterChip(selected = png, onClick = { png = true; pdf = false }, label = { Text("PNG") })
                }
                if (png) {
                    Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("Transparent background", Modifier.weight(1f))
                        Switch(transparent, { transparent = it })
                    }
                    Text(
                        "The current frame as on screen (modifiers, masks, effects), ${controller.document.canvasWidth}×${controller.document.canvasHeight} px; annotations are not included.",
                        Modifier.fillMaxWidth().padding(top = 8.dp)
                    )
                } else {
                    Text("Frames", Modifier.padding(top = 8.dp))
                    Row {
                        FilterChip(selected = !wholeTimeline, onClick = { wholeTimeline = false }, label = { Text("Current frame") }, modifier = Modifier.padding(end = 6.dp))
                        FilterChip(selected = wholeTimeline, onClick = { wholeTimeline = true }, label = { Text("Timeline (${controller.animation.timelineEnd})") })
                    }
                    Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("Include annotations", Modifier.weight(1f))
                        Switch(includeAnnotations, { includeAnnotations = it })
                    }
                    Text(
                        if (pdf) "One PDF, one page per frame." else if (wholeTimeline) "A folder with one SVG per frame." else "One SVG file.",
                        Modifier.fillMaxWidth().padding(top = 8.dp)
                    )
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                val extension = if (pdf) "pdf" else "svg"
                if (png) pngFile.launch("$baseName.png")
                else if (!pdf && wholeTimeline) folder.launch(null) else singleFile.launch("$baseName.$extension")
            }) { Text("Export") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } }
    )
}
