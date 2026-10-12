package com.smitnk.projectgrease.ui

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.Rotoscope

/** Decodes rotoscope frames (still, sequence, movie) for the editor, a few frames cached. */
class RotoscopeLoader(private val context: Context) {
    private val cache = object : LinkedHashMap<String, Triple<IntArray, Int, Int>>(16, 0.75f, true) {
        override fun removeEldestEntry(e: MutableMap.MutableEntry<String, Triple<IntArray, Int, Int>>?) = size > 24
    }
    private var retriever: MediaMetadataRetriever? = null
    private var retrieverUri: String? = null

    fun frame(r: Rotoscope, n: Int): Triple<IntArray, Int, Int>? {
        if (n < 1) return null
        val key = r.kind.name + "|" + r.uris.firstOrNull() + "|" + n
        cache[key]?.let { return it }
        val img = when (r.kind) {
            Rotoscope.Kind.IMAGE -> decodeTextureImage(context, Uri.parse(r.uris[0]))
            Rotoscope.Kind.SEQUENCE -> r.uris.getOrNull(n - 1)?.let { decodeTextureImage(context, Uri.parse(it)) }
            Rotoscope.Kind.MOVIE -> movieFrame(r.uris[0], n)
        } ?: return null
        cache[key] = img
        return img
    }

    private fun movieFrame(uri: String, n: Int): Triple<IntArray, Int, Int>? = runCatching {
        val mr = retriever?.takeIf { retrieverUri == uri } ?: MediaMetadataRetriever().also {
            retriever?.release(); it.setDataSource(context, Uri.parse(uri)); retriever = it; retrieverUri = uri
        }
        // a movie frame per scene frame, as Blender plays a movie clip in an image user
        val bmp: Bitmap = (if (Build.VERSION.SDK_INT >= 28) mr.getFrameAtIndex(n - 1) else null)
            ?: mr.getFrameAtTime((n - 1) * 1_000_000L / movieFps(mr), MediaMetadataRetriever.OPTION_CLOSEST) ?: return null
        val s = minOf(1f, 1024f / maxOf(bmp.width, bmp.height))
        val b = if (s < 1f) Bitmap.createScaledBitmap(bmp, maxOf(1, (bmp.width * s).toInt()), maxOf(1, (bmp.height * s).toInt()), true) else bmp
        val px = IntArray(b.width * b.height); b.getPixels(px, 0, b.width, 0, 0, b.width, b.height)
        Triple(px, b.width, b.height)
    }.getOrNull()

    private fun movieFps(mr: MediaMetadataRetriever): Long {
        val count = if (Build.VERSION.SDK_INT >= 28) mr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_FRAME_COUNT)?.toLongOrNull() else null
        val ms = mr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull()
        return if (count != null && ms != null && ms > 0) (count * 1000 / ms).coerceIn(1, 240) else 24
    }

    /** Frame count of a movie (ImageUser frames). */
    fun movieFrames(uri: Uri): Int = runCatching {
        val mr = MediaMetadataRetriever()
        try {
            mr.setDataSource(context, uri)
            val count = if (Build.VERSION.SDK_INT >= 28) mr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_FRAME_COUNT)?.toIntOrNull() else null
            count ?: ((mr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: 0L) * movieFps(mr) / 1000).toInt()
        } finally { mr.release() }
    }.getOrDefault(0)

    fun clearCache() { cache.clear() }
}

/** Rotoscope reference: import, opacity, visibility and the timeline mapping (start, offset, cyclic). */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun RotoscopeSheet(controller: EditorController, context: Context, onDismiss: () -> Unit, redraw: () -> Unit) {
    val r = controller.rotoscope
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val observed = tick
    fun changed() { controller.rotoscopeChanged(); tick++; redraw() }
    fun keep(uri: Uri) = runCatching { context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }
    val image = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) { keep(uri); r.set(Rotoscope.Kind.IMAGE, listOf(uri.toString()), 1); changed() }
    }
    val sequence = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        if (uris.isNotEmpty()) {
            uris.forEach(::keep)
            // sorted by name, as Blender reads a numbered sequence
            val sorted = uris.sortedBy { it.lastPathSegment ?: it.toString() }.map { it.toString() }
            r.set(Rotoscope.Kind.SEQUENCE, sorted, sorted.size); changed()
        }
    }
    val movie = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            keep(uri)
            val n = controller.rotoscopeLoader?.movieFrames(uri) ?: 0
            r.set(Rotoscope.Kind.MOVIE, listOf(uri.toString()), n.coerceAtLeast(1)); changed()
        }
    }
    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(16.dp)) {
            Text("Rotoscope reference", style = MaterialTheme.typography.headlineSmall)
            Text(if (r.isEmpty) "No reference" else "${r.kind.name.lowercase()} · ${r.frames} frame(s)", Modifier.padding(vertical = 6.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Button(onClick = { image.launch(arrayOf("image/*")) }, Modifier.testTag("rotoImage")) { Text("Image") }
                Button(onClick = { sequence.launch(arrayOf("image/*")) }) { Text("Image sequence") }
                Button(onClick = { movie.launch(arrayOf("video/*")) }) { Text("Video") }
            }
            if (!r.isEmpty) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Show", Modifier.weight(1f)); Switch(r.visible, { r.visible = it; changed() })
                }
                Text("Opacity ${(r.opacity * 100).toInt()}%")
                Slider(r.opacity, { r.setOpacity(it); changed() }, valueRange = 0f..1f, modifier = Modifier.testTag("rotoOpacity"))
                if (r.kind != Rotoscope.Kind.IMAGE) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text("Start frame ${r.start}", Modifier.weight(1f))
                        TextButton({ r.start--; changed() }) { Text("−") }; TextButton({ r.start++; changed() }) { Text("+") }
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text("Offset ${r.offset}", Modifier.weight(1f))
                        TextButton({ r.offset--; changed() }) { Text("−") }; TextButton({ r.offset++; changed() }) { Text("+") }
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text("Cyclic", Modifier.weight(1f)); Switch(r.cyclic, { r.cyclic = it; changed() })
                    }
                    TextButton(onClick = { controller.fitTimelineToRotoscope(); changed() }) { Text("Set the scene end to the reference length") }
                }
                TextButton(onClick = { r.clear(); changed() }) { Text("Remove reference") }
            }
        }
    }
}
