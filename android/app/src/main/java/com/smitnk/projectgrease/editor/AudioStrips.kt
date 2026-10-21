package com.smitnk.projectgrease.editor

import org.json.JSONArray
import org.json.JSONObject

/**
 * Sound strips (Blender's sequencer sound strips): an audio file placed on the timeline from
 * [start] (scene frame), with its volume and mute. They play with the animation and are mixed into
 * the MP4 export.
 */
class AudioStrip(var uri: String, var name: String) {
    var start = 1
    var volume = 1f
        set(v) { field = if (v.isFinite()) v.coerceIn(0f, 2f) else 1f }
    var muted = false
    /** Length in milliseconds (0 = unknown). */
    var durationMs = 0L

    /** Milliseconds into the file at scene frame [frame] (negative: before it starts). */
    fun offsetMs(frame: Int, fps: Int): Long = (frame - start).toLong() * 1000L / fps.coerceIn(1, 120)
    /** Whether the strip is sounding at scene frame [frame]. */
    fun playsAt(frame: Int, fps: Int): Boolean {
        val ms = offsetMs(frame, fps)
        return !muted && ms >= 0 && (durationMs <= 0 || ms < durationMs)
    }

    fun toJson(): JSONObject = JSONObject().put("uri", uri).put("name", name).put("start", start)
        .put("volume", volume.toDouble()).put("muted", muted).put("durationMs", durationMs)

    companion object {
        fun fromJson(o: JSONObject?): AudioStrip? {
            val uri = o?.optString("uri").orEmpty()
            if (uri.isEmpty()) return null
            return AudioStrip(uri, o!!.optString("name", "Audio")).apply {
                start = o.optInt("start", 1); volume = o.optDouble("volume", 1.0).toFloat()
                muted = o.optBoolean("muted", false); durationMs = o.optLong("durationMs", 0L)
            }
        }
        fun listToJson(strips: List<AudioStrip>): JSONArray = JSONArray(strips.map { it.toJson() })
        fun listFromJson(a: JSONArray?): List<AudioStrip> =
            if (a == null) emptyList() else (0 until a.length()).mapNotNull { fromJson(a.optJSONObject(it)) }
    }
}
