package com.smitnk.projectgrease.ui

import android.content.Context
import android.media.MediaPlayer
import android.net.Uri
import android.os.Handler
import android.os.Looper
import com.smitnk.projectgrease.editor.AudioStrip

/**
 * Plays the sound strips with the animation: on play every strip is started at the time of the
 * current frame (or later, when it begins after it); on pause / stop / loop wrap they stop and
 * resync. One MediaPlayer per strip, kept while the strip exists.
 */
class AudioPlayback(private val context: Context) {
    /** For the export's audio mix. */
    val appContext: Context get() = context
    private val players = HashMap<AudioStrip, MediaPlayer>()
    private val handler = Handler(Looper.getMainLooper())

    private fun player(s: AudioStrip): MediaPlayer? = players[s] ?: runCatching {
        MediaPlayer().apply { setDataSource(context, Uri.parse(s.uri)); prepare() }
    }.onFailure { com.smitnk.projectgrease.CrashLog.event("AUDIO can't open ${s.name}: ${it.message}") }
        .getOrNull()?.also { players[s] = it; if (s.durationMs <= 0) s.durationMs = it.duration.toLong().coerceAtLeast(0) }

    /** Starts (playing = true) or stops every strip for scene frame [frame]. */
    fun sync(strips: List<AudioStrip>, playing: Boolean, frame: Int, fps: Int) {
        handler.removeCallbacksAndMessages(null)
        // strips removed since: release their players
        players.keys.filter { s -> strips.none { it === s } }.forEach { runCatching { players.remove(it)?.apply { stop(); release() } } }
        com.smitnk.projectgrease.CrashLog.event("AUDIO sync playing=$playing frame $frame: ${strips.size} clips, ${players.size} players")
        for (s in strips) {
            val p = players[s]
            if (p != null && runCatching { p.isPlaying }.getOrDefault(false)) runCatching { p.pause() }
            if (!playing || s.muted) continue
            val mp = player(s) ?: continue
            runCatching { mp.setVolume(s.volume.coerceAtMost(1f), s.volume.coerceAtMost(1f)) }
            val ms = s.offsetMs(frame, fps)
            val len = s.lengthMs
            if (len <= 0 || ms >= len) continue
            if (ms >= 0) runCatching { mp.seekTo(s.fileMs(frame, fps).toInt()); mp.start() }
            else handler.postDelayed({ runCatching { mp.seekTo(s.trimStartMs.toInt()); mp.start() } }, -ms)
            // the trim's end: stop there
            handler.postDelayed({ runCatching { if (mp.isPlaying) mp.pause() } }, len - ms)
        }
    }

    /** Reads a file's length (for a strip just added). */
    fun durationOf(uri: Uri): Long = runCatching {
        val mr = android.media.MediaMetadataRetriever()
        try { mr.setDataSource(context, uri); mr.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: 0L }
        finally { mr.release() }
    }.getOrDefault(0L)

    fun release() { handler.removeCallbacksAndMessages(null); players.values.forEach { runCatching { it.release() } }; players.clear() }
}
