@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)
package com.smitnk.projectgrease.ui

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.media.MediaMetadataRetriever
import android.media.MediaPlayer
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.smitnk.projectgrease.editor.AudioStrip
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.Rotoscope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * The three separate imports (photo, video, audio). A picked video or audio file opens its trim
 * screen first (preview, start / end, where it starts on the timeline); only the kept part is added.
 */
class MediaImport internal constructor(
    val photo: androidx.activity.result.ActivityResultLauncher<Array<String>>,
    val video: androidx.activity.result.ActivityResultLauncher<Array<String>>,
    val audio: androidx.activity.result.ActivityResultLauncher<Array<String>>,
    internal val pendingVideo: MutableState<Uri?>,
    internal val pendingAudio: MutableState<List<Uri>>
) {
    fun importPhoto() = photo.launch(arrayOf("image/*"))
    fun importVideo() = video.launch(arrayOf("video/*"))
    fun importAudio() = audio.launch(arrayOf("audio/*"))
}

private fun keep(context: Context, uri: Uri) =
    runCatching { context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }

internal fun displayName(context: Context, uri: Uri): String = runCatching {
    context.contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
        if (c.moveToFirst()) c.getString(0) else null
    }
}.getOrNull() ?: uri.lastPathSegment ?: "Audio"

@Composable
fun rememberMediaImport(controller: EditorController, context: Context, redraw: () -> Unit): MediaImport {
    val pendingVideo = remember { mutableStateOf<Uri?>(null) }
    val pendingAudio = remember { mutableStateOf<List<Uri>>(emptyList()) }
    val photo = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        com.smitnk.projectgrease.CrashLog.event("IMPORT photo picked $uri")
        if (uri == null) return@rememberLauncherForActivityResult
        keep(context, uri)
        controller.afterReattach { addPhoto(controller, context, uri); redraw() }
    }
    val video = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        com.smitnk.projectgrease.CrashLog.event("IMPORT video picked $uri")
        if (uri == null) return@rememberLauncherForActivityResult
        keep(context, uri); pendingVideo.value = uri
    }
    val audio = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        com.smitnk.projectgrease.CrashLog.event("IMPORT audio picked ${uris.size} files")
        uris.forEach { keep(context, it) }
        if (uris.isNotEmpty()) pendingAudio.value = pendingAudio.value + uris
    }
    return remember(photo, video, audio) { MediaImport(photo, video, audio, pendingVideo, pendingAudio) }
}

/** A photo on the current layer, then the transform box to place it. */
private fun addPhoto(controller: EditorController, context: Context, uri: Uri) {
    val r = controller.addReference(controller.layerName(controller.selectedLayer))
    r.set(Rotoscope.Kind.IMAGE, listOf(uri.toString()), 1)
    val img = controller.rotoscopeLoader?.frame(r, 1)
    if (img == null) { controller.removeReference(); toast(context, "Couldn't open this photo (format not supported)"); return }
    r.pixW = img.second; r.pixH = img.third
    // a photo belongs to the frame it was imported on (Copy / Paste puts it on others; "every frame" in Photos & videos)
    r.onFrames.add(controller.animation.currentFrame)
    controller.rotoscopeChanged(); controller.history.markEdit()
    controller.referenceAdjusting = true
}

/** The trim screens; shown while a picked video / audio file waits. */
@Composable
fun MediaImportDialogs(m: MediaImport, controller: EditorController, context: Context, redraw: () -> Unit) {
    m.pendingVideo.value?.let { uri -> VideoTrimDialog(uri, controller, context, { m.pendingVideo.value = null; redraw() }) }
    m.pendingAudio.value.firstOrNull()?.let { uri ->
        AudioTrimDialog(uri, controller, context) { m.pendingAudio.value = m.pendingAudio.value.drop(1); redraw() }
    }
}

private fun fmt(ms: Long) = "%d:%04.1f".format(ms / 60000, (ms % 60000) / 1000f)

