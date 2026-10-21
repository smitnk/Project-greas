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
import androidx.compose.foundation.horizontalScroll
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

    private val worker = java.util.concurrent.Executors.newSingleThreadExecutor()
    private val main = android.os.Handler(android.os.Looper.getMainLooper())
    private val pending = java.util.Collections.synchronizedSet(HashSet<String>())
    private val failed = java.util.Collections.synchronizedSet(HashSet<String>())
    private fun key(kind: Rotoscope.Kind, uris: List<String>, n: Int) = kind.name + "|" + uris.firstOrNull() + "|" + n

    fun cached(r: Rotoscope, n: Int): Triple<IntArray, Int, Int>? = synchronized(cache) { cache[key(r.kind, r.uris, n)] }

    /** Decodes frame [n] off the UI thread, then runs [done] on it. The job keeps its own copy of the
     *  reference (kind, files): the reference may be replaced or removed while it runs. */
    fun decodeAsync(r: Rotoscope, n: Int, done: () -> Unit = {}) {
        val kind = r.kind; val uris = r.uris.toList()
        val k = key(kind, uris, n)
        if (uris.isEmpty() || n < 1 || n > r.frames || cached(r, n) != null || k in failed || !pending.add(k)) return
        worker.execute {
            // a frame that does not decode is not retried: each failure re-rendered, which asked again
            val ok = try { decode(kind, uris, n) != null } catch (_: Throwable) { false } finally { pending.remove(k) }
            if (ok) main.post(done) else failed.add(k)
        }
    }
    fun prefetch(r: Rotoscope, from: Int, count: Int) { for (n in from until from + count) decodeAsync(r, n) }

    fun frame(r: Rotoscope, n: Int): Triple<IntArray, Int, Int>? = decode(r.kind, r.uris.toList(), n)

    private fun decode(kind: Rotoscope.Kind, uris: List<String>, n: Int): Triple<IntArray, Int, Int>? {
        if (n < 1 || uris.isEmpty()) return null
        val key = key(kind, uris, n)
        synchronized(cache) { cache[key] }?.let { return it }
        val img = when (kind) {
            Rotoscope.Kind.IMAGE -> decodeTextureImage(context, Uri.parse(uris[0]))
            Rotoscope.Kind.SEQUENCE -> uris.getOrNull(n - 1)?.let { decodeTextureImage(context, Uri.parse(it)) }
            Rotoscope.Kind.MOVIE -> movieFrame(uris[0], n)
        } ?: return null
        synchronized(cache) { cache[key] = img }
        return img
    }

    private fun movieFrame(uri: String, n: Int): Triple<IntArray, Int, Int>? = runCatching {
        val mr = retriever?.takeIf { retrieverUri == uri } ?: MediaMetadataRetriever().also {
            retriever?.release(); it.setDataSource(context, Uri.parse(uri)); retriever = it; retrieverUri = uri
        }
        // a movie frame per scene frame, as Blender plays a movie clip in an image user
        // the phone's own video decoders (no encoder involved); some (HEVC / HDR phone videos) fail
        // getFrameAtIndex, so the time-based, scaled-down decode is the fallback, not a casualty
        val us = (n - 1) * 1_000_000L / movieFps(mr)
        val bmp: Bitmap = (if (Build.VERSION.SDK_INT >= 28) runCatching { mr.getFrameAtIndex(n - 1) }.getOrNull() else null)
            ?: (if (Build.VERSION.SDK_INT >= 27) runCatching { mr.getScaledFrameAtTime(us, MediaMetadataRetriever.OPTION_CLOSEST, 1024, 1024) }.getOrNull() else null)
            ?: runCatching { mr.getFrameAtTime(us, MediaMetadataRetriever.OPTION_CLOSEST) }.getOrNull() ?: return null
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

    fun clearCache() { synchronized(cache) { cache.clear() } }
}

internal class RotoscopePickers(
    val media: androidx.activity.result.ActivityResultLauncher<Array<String>>,
    val sequence: androidx.activity.result.ActivityResultLauncher<Array<String>>,
    val audio: androidx.activity.result.ActivityResultLauncher<Array<String>>)

