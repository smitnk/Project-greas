package com.smitnk.projectgrease.ui

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import android.provider.DocumentsContract
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.rememberScrollState
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
import com.smitnk.projectgrease.editor.GifEncoder
import com.smitnk.projectgrease.editor.VectorExport
import com.smitnk.projectgrease.editor.VectorExportOptions

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
 * Animated GIF of the project's frame range (project settings start..end, fps; holds show the
 * previous keyframe), each frame rendered offscreen like PNG export. Returns the number of frames.
 */
internal fun writeGif(context: Context, uri: Uri, controller: EditorController, transparent: Boolean, dither: Boolean = false): Int = runCatching {
    var frames = 0
    context.contentResolver.openOutputStream(uri)?.use { out ->
        val gif = GifEncoder(java.io.BufferedOutputStream(out), controller.document.canvasWidth, controller.document.canvasHeight,
            controller.projectSettings.fps, dither)
        gif.begin()
        val ok = controller.renderExportFrames(transparent) { _, px -> gif.addFrame(px); frames++ }
        gif.finish()
        if (!ok) frames = 0
    }
    frames
}.getOrDefault(0)

/** PNG sequence (name_0001.png ...) of the project's frame range into a SAF folder; files written. */
internal fun writePngSequence(context: Context, tree: Uri, controller: EditorController, transparent: Boolean): Int {
    val parent = DocumentsContract.buildDocumentUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree))
    var written = 0
    controller.renderExportFrames(transparent) { item, px ->
        val target = runCatching {
            DocumentsContract.createDocument(context.contentResolver, parent, "image/png", item.fileName)
        }.getOrNull()
        if (target != null && writePng(context, target, px, controller.document.canvasWidth, controller.document.canvasHeight)) written++
    }
    return written
}

/**
 * SVG / PDF / PNG export through the Storage Access Framework: a single file for the current frame (SVG)
 * or any selection (PDF, one page per frame), or a folder with one SVG per frame for the whole
 * timeline. The files are built by [VectorExport]; see its notes for what is exported.
 */