@Composable
private fun VideoTrimDialog(uri: Uri, controller: EditorController, context: Context, done: () -> Unit) {
    val loader = controller.rotoscopeLoader
    val pink = Color(0xFFF2416B)
    var frames by remember(uri) { mutableIntStateOf(-1) }
    var fps by remember(uri) { mutableIntStateOf(24) }
    var thumbs by remember(uri) { mutableStateOf<List<Bitmap?>>(emptyList()) }
    fun grab(frame: Int, size: Int): Bitmap? = runCatching {
        val mr = MediaMetadataRetriever()
        try {
            mr.setDataSource(context, uri)
            val us = (frame - 1) * 1_000_000L / fps.coerceAtLeast(1)
            if (android.os.Build.VERSION.SDK_INT >= 27) mr.getScaledFrameAtTime(us, MediaMetadataRetriever.OPTION_CLOSEST, size, size)
            else mr.getFrameAtTime(us, MediaMetadataRetriever.OPTION_CLOSEST)
        } finally { mr.release() }
    }.getOrNull()
    LaunchedEffect(uri) {
        withContext(Dispatchers.IO) { frames = loader?.movieFrames(uri) ?: 0; fps = loader?.movieFps(uri) ?: 24 }
        // the strip's thumbnails, decoded once
        if (frames > 0) thumbs = withContext(Dispatchers.IO) { (0 until 8).map { k -> grab(1 + k * (frames - 1) / 7, 160) } }
    }
    Dialog(onDismissRequest = done, properties = DialogProperties(usePlatformDefaultWidth = false, decorFitsSystemWindows = false)) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.surface) {
            Column(Modifier.fillMaxSize().systemBarsPadding()) {
                var a by remember(uri) { mutableIntStateOf(1) }
                var b by remember(uri) { mutableIntStateOf(1) }
                var pos by remember(uri) { mutableIntStateOf(1) }
                var playing by remember(uri) { mutableStateOf(false) }
                var importing by remember(uri) { mutableStateOf(false) }
                LaunchedEffect(frames) { if (frames > 0) { a = 1; b = frames; pos = 1 } }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = done) { Icon(Icons.Default.Close, "Cancel") }
                    Text("Video import", Modifier.weight(1f), style = MaterialTheme.typography.titleSmall)
                    IconButton(enabled = frames > 0 && !importing, onClick = {
                        importing = true; playing = false
                        addVideoLayer(controller, context, uri, frames, a, b)
                        done()
                    }, modifier = Modifier.testTag("videoAdd")) { Icon(Icons.Default.Check, "Import", tint = if (frames > 0) pink else Color.Gray) }
                }
                when {
                    frames < 0 -> { Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { CircularProgressIndicator(color = pink) }; return@Column }
                    frames == 0 -> { Text("Couldn't read this video on this phone.", Modifier.padding(16.dp)); TextButton(onClick = done) { Text("Close") }; return@Column }
                }
                // the frame under the playhead (decoded off the UI thread)
                var bmp by remember(uri) { mutableStateOf<Bitmap?>(null) }
                LaunchedEffect(uri, pos) { bmp = withContext(Dispatchers.IO) { grab(pos, 720) } }
                Box(Modifier.fillMaxWidth().aspectRatio(16f / 9f).background(Color.Black), contentAlignment = Alignment.Center) {
                    bmp?.let { Image(it.asImageBitmap(), null, Modifier.fillMaxSize()) }
                    Text("${clock((pos - 1) * 1000L / fps)} | $fps fps", Modifier.align(Alignment.TopEnd).padding(6.dp)
                        .background(Color(0xAA000000), MaterialTheme.shapes.small).padding(horizontal = 6.dp, vertical = 2.dp), color = Color.White, fontSize = 11.sp)
                }
                // play: steps the preview through the kept part (about 12 pictures a second)
                LaunchedEffect(playing) {
                    while (playing) {
                        kotlinx.coroutines.delay(1000L / minOf(fps, 12))
                        val step = maxOf(1, fps / 12)
                        if (pos + step > b) { pos = b; playing = false } else pos += step
                    }
                }
                Row(Modifier.fillMaxWidth().padding(vertical = 8.dp), horizontalArrangement = Arrangement.Center, verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = { pos = a }) { Icon(Icons.Default.SkipPrevious, "To the start") }
                    IconButton(onClick = { pos = (pos - 1).coerceAtLeast(a) }) { Icon(Icons.Default.ChevronLeft, "Previous frame") }
                    IconButton(onClick = { if (!playing && pos >= b) pos = a; playing = !playing }, Modifier.testTag("videoPlay")) {
                        Icon(if (playing) Icons.Default.Pause else Icons.Default.PlayArrow, "Play")
                    }
                    IconButton(onClick = { pos = (pos + 1).coerceAtMost(b) }) { Icon(Icons.Default.ChevronRight, "Next frame") }
                    IconButton(onClick = { pos = b }) { Icon(Icons.Default.SkipNext, "To the end") }
                }
                // the strip: thumbnails, the kept part bright between the pink handles, the playhead
                Canvas(Modifier.fillMaxWidth().height(84.dp).padding(horizontal = 8.dp).testTag("videoTrim").pointerInput(frames) {
                    var which = 0 // 1 start handle, 2 end handle, 0 the playhead
                    val inset = 20.dp.toPx(); val w = size.width - 2 * inset
                    fun frameAt(x: Float) = (1 + ((x - inset) / w * (frames - 1)).toInt()).coerceIn(1, frames)
                    fun xOf(f: Int) = inset + (f - 1).toFloat() / (frames - 1).coerceAtLeast(1) * w
                    detectDragGestures(onDragStart = { o ->
                        which = when { kotlin.math.abs(o.x - xOf(a)) < 28.dp.toPx() -> 1; kotlin.math.abs(o.x - xOf(b)) < 28.dp.toPx() -> 2; else -> 0 }
                        if (which == 0) pos = frameAt(o.x).coerceIn(a, b)
                        playing = false
                    }) { change, _ ->
                        change.consume()
                        val f = frameAt(change.position.x)
                        when (which) { 1 -> { a = f.coerceAtMost(b); pos = a }; 2 -> { b = f.coerceAtLeast(a); pos = b }; else -> pos = f.coerceIn(a, b) }
                    }
                }) {
                    val inset = 20.dp.toPx(); val w = size.width - 2 * inset; val h = size.height
                    fun xOf(f: Int) = inset + (f - 1).toFloat() / (frames - 1).coerceAtLeast(1) * w
                    val tw = w / 8f
                    thumbs.forEachIndexed { k, t ->
                        if (t != null) drawImage(t.asImageBitmap(), dstOffset = androidx.compose.ui.unit.IntOffset((inset + k * tw).toInt(), 0),
                            dstSize = androidx.compose.ui.unit.IntSize(tw.toInt() + 1, h.toInt()))
                    }
                    val xa = xOf(a); val xb = xOf(b)
                    drawRect(Color(0xAAFFFFFF), Offset(inset, 0f), Size(xa - inset, h))
                    drawRect(Color(0xAAFFFFFF), Offset(xb, 0f), Size(inset + w - xb, h))
                    drawRect(pink, Offset(xa, 0f), Size(xb - xa, h), style = androidx.compose.ui.graphics.drawscope.Stroke(5f))
                    for (hx in listOf(xa, xb)) {
                        drawRoundRect(pink, Offset(hx - 14f, 0f), Size(28f, h), CornerRadius(8f, 8f))
                        drawLine(Color.White, Offset(hx, h * 0.35f), Offset(hx, h * 0.65f), 3f)
                    }
                    val xp = xOf(pos)
                    drawLine(Color.White, Offset(xp, -6f), Offset(xp, h + 6f), 4f)
                    drawLine(pink, Offset(xp, -6f), Offset(xp, h + 6f), 2f)
                }
                Text("Keep ${clock((a - 1) * 1000L / fps)} – ${clock(b * 1000L / fps)} · ${b - a + 1} frames, from frame ${controller.animation.currentFrame}",
                    Modifier.align(Alignment.CenterHorizontally).padding(8.dp), fontSize = 11.sp)
                if (fps != controller.animation.fps) Text("The video is $fps fps and the animation ${controller.animation.fps} fps: one video frame per animation frame, as Blender plays a clip.",
                    Modifier.padding(horizontal = 16.dp), fontSize = 10.sp, color = Color.Gray)
                if (importing) Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(Modifier.size(18.dp), color = pink, strokeWidth = 2.dp); Text("  Importing video frames", color = pink, fontSize = 12.sp)
                }
            }
        }
    }
}