/** The photo / video pickers, registered on the editor screen (always composed). Registered inside
 *  the sheet, the picked file was lost whenever the sheet was gone or the app was restarted in the
 *  background before Android handed the file back; the result also waits for the restored drawing. */
@Composable
internal fun rememberRotoscopePickers(controller: EditorController, context: Context, redraw: () -> Unit): RotoscopePickers {
    fun changed() { controller.rotoscopeChanged(); redraw() }
    fun keep(uri: Uri) = runCatching { context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }
    // one picker for photos and videos; the file's type decides which it is
    val media = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        com.smitnk.projectgrease.CrashLog.event("IMPORT picked $uri type ${uri?.let { context.contentResolver.getType(it) }}")
        if (uri == null) return@rememberLauncherForActivityResult
        keep(uri)
        controller.afterReattach { importMedia(controller, context, uri, ::changed) }
    }
    val sequence = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        com.smitnk.projectgrease.CrashLog.event("IMPORT sequence picked ${uris.size} files")
        if (uris.isNotEmpty()) {
            uris.forEach(::keep)
            // sorted by name, as Blender reads a numbered sequence
            val sorted = uris.sortedBy { it.lastPathSegment ?: it.toString() }.map { it.toString() }
            controller.afterReattach { controller.rotoscope.set(Rotoscope.Kind.SEQUENCE, sorted, sorted.size); changed() }
        }
    }
    // several audio files at once, each its own track starting on the current frame
    val audio = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        com.smitnk.projectgrease.CrashLog.event("IMPORT audio picked ${uris.size} files")
        if (uris.isNotEmpty()) {
            uris.forEach(::keep)
            controller.afterReattach {
                var added = 0
                for (u in uris) {
                    val ms = controller.audioPlayback?.durationOf(u) ?: 0L
                    if (ms <= 0L) { toast(context, "Couldn't read ${u.lastPathSegment ?: "this file"} as audio"); continue }
                    if (controller.addAudio(u.toString(), displayName(context, u), ms) != null) added++
                }
                if (added > 0) toast(context, "Added $added audio track" + (if (added > 1) "s" else ""))
                redraw()
            }
        }
    }
    return remember(media, sequence, audio) { RotoscopePickers(media, sequence, audio) }
}

private fun displayName(context: Context, uri: Uri): String = runCatching {
    context.contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
        if (c.moveToFirst()) c.getString(0) else null
    }
}.getOrNull() ?: uri.lastPathSegment ?: "Audio"

private fun importMedia(controller: EditorController, context: Context, uri: Uri, changed: () -> Unit) {
        val r = controller.rotoscope
        val loader = controller.rotoscopeLoader
        if (context.contentResolver.getType(uri)?.startsWith("video") == true) {
            val n = loader?.movieFrames(uri) ?: 0
            if (n <= 0) { toast(context, "Couldn't read this video on this phone"); return }
            r.set(Rotoscope.Kind.MOVIE, listOf(uri.toString()), n)
            // the clip starts on the current frame and the animation grows to hold it (trim it in the sheet)
            r.start = controller.animation.currentFrame
            if (controller.animation.sceneEnd < r.start + n - 1) controller.animation.setSceneEnd(r.start + n - 1)
            changed()
            loader?.decodeAsync(r, 1) { if (loader.cached(r, 1) == null) toast(context, "Couldn't decode this video's frames") else changed() }
        } else {
            r.set(Rotoscope.Kind.IMAGE, listOf(uri.toString()), 1)
            if (loader?.frame(r, 1) == null) { r.clear(); toast(context, "Couldn't open this photo (format not supported)") }
            changed()
        }
}

