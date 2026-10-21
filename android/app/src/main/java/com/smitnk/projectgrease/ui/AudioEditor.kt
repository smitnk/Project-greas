package com.smitnk.projectgrease.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.MutableTransitionState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
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
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.smitnk.projectgrease.editor.AudioStrip
import com.smitnk.projectgrease.editor.EditorController
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

private val Pink = Color(0xFFF2416B)
private val PinkLight = Color(0xFFF7A6B8)

internal fun clock(ms: Long) = "%d:%02d.%03d".format(ms / 60000, (ms / 1000) % 60, ms % 1000)

/**
 * The audio timeline (sound strips on Blender sequencer channels): a frame ruler with the playhead,
 * six tracks, each clip with its name and waveform. Tap a clip to select it; drag it to move it in
 * time or to another track; drag its end handles to trim; the toolbar duplicates, sets the volume,
 * deletes or splits it at the playhead (Blender's soft split). Drag the ruler to scrub, empty space to scroll.
 */
@Composable
fun AudioEditor(controller: EditorController, m: MediaImport, onClose: () -> Unit, redraw: () -> Unit) {
    val context = LocalContext.current
    val anim = controller.animation
    val fps = anim.fps.coerceAtLeast(1)
    var tick by remember { mutableIntStateOf(0) }
    var selected by remember { mutableStateOf<AudioStrip?>(null) }
    var scrollPx by remember { mutableFloatStateOf(0f) }
    var volumeOpen by remember { mutableStateOf(false) }
    val shown = remember { MutableTransitionState(false) }.apply { targetState = true }
    val density = LocalDensity.current
    val frameW = with(density) { 18.dp.toPx() }
    val trackH = with(density) { 58.dp.toPx() }
    val rulerH = with(density) { 26.dp.toPx() }
    val handleW = with(density) { 14.dp.toPx() }
    val measurer = rememberTextMeasurer()
    // the newest clip drops in: its alpha animates from 0
    var newest by remember { mutableStateOf<AudioStrip?>(null) }
    var lastCount by remember { mutableIntStateOf(controller.audioStrips.size) }
    if (controller.audioStrips.size > lastCount) { newest = controller.audioStrips.lastOrNull(); selected = newest }
    lastCount = controller.audioStrips.size
    var dropStarted by remember { mutableStateOf(false) }
    val newAlpha by animateFloatAsState(if (dropStarted) 1f else 0f, label = "newClip")
    LaunchedEffect(newest) { dropStarted = false; kotlinx.coroutines.delay(16); dropStarted = true }
    val lift by animateFloatAsState(if (selected != null) 1f else 0f, label = "lift")
    // waveforms of every file, decoded once
    val uris = controller.audioStrips.map { it.uri }.distinct()
    LaunchedEffect(uris) {
        for (u in uris) if (AudioWaveform.cached(u) == null) { withContext(Dispatchers.IO) { AudioWaveform.peaks(context, u) }; tick++ }
    }

    // its own window, over the whole editor (back closes it)
    androidx.compose.ui.window.Dialog(onDismissRequest = { onClose(); redraw() },
        properties = androidx.compose.ui.window.DialogProperties(usePlatformDefaultWidth = false, decorFitsSystemWindows = false)) {
    AnimatedVisibility(shown, enter = slideInVertically { it } + fadeIn(), exit = slideOutVertically { it } + fadeOut()) {
        Surface(Modifier.fillMaxSize().testTag("audioEditor"), color = MaterialTheme.colorScheme.surface) {
            Column(Modifier.fillMaxSize().systemBarsPadding()) {
                Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = { anim.playing.let { if (it) anim.togglePlayback() }; onClose(); redraw() }, Modifier.testTag("audioClose")) {
                        Icon(Icons.Default.KeyboardArrowDown, "Close")
                    }
                    Text("Audio", style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                    Text(clock((anim.currentFrame - 1).toLong() * 1000L / fps), fontSize = 12.sp)
                    Spacer(Modifier.width(12.dp))
                }
                Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                    val allMuted = controller.audioStrips.isNotEmpty() && controller.audioStrips.all { it.muted }
                    IconButton(onClick = { val m2 = !allMuted; controller.audioStrips.forEach { it.muted = m2 }; controller.audioChanged(record = true); tick++ }) {
                        Icon(if (allMuted) Icons.Default.VolumeOff else Icons.Default.VolumeUp, "Mute all")
                    }
                    Spacer(Modifier.weight(1f))
                    IconButton(onClick = { controller.selectFrame(1); redraw() }) { Icon(Icons.Default.SkipPrevious, "Start") }
                    IconButton(onClick = { anim.togglePlayback(); redraw() }) { Icon(if (anim.playing) Icons.Default.Pause else Icons.Default.PlayArrow, "Play") }
                    IconButton(onClick = { controller.selectFrame(maxOf(anim.sceneEnd, 1)); redraw() }) { Icon(Icons.Default.SkipNext, "End") }
                    Spacer(Modifier.weight(1f))
                    IconButton(onClick = { m.importAudio() }, Modifier.testTag("audioAdd")) { Icon(Icons.Default.Add, "Add audio", tint = Pink) }
                }
                Box(Modifier.fillMaxWidth().weight(1f)) {
                    @Suppress("UNUSED_VARIABLE") val t = tick
                    val cur = anim.currentFrame
                    Canvas(Modifier.fillMaxSize().testTag("audioTracks").pointerInput(fps) {
                        awaitEachGesture {
                            val down = awaitFirstDown()
                            val p0 = down.position
                            fun frameAt(x: Float) = ((x + scrollPx) / frameW).toInt() + 1
                            fun trackAt(y: Float) = ((y - rulerH) / trackH).toInt().coerceIn(0, AudioStrip.MAX_TRACKS - 1)
                            val onRuler = p0.y < rulerH
                            val hit = if (onRuler) null else controller.audioStrips.lastOrNull { a ->
                                val x0 = (a.start - 1) * frameW - scrollPx; val x1 = x0 + a.frames(fps) * frameW
                                a.track == trackAt(p0.y) && p0.x >= x0 - handleW && p0.x <= x1 + handleW
                            }
                            // what the drag does: 0 scroll, 1 move, 2 left trim, 3 right trim, 4 scrub
                            val mode = when {
                                onRuler -> 4
                                hit == null -> 0
                                hit === selected && p0.x < (hit.start - 1) * frameW - scrollPx + handleW -> 2
                                hit === selected && p0.x > (hit.start - 1 + hit.frames(fps)) * frameW - scrollPx - handleW -> 3
                                else -> 1
                            }
                            if (mode == 4) { controller.selectFrame(frameAt(p0.x).coerceAtLeast(1)); redraw() }
                            var dx = 0f; var dy = 0f; var moved = false
                            val startFrame = hit?.start ?: 0; val startTrack = hit?.track ?: 0
                            val startTrimMs = hit?.trimStartMs ?: 0L; val startEndMs = hit?.let { it.trimStartMs + it.lengthMs } ?: 0L
                            while (true) {
                                val ev = awaitPointerEvent()
                                val c = ev.changes.firstOrNull { it.id == down.id } ?: break
                                if (!c.pressed) break
                                val d = c.positionChange()
                                if (d.x == 0f && d.y == 0f) continue
                                dx += d.x; dy += d.y
                                if (!moved && kotlin.math.hypot(dx, dy) < viewConfiguration.touchSlop) continue
                                moved = true; c.consume()
                                val df = (dx / frameW).toInt()
                                val msPerFrame = 1000L / fps
                                when (mode) {
                                    0 -> { scrollPx = (scrollPx - d.x).coerceAtLeast(0f) }
                                    4 -> { controller.selectFrame(frameAt(c.position.x).coerceAtLeast(1)); redraw() }
                                    1 -> if (!controller.audioTrackLocked(startTrack)) {
                                        hit!!.start = (startFrame + df).coerceAtLeast(1)
                                        val tr = (startTrack + (dy / trackH).let { kotlin.math.round(it).toInt() }).coerceIn(0, AudioStrip.MAX_TRACKS - 1)
                                        if (!controller.audioTrackLocked(tr)) hit.track = tr
                                        controller.audioChanged()
                                    }
                                    2 -> if (!controller.audioTrackLocked(startTrack)) { // left handle: the clip's start and the file offset move together
                                        val newTrim = (startTrimMs + df * msPerFrame).coerceIn(0L, startEndMs - msPerFrame)
                                        val realDf = ((newTrim - startTrimMs) / msPerFrame).toInt()
                                        if (startFrame + realDf >= 1) { hit!!.trimStartMs = newTrim; hit.trimEndMs = startEndMs; hit.start = startFrame + realDf; controller.audioChanged() }
                                    }
                                    3 -> if (!controller.audioTrackLocked(startTrack)) {
                                        val max = if (hit!!.durationMs > 0) hit.durationMs else Long.MAX_VALUE
                                        hit.trimEndMs = (startEndMs + df * msPerFrame).coerceIn(startTrimMs + msPerFrame, max); controller.audioChanged()
                                    }
                                }
                                tick++
                            }
                            if (!moved && mode != 4) { selected = hit; volumeOpen = false }
                            if (moved && mode in 1..3) controller.audioChanged(record = true)
                            tick++
                        }
                    }) {
                        @Suppress("UNUSED_VARIABLE") val t2 = tick
                        val last = (size.width + scrollPx) / frameW + 1
                        // ruler: a tick per frame, a label every fps frames (seconds)
                        drawRect(Color(0x11000000), Offset.Zero, Size(size.width, rulerH))
                        var f = (scrollPx / frameW).toInt() + 1
                        while (f <= last) {
                            val x = (f - 1) * frameW - scrollPx
                            val sec = (f - 1) % fps == 0
                            drawLine(Color.Gray, Offset(x, rulerH - if (sec) 12f else 5f), Offset(x, rulerH), 1f)
                            if (sec) drawText(measurer, clock((f - 1).toLong() * 1000L / fps).dropLast(4), Offset(x + 3f, 2f), TextStyle(fontSize = 9.sp, color = Color.Gray))
                            f++
                        }
                        // tracks
                        for (tr in 0 until AudioStrip.MAX_TRACKS) {
                            val y = rulerH + tr * trackH
                            drawLine(Color(0x22000000), Offset(0f, y + trackH), Offset(size.width, y + trackH), 1f)
                            if (controller.audioTrackLocked(tr)) drawRect(Color(0x0F000000), Offset(0f, y), Size(size.width, trackH))
                        }
                        // clips
                        for (a in controller.audioStrips) {
                            val x0 = (a.start - 1) * frameW - scrollPx; val w = a.frames(fps) * frameW
                            if (x0 > size.width || x0 + w < 0) continue
                            val y = rulerH + a.track * trackH + 6f; val h = trackH - 12f
                            val isSel = a === selected
                            val alpha = if (a === newest) newAlpha else 1f
                            val body = if (a.muted) Color.Gray else Pink
                            drawRoundRect(body.copy(alpha = 0.18f * alpha), Offset(x0, y), Size(w, h), CornerRadius(10f, 10f))
                            // waveform of the used part of the file
                            val peaks = AudioWaveform.cached(a.uri)
                            val mid = y + h * 0.6f
                            clipRect(x0, y, x0 + w, y + h) {
                                if (peaks != null && peaks.isNotEmpty() && a.durationMs > 0) {
                                    var px = 0f
                                    while (px < w) {
                                        val ms = a.trimStartMs + (px / frameW * 1000f / fps).toLong()
                                        val v = peaks[((ms * peaks.size) / a.durationMs).toInt().coerceIn(0, peaks.size - 1)] * a.volume.coerceAtMost(1f)
                                        val hh = (h * 0.38f * v).coerceAtLeast(1f)
                                        drawLine(body.copy(alpha = alpha), Offset(x0 + px, mid - hh), Offset(x0 + px, mid + hh), 2f)
                                        px += 3f
                                    }
                                } else drawLine(body.copy(alpha = alpha), Offset(x0, mid), Offset(x0 + w, mid), 3f)
                                drawText(measurer, a.name, Offset(x0 + 8f, y + 4f), TextStyle(fontSize = 10.sp, color = body.copy(alpha = alpha)), maxLines = 1)
                            }
                            if (isSel) {
                                val lw = 2f + 2f * lift
                                drawRoundRect(Pink, Offset(x0, y), Size(w, h), CornerRadius(10f, 10f), style = Stroke(lw))
                                drawRoundRect(Pink, Offset(x0 - handleW / 2, y), Size(handleW, h), CornerRadius(6f, 6f))
                                drawRoundRect(Pink, Offset(x0 + w - handleW / 2, y), Size(handleW, h), CornerRadius(6f, 6f))
                                drawLine(Color.White, Offset(x0, y + h * 0.35f), Offset(x0, y + h * 0.65f), 2f)
                                drawLine(Color.White, Offset(x0 + w, y + h * 0.35f), Offset(x0 + w, y + h * 0.65f), 2f)
                            }
                        }
                        // playhead
                        val px = (cur - 1) * frameW - scrollPx
                        drawLine(Pink, Offset(px, 0f), Offset(px, size.height), 2.5f)
                        drawCircle(Pink, 6f, Offset(px, rulerH - 4f))
                    }
                    // per-track mute / lock, at the left edge over the lanes
                    Column(Modifier.padding(top = with(density) { rulerH.toDp() })) {
                        for (tr in 0 until AudioStrip.MAX_TRACKS) {
                            val clips = controller.audioStrips.filter { it.track == tr }
                            Column(Modifier.height(with(density) { trackH.toDp() }).width(36.dp).background(MaterialTheme.colorScheme.surface.copy(alpha = 0.85f)),
                                verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
                                val muted = clips.isNotEmpty() && clips.all { it.muted }
                                Icon(if (muted) Icons.Default.VolumeOff else Icons.Default.VolumeUp, "Mute track ${tr + 1}",
                                    Modifier.size(20.dp).pointerInput(tr, muted) { awaitEachGesture { awaitFirstDown().consume(); clips.forEach { it.muted = !muted }; controller.audioChanged(record = true); tick++ } },
                                    tint = Color.Gray)
                                Spacer(Modifier.height(6.dp))
                                val locked = controller.audioTrackLocked(tr)
                                Icon(if (locked) Icons.Default.Lock else Icons.Default.LockOpen, "Lock track ${tr + 1}",
                                    Modifier.size(18.dp).pointerInput(tr, locked) { awaitEachGesture { awaitFirstDown().consume(); controller.setAudioTrackLocked(tr, !locked); tick++ } },
                                    tint = Color.Gray)
                            }
                        }
                    }
                    // the selected clip's toolbar pops in above the tracks
                    val sel = selected?.takeIf { it in controller.audioStrips }
                    // (a scoped AnimatedVisibility is ambiguous inside the Column: animate scale / alpha directly)
                    val pop by animateFloatAsState(if (sel != null) 1f else 0f, label = "toolbar")
                    if (sel != null) {
                        Surface(Modifier.align(Alignment.TopCenter).graphicsLayer { scaleX = pop; scaleY = pop; alpha = pop }, shape = RoundedCornerShape(20.dp), tonalElevation = 6.dp, shadowElevation = 6.dp) {
                            Row(Modifier.padding(horizontal = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                                IconButton(onClick = { sel?.let { controller.duplicateAudio(it) }?.let { selected = it }; tick++ }, Modifier.testTag("audioDuplicate")) { Icon(Icons.Default.ContentCopy, "Duplicate") }
                                IconButton(onClick = { volumeOpen = !volumeOpen }, Modifier.testTag("audioVolume")) { Icon(Icons.Default.Edit, "Volume") }
                                IconButton(onClick = { sel?.let { controller.removeAudio(it) }; selected = null; tick++; redraw() }, Modifier.testTag("audioDelete")) { Icon(Icons.Default.Delete, "Delete") }
                                IconButton(onClick = { sel?.let { controller.splitAudio(it) }; tick++ }, Modifier.testTag("audioSplit")) { Icon(Icons.Default.ContentCut, "Split at the playhead") }
                            }
                        }
                    }
                    if (sel != null && volumeOpen) {
                        Surface(Modifier.align(Alignment.BottomCenter).fillMaxWidth().padding(12.dp), shape = RoundedCornerShape(16.dp), tonalElevation = 6.dp) {
                            Column(Modifier.padding(12.dp)) {
                                Text("${sel.name} · volume ${(sel.volume * 100).toInt()}%", fontSize = 12.sp, maxLines = 1)
                                Slider(sel.volume, { sel.volume = it; tick++ }, valueRange = 0f..2f, onValueChangeFinished = { controller.audioChanged(record = true) })
                            }
                        }
                    }
                    if (controller.audioStrips.isEmpty())
                        Text("Tap + to add audio. Drag clips to move them, drag their ends to trim.", Modifier.align(Alignment.Center).padding(24.dp), color = Color.Gray)
                }
            }
        }
    }
}
}