/** The kept part of the video as its own "Video" layer under the drawing: shown solid and in exports. */
private fun addVideoLayer(controller: EditorController, context: Context, uri: Uri, frames: Int, a: Int, b: Int) {
    val startAt = controller.animation.currentFrame
    val drawingLayer = controller.selectedLayer
    val name = "Video" + (if ((0 until controller.layerCount()).any { controller.layerName(it) == "Video" }) " ${controller.layerCount() + 1}" else "")
    if (!controller.createLayer(name)) { toast(context, "Couldn't add a layer for the video"); return }
    // under the drawing: the new layer goes to the bottom
    val newIndex = controller.selectedLayer
    if (newIndex > 0) controller.moveLayer(newIndex, 0)
    val r = controller.addReference(name)
    r.set(Rotoscope.Kind.MOVIE, listOf(uri.toString()), frames)
    r.trim(a, b); r.start = startAt
    r.setOpacity(1f); r.inExport = true
    if (controller.animation.sceneEnd < startAt + r.usedFrames - 1) controller.animation.setSceneEnd(startAt + r.usedFrames - 1)
    controller.selectLayer(drawingLayer.coerceIn(0, controller.layerCount() - 1).let { if (newIndex > 0) it + 1 else it }.coerceAtMost(controller.layerCount() - 1))
    controller.rotoscopeChanged(); controller.history.markEdit()
    controller.rotoscopeLoader?.preload(r, a, b)
    com.smitnk.projectgrease.CrashLog.event("IMPORT video frames $a..$b of $frames as layer '$name' from frame $startAt")
    toast(context, "Video layer added: frames $startAt–${startAt + r.usedFrames - 1}")
}

