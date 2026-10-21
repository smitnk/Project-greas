package com.smitnk.projectgrease.editor

import org.json.JSONArray
import org.json.JSONObject

/**
 * Rotoscope reference: Blender's Image Empty used as a reference (an Image datablock with an
 * ImageUser), one image, an image sequence or a movie, drawn under the strokes with its opacity.
 * The shown image follows the timeline through BKE_image_user_frame_get.
 */
class Rotoscope {
    enum class Kind { IMAGE, SEQUENCE, MOVIE }
    var kind = Kind.IMAGE; private set
    var uris: List<String> = emptyList(); private set
    /** ImageUser frames (number of frames of the sequence / movie; 1 for a still image). */
    var frames = 0; private set
    var visible = true
    /** Name of the layer this reference belongs to ("" = none: always shown, at the bottom). */
    var layer = ""
    var opacity = 0.5f; private set
    /** ImageUser sfra (frame_start), offset (frame_offset), cycl (use_cyclic). */
    var start = 1
    var offset = 0
    var cyclic = false
    /** Placement like a Blender Image Empty: offset from the fitted centre (canvas px), size (1 = fit
     *  inside the canvas) and rotation (radians). */
    var dx = 0f; var dy = 0f; var scale = 1f; var rotation = 0f
    /** Squash / stretch: width and height factors on top of [scale] (an Image Empty's non-uniform scale). */
    var stretchX = 1f; var stretchY = 1f
    /** Trim: how many frames of the video / sequence are used from frame offset + 1 (ImageUser.frame_duration;
     *  0 = all of them). */
    var duration = 0
    /** Scene frames this reference is shown on (empty = every frame): a photo placed on particular frames. */
    val onFrames: MutableSet<Int> = sortedSetOf()
    fun resetPlacement() { dx = 0f; dy = 0f; scale = 1f; rotation = 0f; stretchX = 1f; stretchY = 1f }
    fun moveBy(x: Float, y: Float, zoom: Float = 1f, turn: Float = 0f) {
        if (!(x.isFinite() && y.isFinite() && zoom.isFinite() && turn.isFinite())) return
        dx += x; dy += y; scale = (scale * zoom).coerceIn(0.05f, 20f); rotation += turn
    }
    val isEmpty get() = uris.isEmpty() || frames <= 0

    fun set(kind: Kind, uris: List<String>, frames: Int) {
        this.kind = kind; this.uris = uris; this.frames = frames.coerceAtLeast(if (uris.isEmpty()) 0 else 1)
        start = 1; offset = 0; duration = 0; onFrames.clear(); resetPlacement()
    }
    /** Frames used after trimming. */
    val usedFrames get() = if (duration > 0) duration.coerceAtMost(frames - offset.coerceAtLeast(0)) else frames
    /** Trims to the source frames [first]..[last] (1-based, inclusive). */
    fun trim(first: Int, last: Int) {
        if (frames <= 0) return
        val a = first.coerceIn(1, frames); val b = last.coerceIn(a, frames)
        offset = a - 1; duration = b - a + 1
    }
    /** A copy (same file, placement and timing), for pasting on other frames. */
    fun copy(): Rotoscope = Rotoscope().also { it.fromJson(toJson()) }
    fun clear() { uris = emptyList(); frames = 0 }
    fun setOpacity(v: Float) { if (v.isFinite()) opacity = v.coerceIn(0f, 1f) }

    /** 1-based image / movie frame shown at scene frame [cfra], or 0 for none. A still image shows always. */
    fun imageFrame(cfra: Int): Int {
        if (isEmpty) return 0
        if (onFrames.isNotEmpty() && cfra !in onFrames) return 0
        if (kind == Kind.IMAGE) return 1
        if (duration <= 0) return userFrameGet(frames, start, offset, cyclic, cfra).takeIf { it in 1..frames } ?: 0
        val len = usedFrames
        if (len <= 0) return 0
        // a trimmed clip shows nothing outside its frames (untrimmed, Blender holds the end frames)
        if (!cyclic && (cfra < start || cfra >= start + len)) return 0
        val n = userFrameGet(len, start, offset, cyclic, cfra)
        return if (n in 1..frames) n else 0
    }

    fun toJson(): JSONObject = JSONObject().put("kind", kind.name).put("uris", JSONArray(uris)).put("frames", frames)
        .put("visible", visible).put("layer", layer).put("opacity", opacity.toDouble()).put("start", start).put("offset", offset).put("cyclic", cyclic)
        .put("dx", dx.toDouble()).put("dy", dy.toDouble()).put("scale", scale.toDouble()).put("rotation", rotation.toDouble())
        .put("stretchX", stretchX.toDouble()).put("stretchY", stretchY.toDouble()).put("duration", duration)
        .put("onFrames", JSONArray(onFrames.toList()))

    fun fromJson(o: JSONObject?) {
        if (o == null) { clear(); return }
        val list = o.optJSONArray("uris")?.let { a -> (0 until a.length()).map { a.getString(it) } } ?: emptyList()
        set(runCatching { Kind.valueOf(o.optString("kind")) }.getOrDefault(Kind.IMAGE), list, o.optInt("frames", list.size))
        visible = o.optBoolean("visible", true); layer = o.optString("layer", ""); setOpacity(o.optDouble("opacity", 0.5).toFloat())
        start = o.optInt("start", 1); offset = o.optInt("offset", 0); cyclic = o.optBoolean("cyclic", false)
        dx = o.optDouble("dx", 0.0).toFloat(); dy = o.optDouble("dy", 0.0).toFloat()
        scale = o.optDouble("scale", 1.0).toFloat().coerceIn(0.05f, 20f); rotation = o.optDouble("rotation", 0.0).toFloat()
        stretchX = o.optDouble("stretchX", 1.0).toFloat().coerceIn(0.05f, 20f); stretchY = o.optDouble("stretchY", 1.0).toFloat().coerceIn(0.05f, 20f)
        duration = o.optInt("duration", 0).coerceAtLeast(0)
        onFrames.clear(); o.optJSONArray("onFrames")?.let { a -> for (i in 0 until a.length()) onFrames.add(a.optInt(i)) }
    }

    companion object {
        /** BKE_image_user_frame_get (image.cc, Blender 3.6.23), statement by statement. */
        fun userFrameGet(frames: Int, sfra: Int, offset: Int, cycl: Boolean, cfraIn: Int): Int {
            val len = frames
            if (len == 0) return 0
            var cfra = cfraIn - sfra + 1
            if (cycl) {
                cfra %= len
                if (cfra < 0) cfra += len
                if (cfra == 0) cfra = len
            }
            if (cfra < 0) cfra = 0 else if (cfra > len) cfra = len
            var framenr = cfra
            if (framenr > frames) framenr = frames
            if (cycl) {
                framenr %= len
                while (framenr < 0) framenr += len
                if (framenr == 0) framenr = len
            }
            framenr += offset
            return framenr
        }
    }
}
