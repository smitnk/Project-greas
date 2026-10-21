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
    if (controller.rotoscopeLoader?.frame(r, 1) == null) { controller.removeReference(); toast(context, "Couldn't open this photo (format not supported)"); return }
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
    LaunchedEffect(uri) { durationMs = withContext(Dispatchers.IO) { controller.audioPlayback?.durationOf(uri) ?: 0L } }
    val player = remember(uri) { mutableStateOf<MediaPlayer?>(null) }
    DisposableEffect(uri) { onDispose { player.value?.release(); player.value = null } }
    val name = remember(uri) { displayName(context, uri) }
    Dialog(onDismissRequest = done, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxWidth().padding(12.dp), shape = MaterialTheme.shapes.large) {
            Column(Modifier.verticalScroll(rememberScrollState()).padding(16.dp)) {
                Text("Trim audio", style = MaterialTheme.typography.headlineSmall)
                Text(name, fontSize = 12.sp)
                when {
                    durationMs < 0 -> { CircularProgressIndicator(Modifier.padding(24.dp)); return@Column }
                    durationMs == 0L -> { Text("Couldn't read this file as audio."); TextButton(onClick = done) { Text("Skip") }; return@Column }
                }
                var range by remember(uri) { mutableStateOf(0f..durationMs.toFloat()) }
                var startAt by remember(uri) { mutableIntStateOf(controller.animation.currentFrame) }
                var playing by remember(uri) { mutableStateOf(false) }
                val fps = controller.animation.fps.coerceAtLeast(1)
                val a = range.start.toLong(); val b = range.endInclusive.toLong()
                Text("Keep ${fmt(a)} to ${fmt(b)} (${fmt(b - a)}) · ${((b - a) * fps + 999) / 1000} frames at $fps fps", Modifier.padding(top = 8.dp))
                RangeSlider(range, { range = it }, valueRange = 0f..durationMs.toFloat(), modifier = Modifier.testTag("audioTrim"))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Button(onClick = {
                        val p = player.value ?: runCatching { MediaPlayer().apply { setDataSource(context, uri); prepare() } }.getOrNull()?.also { player.value = it }
                        if (p != null) {
                            if (playing) { p.pause(); playing = false }
                            else { p.seekTo(a.toInt()); p.start(); playing = true }
                        }
                    }) { Text(if (playing) "Stop" else "Play the kept part") }
                }
                // stops at the trim's end while previewing
                LaunchedEffect(playing, a, b) {
                    while (playing) {
                        val p = player.value ?: break
                        if (!p.isPlaying || p.currentPosition >= b) { runCatching { p.pause() }; playing = false }
                        kotlinx.coroutines.delay(50)
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Starts at timeline frame $startAt", Modifier.weight(1f))
                    TextButton({ startAt = (startAt - 1).coerceAtLeast(1) }) { Text("−") }
                    TextButton({ startAt++ }) { Text("+") }
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = done) { Text("Cancel") }
                    Button(onClick = {
                        val s = controller.addAudio(uri.toString(), name, durationMs, a, b, startAt)
                        if (s != null) toast(context, "Audio added: frames ${s.start}–${s.endFrame(fps)}")
                        done()
                    }, Modifier.testTag("audioAdd")) { Text("Add") }
                }
            }
        }
    }
}

