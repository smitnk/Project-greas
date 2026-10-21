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
    var frames by remember(uri) { mutableIntStateOf(-1) }
    var fps by remember(uri) { mutableIntStateOf(24) }
    LaunchedEffect(uri) {
        withContext(Dispatchers.IO) { frames = loader?.movieFrames(uri) ?: 0; fps = loader?.movieFps(uri) ?: 24 }
    }
    Dialog(onDismissRequest = done, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxWidth().padding(12.dp), shape = MaterialTheme.shapes.large) {
            Column(Modifier.verticalScroll(rememberScrollState()).padding(16.dp)) {
                Text("Trim video", style = MaterialTheme.typography.headlineSmall)
                when {
                    frames < 0 -> { CircularProgressIndicator(Modifier.padding(24.dp)); return@Column }
                    frames == 0 -> { Text("Couldn't read this video on this phone."); TextButton(onClick = done) { Text("Close") }; return@Column }
                }
                var range by remember(uri) { mutableStateOf(1f..frames.toFloat()) }
                var preview by remember(uri) { mutableIntStateOf(1) }
                var startAt by remember(uri) { mutableIntStateOf(controller.animation.currentFrame) }
                var bmp by remember(uri) { mutableStateOf<Bitmap?>(null) }
                // the frame under the handle being moved, decoded off the UI thread
                LaunchedEffect(uri, preview) {
                    bmp = withContext(Dispatchers.IO) {
                        runCatching {
                            val mr = MediaMetadataRetriever()
                            try {
                                mr.setDataSource(context, uri)
                                val us = (preview - 1) * 1_000_000L / fps.coerceAtLeast(1)
                                if (android.os.Build.VERSION.SDK_INT >= 27) mr.getScaledFrameAtTime(us, MediaMetadataRetriever.OPTION_CLOSEST, 720, 720)
                                else mr.getFrameAtTime(us, MediaMetadataRetriever.OPTION_CLOSEST)
                            } finally { mr.release() }
                        }.getOrNull()
                    }
                }
                Box(Modifier.fillMaxWidth().height(260.dp).background(Color.Black), contentAlignment = Alignment.Center) {
                    bmp?.let { Image(it.asImageBitmap(), null, Modifier.fillMaxSize()) } ?: CircularProgressIndicator()
                }
                Text("Showing video frame $preview (${fmt((preview - 1) * 1000L / fps)})", fontSize = 12.sp)
                val a = range.start.toInt(); val b = range.endInclusive.toInt()
                Text("Keep frames $a–$b · ${fmt((a - 1) * 1000L / fps)} to ${fmt(b * 1000L / fps)} · ${b - a + 1} frames", Modifier.padding(top = 8.dp))
                RangeSlider(range, { v ->
                    if (v.start.toInt() != range.start.toInt()) preview = v.start.toInt() else if (v.endInclusive.toInt() != range.endInclusive.toInt()) preview = v.endInclusive.toInt()
                    range = v
                }, valueRange = 1f..frames.toFloat(), modifier = Modifier.testTag("videoTrim"))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    TextButton({ preview = (preview - 1).coerceAtLeast(1) }) { Text("◀ frame") }
                    TextButton({ preview = (preview + 1).coerceAtMost(frames) }) { Text("frame ▶") }
                    TextButton({ range = preview.toFloat()..maxOf(preview, b).toFloat() }) { Text("Start here") }
                    TextButton({ range = minOf(a, preview).toFloat()..preview.toFloat() }) { Text("End here") }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Place on the timeline from frame $startAt", Modifier.weight(1f))
                    TextButton({ startAt = (startAt - 1).coerceAtLeast(1) }) { Text("−") }
                    TextButton({ startAt++ }) { Text("+") }
                }
                if (fps != controller.animation.fps) Text("The video is $fps fps and the animation ${controller.animation.fps} fps: one video frame per animation frame, as Blender plays a clip.", fontSize = 11.sp)
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = done) { Text("Cancel") }
                    Button(onClick = {
                        val r = controller.addReference(controller.layerName(controller.selectedLayer))
                        r.set(Rotoscope.Kind.MOVIE, listOf(uri.toString()), frames)
                        r.trim(a, b); r.start = startAt
                        if (controller.animation.sceneEnd < startAt + r.usedFrames - 1) controller.animation.setSceneEnd(startAt + r.usedFrames - 1)
                        controller.rotoscopeChanged(); controller.history.markEdit()
                        com.smitnk.projectgrease.CrashLog.event("IMPORT video frames $a..$b of $frames from frame $startAt")
                        toast(context, "Video added: frames $startAt–${startAt + r.usedFrames - 1}")
                        done()
                    }, Modifier.testTag("videoAdd")) { Text("Add") }
                }
            }
        }
    }
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
                    detectDragGestures(onDragStart = { o ->
                        val xa = a.toFloat() / durationMs * size.width; val xb = b.toFloat() / durationMs * size.width
                        which = if (kotlin.math.abs(o.x - xa) < kotlin.math.abs(o.x - xb)) 1 else 2
                    }) { change, _ ->
                        change.consume()
                        val ms = (change.position.x / size.width * durationMs).toLong().coerceIn(0L, durationMs)
                        val minLen = 1000L / fps
                        if (which == 1) a = ms.coerceAtMost(b - minLen) else b = ms.coerceAtLeast(a + minLen)
                        pos = if (which == 1) a else b
                    }
                }) {
                    val p = peaks ?: FloatArray(0)
                    val mid = size.height / 2
                    val xa = a.toFloat() / durationMs * size.width; val xb = b.toFloat() / durationMs * size.width
                    var x = 0f
                    while (x < size.width && p.isNotEmpty()) {
                        val v = p[((x / size.width) * p.size).toInt().coerceIn(0, p.size - 1)]
                        val h = (size.height * 0.45f * v).coerceAtLeast(1f)
                        drawLine(if (x in xa..xb) pink else pink.copy(alpha = 0.3f), Offset(x, mid - h), Offset(x, mid + h), 2f)
                        x += 3f
                    }
                    for (hx in listOf(xa, xb)) {
                        drawRoundRect(pink, Offset(hx - 12f, mid - 40f), Size(24f, 80f), CornerRadius(8f, 8f))
                        drawLine(Color.White, Offset(hx, mid - 16f), Offset(hx, mid + 16f), 2f)
                    }
                    val xp = pos.toFloat() / durationMs * size.width
                    drawLine(pink, Offset(xp, 0f), Offset(xp, size.height), 2f)
                }
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