@Composable
private fun AudioTrimDialog(uri: Uri, controller: EditorController, context: Context, done: () -> Unit) {
    var durationMs by remember(uri) { mutableLongStateOf(-1L) }
    var peaks by remember(uri) { mutableStateOf<FloatArray?>(null) }
    LaunchedEffect(uri) {
        durationMs = withContext(Dispatchers.IO) { controller.audioPlayback?.durationOf(uri) ?: 0L }
        peaks = withContext(Dispatchers.IO) { AudioWaveform.peaks(context, uri.toString()) }
    }
    val player = remember(uri) { mutableStateOf<MediaPlayer?>(null) }
    DisposableEffect(uri) { onDispose { player.value?.release(); player.value = null } }
    val name = remember(uri) { displayName(context, uri) }
    val pink = Color(0xFFF2416B)
    Dialog(onDismissRequest = done, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.surface) {
            Column(Modifier.fillMaxSize().systemBarsPadding()) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = done) { Icon(Icons.Default.Close, "Cancel") }
                    Text(name, maxLines = 1, style = MaterialTheme.typography.titleSmall)
                }
                if (durationMs < 0 || (durationMs > 0 && peaks == null)) {
                    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
                        CircularProgressIndicator(color = pink); Text("Importing audio file", Modifier.padding(top = 12.dp), fontSize = 12.sp)
                    }
                    return@Column
                }
                if (durationMs == 0L) { Text("Couldn't read this file as audio.", Modifier.padding(16.dp)); TextButton(onClick = done) { Text("Skip") }; return@Column }
                var a by remember(uri) { mutableLongStateOf(0L) }
                var b by remember(uri) { mutableLongStateOf(durationMs) }
                var playing by remember(uri) { mutableStateOf(false) }
                var pos by remember(uri) { mutableLongStateOf(0L) }
                val fps = controller.animation.fps.coerceAtLeast(1)
                // the waveform: the kept part bright, the rest pale; drag a handle to trim, tap to set the preview point
                Canvas(Modifier.fillMaxWidth().weight(1f).padding(vertical = 24.dp).testTag("audioTrim").pointerInput(durationMs) {
                    var which = 0
                    val inset = 36.dp.toPx() // the handles stay fully on screen at the very start / end
                    val w = size.width - 2 * inset
                    detectDragGestures(onDragStart = { o ->
                        val xa = inset + a.toFloat() / durationMs * w; val xb = inset + b.toFloat() / durationMs * w
                        which = if (kotlin.math.abs(o.x - xa) < kotlin.math.abs(o.x - xb)) 1 else 2
                    }) { change, _ ->
                        change.consume()
                        val ms = ((change.position.x - inset) / w * durationMs).toLong().coerceIn(0L, durationMs)
                        val minLen = 1000L / fps
                        if (which == 1) a = ms.coerceAtMost(b - minLen) else b = ms.coerceAtLeast(a + minLen)
                        pos = if (which == 1) a else b
                    }
                }) {
                    val p = peaks ?: FloatArray(0)
                    val mid = size.height / 2
                    val inset = 36.dp.toPx(); val w = size.width - 2 * inset
                    val xa = inset + a.toFloat() / durationMs * w; val xb = inset + b.toFloat() / durationMs * w
                    var x = inset
                    while (x < inset + w && p.isNotEmpty()) {
                        val v = p[(((x - inset) / w) * p.size).toInt().coerceIn(0, p.size - 1)]
                        val h = (size.height * 0.45f * v).coerceAtLeast(1f)
                        drawLine(if (x in xa..xb) pink else pink.copy(alpha = 0.3f), Offset(x, mid - h), Offset(x, mid + h), 2f)
                        x += 3f
                    }
                    for (hx in listOf(xa, xb)) {
                        drawRoundRect(pink, Offset(hx - 12f, mid - 40f), Size(24f, 80f), CornerRadius(8f, 8f))
                        drawLine(Color.White, Offset(hx, mid - 16f), Offset(hx, mid + 16f), 2f)
                    }
                    val xp = inset + pos.toFloat() / durationMs * w
                    drawLine(pink, Offset(xp, 0f), Offset(xp, size.height), 2f)
                }
                // the same trim as a slider, for exact values
                RangeSlider(a.toFloat()..b.toFloat(), { v -> if (v.endInclusive - v.start >= 1000f / fps) { if (v.start.toLong() != a) pos = v.start.toLong() else pos = v.endInclusive.toLong(); a = v.start.toLong(); b = v.endInclusive.toLong() } },
                    valueRange = 0f..durationMs.toFloat(), modifier = Modifier.padding(horizontal = 16.dp).testTag("audioTrimSlider"),
                    colors = SliderDefaults.colors(thumbColor = pink, activeTrackColor = pink))
                Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(clock(a), fontSize = 11.sp, color = pink)
                    Spacer(Modifier.weight(1f))
                    IconButton(onClick = { pos = a }) { Icon(Icons.Default.SkipPrevious, "To the start") }
                    IconButton(onClick = {
                        val p = player.value ?: runCatching { MediaPlayer().apply { setDataSource(context, uri); prepare() } }.getOrNull()?.also { player.value = it }
                        if (p != null) {
                            if (playing) { p.pause(); playing = false }
                            else { p.seekTo((if (pos in a until b) pos else a).toInt()); p.start(); playing = true }
                        }
                    }, Modifier.testTag("audioPreview")) { Icon(if (playing) Icons.Default.Pause else Icons.Default.PlayArrow, "Play") }
                    IconButton(onClick = { pos = b }) { Icon(Icons.Default.SkipNext, "To the end") }
                    Spacer(Modifier.weight(1f))
                    Text(clock(b), fontSize = 11.sp, color = pink)
                }
                // preview: the playhead follows, stopping at the trim's end
                LaunchedEffect(playing, a, b) {
                    while (playing) {
                        val p = player.value ?: break
                        pos = p.currentPosition.toLong()
                        if (!p.isPlaying || pos >= b) { runCatching { p.pause() }; playing = false }
                        kotlinx.coroutines.delay(33)
                    }
                }
                Text("${clock(b - a)} · ${((b - a) * fps + 999) / 1000} frames from frame ${controller.animation.currentFrame}",
                    Modifier.align(Alignment.CenterHorizontally).padding(4.dp), fontSize = 11.sp)
                Button(onClick = {
                    val s = controller.addAudio(uri.toString(), name, durationMs, a, b, controller.animation.currentFrame)
                    if (s != null) toast(context, "Audio added: frames ${s.start}–${s.endFrame(fps)}")
                    done()
                }, Modifier.fillMaxWidth().padding(16.dp).height(52.dp).testTag("audioAddToTimeline"), colors = ButtonDefaults.buttonColors(containerColor = pink)) {
                    Text("Add to timeline")
                }
            }
        }
    }
}