/** The audio tracks: each a bar on a frame ruler; drag the bar to move it to any frame, re-trim,
 *  volume, mute, remove. Changes are one undo step each. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AudioSheet(controller: EditorController, m: MediaImport, onDismiss: () -> Unit, redraw: () -> Unit) {
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val observed = tick
    val fps = controller.animation.fps.coerceAtLeast(1)
    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(Modifier.verticalScroll(rememberScrollState()).padding(16.dp)) {
            Text("Audio tracks", style = MaterialTheme.typography.headlineSmall)
            Text("Drag a bar to move the sound to another frame. They play with the animation and go into the MP4 export.", fontSize = 12.sp)
            Button(onClick = { m.importAudio() }, Modifier.fillMaxWidth().padding(vertical = 6.dp)) { Text("Import audio") }
            val last = maxOf(controller.animation.timelineEnd, controller.audioStrips.maxOfOrNull { it.endFrame(fps) } ?: 1) + 24
            val cell = 14.dp
            val cellPx = with(LocalDensity.current) { cell.toPx() }
            val scroll = rememberScrollState()
            controller.audioStrips.toList().forEachIndexed { index, a ->
                Card(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
                    Column(Modifier.padding(10.dp)) {
                        Text(a.name, style = MaterialTheme.typography.titleSmall, maxLines = 1)
                        Text("Frames ${a.start}–${a.endFrame(fps)} · uses ${fmt(a.trimStartMs)}–${fmt(a.trimStartMs + a.lengthMs)} of ${fmt(a.durationMs)}", fontSize = 11.sp)
                        // the lane: a frame ruler with the strip's bar, dragged sideways in whole frames
                        Box(Modifier.fillMaxWidth().height(40.dp).horizontalScroll(scroll)) {
                            var dragRest by remember { mutableFloatStateOf(0f) }
                            Canvas(Modifier.width(cell * last).fillMaxHeight().testTag("audioLane_$index").pointerInput(a) {
                                detectDragGestures(onDragEnd = { dragRest = 0f; controller.audioChanged(record = true); tick++ }) { change, d ->
                                    change.consume()
                                    dragRest += d.x
                                    val steps = (dragRest / cellPx).toInt()
                                    if (steps != 0) { a.start = (a.start + steps).coerceAtLeast(1); dragRest -= steps * cellPx; controller.audioChanged(); tick++ }
                                }
                            }) {
                                @Suppress("UNUSED_VARIABLE") val t = tick
                                for (f in 1..last) if (f % 12 == 1 || f == 1) drawLine(Color.Gray, Offset((f - 1) * cellPx, 0f), Offset((f - 1) * cellPx, 8f), 1f)
                                val cur = controller.animation.currentFrame
                                drawLine(Color(0xFFFF5252), Offset((cur - 0.5f) * cellPx, 0f), Offset((cur - 0.5f) * cellPx, size.height), 2f)
                                val x0 = (a.start - 1) * cellPx; val w = a.frames(fps) * cellPx
                                drawRoundRect(if (a.muted) Color(0xFF777777) else Color(0xFF26A69A), Offset(x0, 10f), Size(w, size.height - 14f), CornerRadius(8f, 8f))
                            }
                        }
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text("Start frame ${a.start}", Modifier.weight(1f), fontSize = 12.sp)
                            TextButton({ a.start = (a.start - 1).coerceAtLeast(1); controller.audioChanged(record = true); tick++ }) { Text("−") }
                            TextButton({ a.start++; controller.audioChanged(record = true); tick++ }) { Text("+") }
                            TextButton({ a.start = controller.animation.currentFrame; controller.audioChanged(record = true); tick++ }) { Text("Here") }
                        }
                        if (a.durationMs > 0) {
                            Text("Trim", fontSize = 12.sp)
                            RangeSlider(a.trimStartMs.toFloat()..(a.trimStartMs + a.lengthMs).toFloat(), { v -> a.trim(v.start.toLong(), v.endInclusive.toLong()); tick++ },
                                valueRange = 0f..a.durationMs.toFloat(), onValueChangeFinished = { controller.audioChanged(record = true) })
                        }
                        Text("Volume ${(a.volume * 100).toInt()}%", fontSize = 12.sp)
                        Slider(a.volume, { a.volume = it; tick++ }, valueRange = 0f..2f, onValueChangeFinished = { controller.audioChanged(record = true) })
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text("Mute", Modifier.weight(1f)); Switch(a.muted, { a.muted = it; controller.audioChanged(record = true); tick++ })
                            TextButton({ controller.removeAudio(a); tick++; redraw() }) { Text("Remove") }
                        }
                    }
                }
            }
            if (controller.audioStrips.isEmpty()) Text("No audio yet.", Modifier.padding(vertical = 12.dp))
        }
    }
}