@Composable
fun ExportDialog(controller: EditorController, context: Context, onDismiss: () -> Unit) {
    var pdf by remember { mutableStateOf(false) }
    var png by remember { mutableStateOf(false) }
    // Animation formats over the project settings' frame range: GIF or a PNG sequence.
    var gif by remember { mutableStateOf(false) }
    var sequence by remember { mutableStateOf(false) }
    var mp4 by remember { mutableStateOf(false) }
    var transparent by remember { mutableStateOf(false) }
    var wholeTimeline by remember { mutableStateOf(false) }
    // Annotations are overlay notes, not part of the drawing: left out unless asked for.
    var includeAnnotations by remember { mutableStateOf(false) }
    // Blender export options: frame_start..frame_end range, use_clip_camera, selected-only (layers here)
    var frameRange by remember { mutableStateOf(false) }
    var clipToCanvas by remember { mutableStateOf(false) }
    var selectedLayersOnly by remember { mutableStateOf(false) }
    val layerNames = remember { controller.exportLayerNames() }
    var chosenLayers by remember { mutableStateOf(setOf(controller.selectedLayer)) }
    var dither by remember { mutableStateOf(false) }
    val settings = controller.projectSettings
    val frames = if (frameRange) VectorExport.frameRange(settings.frameStart.coerceAtLeast(1), settings.frameEnd)
    else if (wholeTimeline) (1..controller.animation.timelineEnd.coerceAtLeast(1)).toList()
    else listOf(controller.animation.currentFrame)
    val exportOptions = VectorExportOptions(if (selectedLayersOnly) chosenLayers else null, clipToCanvas)
    val baseName = controller.document.projectName.ifBlank { "Project Grease" }

    fun toast(text: String) = Toast.makeText(context, text, Toast.LENGTH_SHORT).show()

    fun writeBytes(uri: Uri, bytes: ByteArray): Boolean = runCatching {
        context.contentResolver.openOutputStream(uri)?.use { it.write(bytes) } != null
    }.getOrDefault(false)

    val singleFile = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument(if (pdf) "application/pdf" else "image/svg+xml")
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        val pages = controller.exportPages(frames, includeAnnotations, exportOptions)
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
        val pixels = controller.exportCanvasPixels(transparent)
        val ok = pixels != null && writePng(context, uri, pixels, w, h)
        toast(if (ok) "Exported PNG ${w}×$h" else "PNG export failed")
        if (ok) onDismiss()
    }
    val gifFile = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("image/gif")) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        val n = writeGif(context, uri, controller, transparent, dither)
        toast(if (n > 0) "Exported GIF, $n frames" else "GIF export failed")
        if (n > 0) onDismiss()
    }
    val mp4File = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("video/mp4")) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        val n = runCatching {
            context.contentResolver.openFileDescriptor(uri, "rw")?.use { VideoExport.exportToDescriptor(controller, it.fileDescriptor) } ?: 0
        }.getOrDefault(0)
        toast(if (n > 0) "Exported MP4, $n frames" else "MP4 export failed")
        if (n > 0) onDismiss()
    }
    val sequenceFolder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { tree ->
        if (tree == null) return@rememberLauncherForActivityResult
        val n = writePngSequence(context, tree, controller, transparent)
        toast(if (n > 0) "Exported $n PNG files" else "PNG sequence export failed")
        if (n > 0) onDismiss()
    }
    val folder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { tree ->
        if (tree == null) return@rememberLauncherForActivityResult
        val pages = controller.exportPages(frames, includeAnnotations, exportOptions)
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
                    FilterChip(selected = !pdf && !png && !gif && !sequence && !mp4, onClick = { pdf = false; png = false; gif = false; sequence = false; mp4 = false }, label = { Text("SVG") }, modifier = Modifier.padding(end = 6.dp))
                    FilterChip(selected = pdf, onClick = { pdf = true; png = false; gif = false; sequence = false; mp4 = false }, label = { Text("PDF") }, modifier = Modifier.padding(end = 6.dp))
                    FilterChip(selected = png, onClick = { png = true; pdf = false; gif = false; sequence = false; mp4 = false }, label = { Text("PNG") })
                }
                Row {
                    FilterChip(selected = gif, onClick = { gif = true; sequence = false; png = false; pdf = false; mp4 = false }, label = { Text("GIF") }, modifier = Modifier.padding(end = 6.dp))
                    FilterChip(selected = sequence, onClick = { sequence = true; gif = false; png = false; pdf = false; mp4 = false }, label = { Text("PNG sequence") }, modifier = Modifier.padding(end = 6.dp))
                    FilterChip(selected = mp4, onClick = { mp4 = true; sequence = false; gif = false; png = false; pdf = false }, label = { Text("MP4 video") })
                }
                if (mp4) {
                    val s = controller.projectSettings
                    val (vw, vh) = com.smitnk.projectgrease.editor.VideoFrames.evenSize(controller.document.canvasWidth, controller.document.canvasHeight)
                    Text(
                        "H.264 MP4, frames ${s.frameStart}–${s.frameEnd} at ${s.fps} FPS, ${vw}×$vh px (odd sizes lose one row / column); held frames repeat the previous keyframe; the project background colour is used.",
                        Modifier.fillMaxWidth().padding(top = 8.dp)
                    )
                } else if (gif || sequence) {
                    Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("Transparent background", Modifier.weight(1f))
                        Switch(transparent, { transparent = it })
                    }
                    val s = controller.projectSettings
                    if (gif) Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("Dither (Floyd–Steinberg)", Modifier.weight(1f))
                        Switch(dither, { dither = it })
                    }
                    Text(
                        "Frames ${s.frameStart}–${s.frameEnd} at ${s.fps} FPS (Project > Settings), ${controller.document.canvasWidth}×${controller.document.canvasHeight} px; held frames repeat the previous keyframe." +
                            if (gif) " GIF uses an adaptive 255-colour palette per frame (exact when a frame has 255 colours or fewer)." else " One PNG per frame in the chosen folder.",
                        Modifier.fillMaxWidth().padding(top = 8.dp)
                    )
                } else if (png) {
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
                        FilterChip(selected = !wholeTimeline && !frameRange, onClick = { wholeTimeline = false; frameRange = false }, label = { Text("Current") }, modifier = Modifier.padding(end = 6.dp))
                        FilterChip(selected = frameRange, onClick = { frameRange = true; wholeTimeline = false }, label = { Text("${settings.frameStart}–${settings.frameEnd}") }, modifier = Modifier.padding(end = 6.dp))
                        FilterChip(selected = wholeTimeline, onClick = { wholeTimeline = true; frameRange = false }, label = { Text("Timeline (${controller.animation.timelineEnd})") })
                    }
                    Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("Clip to canvas", Modifier.weight(1f))
                        Switch(clipToCanvas, { clipToCanvas = it })
                    }
                    Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("Selected layers only", Modifier.weight(1f))
                        Switch(selectedLayersOnly, { selectedLayersOnly = it })
                    }
                    if (selectedLayersOnly) Row(Modifier.horizontalScroll(rememberScrollState())) {
                        layerNames.forEachIndexed { index, name ->
                            FilterChip(
                                selected = index in chosenLayers,
                                onClick = { chosenLayers = if (index in chosenLayers) chosenLayers - index else chosenLayers + index },
                                label = { Text(name) }, modifier = Modifier.padding(end = 6.dp)
                            )
                        }
                    }
                    Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("Include annotations", Modifier.weight(1f))
                        Switch(includeAnnotations, { includeAnnotations = it })
                    }
                    Text(
                        if (pdf) "One PDF, one page per frame." else if (wholeTimeline || frameRange) "A folder with one SVG per frame." else "One SVG file.",
                        Modifier.fillMaxWidth().padding(top = 8.dp)
                    )
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                val extension = if (pdf) "pdf" else "svg"
                if (mp4) mp4File.launch("$baseName.mp4")
                else if (gif) gifFile.launch("$baseName.gif")
                else if (sequence) sequenceFolder.launch(null)
                else if (png) pngFile.launch("$baseName.png")
                else if (!pdf && (wholeTimeline || frameRange)) folder.launch(null) else singleFile.launch("$baseName.$extension")
            }) { Text("Export") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } }
    )
}