/** Rotoscope reference: import, opacity, visibility and the timeline mapping (start, offset, cyclic). */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun RotoscopeSheet(controller: EditorController, context: Context, onDismiss: () -> Unit, redraw: () -> Unit, pickers: RotoscopePickers) {
    val r = controller.rotoscope
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val observed = tick
    fun changed() { controller.rotoscopeChanged(); tick++; redraw() }
    fun placed() { controller.rotoscopeChanged(imageChanged = false); tick++; redraw() }
    var more by remember { mutableStateOf(false) }
    val media = pickers.media
    val sequence = pickers.sequence
    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(16.dp)) {
            Text("Photo, video & audio", style = MaterialTheme.typography.headlineSmall)
            Text(if (r.isEmpty) "Pick a photo or video to draw over. It sits behind your drawing and is not exported."
                 else "${if (r.kind == Rotoscope.Kind.MOVIE) "Video" else if (r.kind == Rotoscope.Kind.SEQUENCE) "Image sequence" else "Photo"}" +
                      (if (r.kind == Rotoscope.Kind.IMAGE) "" else " · ${r.frames} frames, one per timeline frame"), Modifier.padding(vertical = 6.dp))
            val layerNow = controller.layerName(controller.selectedLayer)
            // one reference per photo / video, each on its layer (hidden with it, in layer order)
            if (controller.references.count { !it.isEmpty } > 1 || controller.references.size > 1) {
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    controller.references.forEachIndexed { i, ref ->
                        FilterChip(i == controller.activeReference, { controller.activeReference = i; tick++ },
                            label = { Text((if (ref.kind == Rotoscope.Kind.IMAGE) "Photo" else "Video") + " · " + ref.layer.ifEmpty { "all" }) })
                    }
                }
            }
            Button(onClick = { controller.addReference(layerNow); tick++; media.launch(arrayOf("image/*", "video/*")) },
                Modifier.fillMaxWidth().testTag("rotoImage")) {
                Text(if (controller.references.all { it.isEmpty }) "Import photo or video on $layerNow" else "Add photo or video on $layerNow")
            }
            if (!r.isEmpty) OutlinedButton(onClick = { media.launch(arrayOf("image/*", "video/*")) }, Modifier.fillMaxWidth()) {
                Text("Replace this one")
            }
            if (!r.isEmpty) TextButton(onClick = { r.layer = layerNow; changed() }) { Text("Move this one to $layerNow (now: ${r.layer.ifEmpty { "all layers" }})") }
            if (!r.isEmpty) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Show", Modifier.weight(1f)); Switch(r.visible, { r.visible = it; changed() })
                }
                Text("Opacity ${(r.opacity * 100).toInt()}%")
                Slider(r.opacity, { r.setOpacity(it); placed() }, valueRange = 0f..1f, modifier = Modifier.testTag("rotoOpacity"))
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Button(onClick = { controller.referenceAdjusting = true; onDismiss() }, Modifier.testTag("rotoAdjust")) { Text("Move / resize on canvas") }
                    OutlinedButton(onClick = { r.resetPlacement(); placed() }) { Text("Reset") }
                }
                // squash / stretch: width and height separately
                Text("Width ${(r.stretchX * 100).toInt()}%")
                Slider(r.stretchX, { r.stretchX = it; placed() }, valueRange = 0.1f..3f, modifier = Modifier.testTag("rotoStretchX"))
                Text("Height ${(r.stretchY * 100).toInt()}%")
                Slider(r.stretchY, { r.stretchY = it; placed() }, valueRange = 0.1f..3f, modifier = Modifier.testTag("rotoStretchY"))
                if (r.kind != Rotoscope.Kind.IMAGE && r.frames > 1) {
                    // trim: which part of the video is used; it plays from "Starts at frame"
                    val fps = controller.animation.fps.coerceAtLeast(1)
                    val last = (r.offset + r.usedFrames).coerceIn(1, r.frames); val first = (r.offset + 1).coerceIn(1, last)
                    fun secs(f: Int) = "%.1fs".format((f - 1) / fps.toFloat())
                    Text("Trim: video frames $first–$last (${secs(first)}–${secs(last + 1)}), ${last - first + 1} frames")
                    RangeSlider(first.toFloat()..last.toFloat(), { v ->
                        r.trim(v.start.toInt(), v.endInclusive.toInt()); tick++
                    }, valueRange = 1f..r.frames.toFloat(), onValueChangeFinished = { changed() }, modifier = Modifier.testTag("rotoTrim"))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text("Starts at frame ${r.start}", Modifier.weight(1f))
                        TextButton({ r.start--; changed() }) { Text("−") }; TextButton({ r.start++; changed() }) { Text("+") }
                        TextButton({ r.start = controller.animation.currentFrame; changed() }) { Text("Here") }
                    }
                    TextButton(onClick = { controller.fitTimelineToRotoscope(); changed() }) { Text("Make the animation as long as the clip") }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(if (r.onFrames.isEmpty()) "Shown on every frame" else "Only on frame" + (if (r.onFrames.size > 1) "s " else " ") + r.onFrames.joinToString(), Modifier.weight(1f))
                    Switch(r.onFrames.isNotEmpty(), { controller.showReferenceOnlyHere(it); tick++; redraw() }, Modifier.testTag("rotoOnlyHere"))
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    OutlinedButton(onClick = { controller.copyReference(); tick++ }, Modifier.testTag("rotoCopy")) { Text("Copy") }
                    OutlinedButton(onClick = { controller.pasteReference(); tick++; redraw() }, enabled = controller.referenceClipboard != null,
                        modifier = Modifier.testTag("rotoPaste")) { Text("Paste on frame ${controller.animation.currentFrame}") }
                }
                TextButton(onClick = { more = !more }) { Text(if (more) "Fewer options ▴" else "More options ▾") }
                if (more) {
                    Text("Size ${(r.scale * 100).toInt()}%")
                    Slider(r.scale, { r.scale = it; placed() }, valueRange = 0.1f..4f, modifier = Modifier.testTag("rotoScale"))
                    Text("Rotation ${Math.toDegrees(r.rotation.toDouble()).toInt()}°")
                    Slider(r.rotation, { r.rotation = it; placed() }, valueRange = -3.1416f..3.1416f)
                    if (r.kind != Rotoscope.Kind.IMAGE) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text("Loop", Modifier.weight(1f)); Switch(r.cyclic, { r.cyclic = it; changed() })
                        }
                    }
                    OutlinedButton(onClick = { sequence.launch(arrayOf("image/*")) }) { Text("Import an image sequence instead") }
                }
                TextButton(onClick = { controller.removeReference(); tick++; redraw() }) { Text("Remove") }
            }
            if (r.isEmpty && controller.referenceClipboard != null) {
                OutlinedButton(onClick = { controller.pasteReference(); tick++; redraw() }, Modifier.fillMaxWidth()) {
                    Text("Paste copied photo on frame ${controller.animation.currentFrame}")
                }
            }
            HorizontalDivider(Modifier.padding(vertical = 12.dp))
            Text("Audio", style = MaterialTheme.typography.titleMedium)
            Text("Plays with the animation and goes into the MP4 export.", style = MaterialTheme.typography.bodySmall)
            Button(onClick = { pickers.audio.launch(arrayOf("audio/*")) }, Modifier.fillMaxWidth().padding(vertical = 4.dp).testTag("audioAdd")) {
                Text("Add audio (one or more files) on frame ${controller.animation.currentFrame}")
            }
            val fps = controller.animation.fps.coerceAtLeast(1)
            controller.audioStrips.toList().forEach { a ->
                Card(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
                    Column(Modifier.padding(10.dp)) {
                        val endFrame = a.start + ((a.durationMs * fps + 999) / 1000).toInt() - 1
                        Text(a.name, style = MaterialTheme.typography.titleSmall)
                        Text("Frames ${a.start}–$endFrame (%.1fs)".format(a.durationMs / 1000f), style = MaterialTheme.typography.bodySmall)
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text("Starts at frame ${a.start}", Modifier.weight(1f))
                            TextButton({ a.start--; controller.audioChanged(); tick++ }) { Text("−") }
                            TextButton({ a.start++; controller.audioChanged(); tick++ }) { Text("+") }
                            TextButton({ a.start = controller.animation.currentFrame; controller.audioChanged(); tick++ }) { Text("Here") }
                        }
                        Text("Volume ${(a.volume * 100).toInt()}%")
                        Slider(a.volume, { a.volume = it; tick++ }, valueRange = 0f..2f, onValueChangeFinished = { controller.audioChanged() })
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text("Mute", Modifier.weight(1f)); Switch(a.muted, { a.muted = it; controller.audioChanged(); tick++ })
                            TextButton({ controller.removeAudio(a); tick++ }) { Text("Remove") }
                        }
                    }
                }
            }
        }
    }
}