/**
 * An animated GIF as its own "GIF" layer under the drawing: its frames, sampled at the animation's
 * fps, are written as PNG files in the app's storage and played as an image sequence (shown solid and
 * in exports, like a video layer). A GIF the decoder can't animate comes in as its first picture.
 * Blocking: call off the UI thread; the layer is made on the UI thread by [done].
 */
internal fun decodeGifFrames(context: Context, uri: Uri, fps: Int): List<String> {
    @Suppress("DEPRECATION")
    val movie = runCatching { context.contentResolver.openInputStream(uri)?.use { android.graphics.Movie.decodeStream(it) } }.getOrNull()
    val dir = java.io.File(context.filesDir, "gif/" + Integer.toHexString(uri.toString().hashCode()) + "_" + System.currentTimeMillis()).apply { mkdirs() }
    fun save(b: Bitmap, k: Int): String {
        val f = java.io.File(dir, "frame_%04d.png".format(k))
        f.outputStream().use { b.compress(Bitmap.CompressFormat.PNG, 100, it) }
        return Uri.fromFile(f).toString()
    }
    if (movie == null || movie.width() <= 0 || movie.height() <= 0) {
        val still = runCatching { context.contentResolver.openInputStream(uri)?.use { android.graphics.BitmapFactory.decodeStream(it) } }.getOrNull() ?: return emptyList()
        return listOf(save(still, 1))
    }
    val dur = movie.duration().coerceAtLeast(1)
    val n = ((dur.toLong() * fps.coerceAtLeast(1) + 999) / 1000).toInt().coerceIn(1, 600)
    val out = ArrayList<String>(n)
    val bmp = Bitmap.createBitmap(movie.width(), movie.height(), Bitmap.Config.ARGB_8888)
    val canvas = android.graphics.Canvas(bmp)
    for (k in 0 until n) {
        bmp.eraseColor(0)
        @Suppress("DEPRECATION") movie.setTime(((k * 1000L) / fps.coerceAtLeast(1)).toInt().coerceAtMost(dur - 1))
        @Suppress("DEPRECATION") movie.draw(canvas, 0f, 0f)
        out.add(save(bmp, k + 1))
    }
    return out
}

