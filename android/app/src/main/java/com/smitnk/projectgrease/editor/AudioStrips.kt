package com.smitnk.projectgrease.editor

import org.json.JSONArray
import org.json.JSONObject

/**
 * Sound strips (Blender's sequencer sound strips): an audio file placed on the timeline from
 * [start] (scene frame), trimmed to [trimStartMs]..[trimEndMs] of the file (the strip's frame
 * offsets), with its volume and mute. They play with the animation and are mixed into the MP4 export.
 */
class AudioStrip(var uri: String, var name: String) {
    var start = 1
    var volume = 1f
        set(v) { field = if (v.isFinite()) v.coerceIn(0f, 2f) else 1f }
    var muted = false
    /** Length of the whole file in milliseconds (0 = unknown). */
    var durationMs = 0L
    /** Trim: the part of the file used (end 0 = to the end of the file). */
    var trimStartMs = 0L
    var trimEndMs = 0L

    /** Milliseconds of the trimmed sound that are used. */
    val lengthMs: Long get() = ((if (trimEndMs > 0) trimEndMs else durationMs) - trimStartMs).coerceAtLeast(0)
    /** Scene frames the strip covers at [fps]. */
    fun frames(fps: Int): Int = ((lengthMs * fps.coerceIn(1, 120) + 999) / 1000).toInt().coerceAtLeast(1)
    fun endFrame(fps: Int) = start + frames(fps) - 1
    /** Milliseconds since the strip started at scene frame [frame] (negative: before it starts). */
    fun offsetMs(frame: Int, fps: Int): Long = (frame - start).toLong() * 1000L / fps.coerceIn(1, 120)
    /** Position in the file at scene frame [frame]. */
    fun fileMs(frame: Int, fps: Int): Long = trimStartMs + offsetMs(frame, fps)
    /** Whether the strip is sounding at scene frame [frame]. */
    fun playsAt(frame: Int, fps: Int): Boolean {
        val ms = offsetMs(frame, fps)
        return !muted && ms >= 0 && (durationMs <= 0 || ms < lengthMs)
    }
    /** Trims to [fromMs]..[toMs] of the file. */
    fun trim(fromMs: Long, toMs: Long) {
        val end = if (durationMs > 0) toMs.coerceIn(1, durationMs) else toMs.coerceAtLeast(1)
        trimStartMs = fromMs.coerceIn(0, end - 1); trimEndMs = end
    }

    fun toJson(): JSONObject = JSONObject().put("uri", uri).put("name", name).put("start", start)
        .put("volume", volume.toDouble()).put("muted", muted).put("durationMs", durationMs)
        .put("trimStartMs", trimStartMs).put("trimEndMs", trimEndMs)

    companion object {
        fun fromJson(o: JSONObject?): AudioStrip? {
            val uri = o?.optString("uri").orEmpty()
            if (uri.isEmpty()) return null
            return AudioStrip(uri, o!!.optString("name", "Audio")).apply {
                start = o.optInt("start", 1); volume = o.optDouble("volume", 1.0).toFloat()
                muted = o.optBoolean("muted", false); durationMs = o.optLong("durationMs", 0L)
                trimStartMs = o.optLong("trimStartMs", 0L); trimEndMs = o.optLong("trimEndMs", 0L)
            }
        }
        fun listToJson(strips: List<AudioStrip>): JSONArray = JSONArray(strips.map { it.toJson() })
        fun listFromJson(a: JSONArray?): List<AudioStrip> =
            if (a == null) emptyList() else (0 until a.length()).mapNotNull { fromJson(a.optJSONObject(it)) }
    }
}