internal fun addGifLayer(controller: EditorController, context: Context, frames: List<String>) {
    if (frames.isEmpty()) { toast(context, "Couldn't read this GIF"); return }
    val startAt = controller.animation.currentFrame
    val drawingLayer = controller.selectedLayer
    if (!controller.createLayer("GIF")) { toast(context, "Couldn't add a layer for the GIF"); return }
    val newIndex = controller.selectedLayer
    if (newIndex > 0) controller.moveLayer(newIndex, 0)
    val r = controller.addReference("GIF")
    r.set(if (frames.size == 1) Rotoscope.Kind.IMAGE else Rotoscope.Kind.SEQUENCE, frames, frames.size)
    r.start = startAt; r.setOpacity(1f); r.inExport = true
    if (frames.size > 1 && controller.animation.sceneEnd < startAt + frames.size - 1) controller.animation.setSceneEnd(startAt + frames.size - 1)
    controller.selectLayer((if (newIndex > 0) drawingLayer + 1 else drawingLayer).coerceIn(0, controller.layerCount() - 1))
    controller.rotoscopeChanged(); controller.history.markEdit()
    com.smitnk.projectgrease.CrashLog.event("IMPORT GIF ${frames.size} frames as layer 'GIF' from frame $startAt")
    toast(context, if (frames.size == 1) "GIF added as a picture layer" else "GIF layer added: frames $startAt–${startAt + frames.size - 1}")
}
