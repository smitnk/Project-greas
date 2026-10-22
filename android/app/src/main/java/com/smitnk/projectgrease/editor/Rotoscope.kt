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
    /** Part of the animation (an imported video layer): drawn in exports too, not only as a guide. */
    var inExport = false
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

    // ---- placement geometry (what the presenter draws: aspect fit in the canvas, then size,
    //      squash / stretch and rotation about the centre). Image pixel (0,0) is the top-left corner. ----
    /** Pixel size of the shown image (set when it is decoded; 0 = not known yet). */
    var pixW = 0; var pixH = 0
    private fun fit(cw: Int, ch: Int) = minOf(cw.toFloat() / pixW, ch.toFloat() / pixH)
    /** Half width / height on the canvas. */
    fun halfSize(cw: Int, ch: Int): Pair<Float, Float> {
        if (pixW <= 0 || pixH <= 0) return 0f to 0f
        val k = fit(cw, ch)
        return pixW * k * scale * stretchX * 0.5f to pixH * k * scale * stretchY * 0.5f
    }
    fun center(cw: Int, ch: Int) = cw * 0.5f + dx to ch * 0.5f + dy
    /** Canvas point of the local offset (u, v) from the centre (before rotation). */
    fun localToCanvas(u: Float, v: Float, cw: Int, ch: Int): Pair<Float, Float> {
        val (mx, my) = center(cw, ch); val c = kotlin.math.cos(rotation); val s = kotlin.math.sin(rotation)
        return mx + c * u - s * v to my + s * u + c * v
    }
    fun canvasToLocal(x: Float, y: Float, cw: Int, ch: Int): Pair<Float, Float> {
        val (mx, my) = center(cw, ch); val c = kotlin.math.cos(rotation); val s = kotlin.math.sin(rotation)
        val px = x - mx; val py = y - my
        return c * px + s * py to -s * px + c * py
    }
    /** Image pixel (fractional) under canvas point (x, y). */
    fun canvasToPixel(x: Float, y: Float, cw: Int, ch: Int): Pair<Float, Float> {
        val (hw, hh) = halfSize(cw, ch)
        if (hw <= 0f || hh <= 0f) return Float.NaN to Float.NaN
        val (u, v) = canvasToLocal(x, y, cw, ch)
        return (u + hw) / (2 * hw) * pixW to (v + hh) / (2 * hh) * pixH
    }
    /** Canvas pixels per image pixel along x and y. */
    fun pixelScale(cw: Int, ch: Int): Pair<Float, Float> {
        val (hw, hh) = halfSize(cw, ch); return if (pixW > 0) 2 * hw / pixW to 2 * hh / pixH else 0f to 0f
    }
    /** Corners (top-left, top-right, bottom-right, bottom-left) on the canvas. */
    fun corners(cw: Int, ch: Int): List<Pair<Float, Float>> {
        val (hw, hh) = halfSize(cw, ch)
        return listOf(-hw to -hh, hw to -hh, hw to hh, -hw to hh).map { localToCanvas(it.first, it.second, cw, ch) }
    }
    /** Transform box handles: 0-3 corners (TL, TR, BR, BL: free resize, squash / stretch), 4-7 edge
     *  middles (T, R, B, L: one direction), 8 rotation (above the top edge), 9 inside (move). */
    fun handlePoints(cw: Int, ch: Int, rotateGap: Float): List<Pair<Float, Float>> {
        val (hw, hh) = halfSize(cw, ch)
        val local = listOf(-hw to -hh, hw to -hh, hw to hh, -hw to hh, 0f to -hh, hw to 0f, 0f to hh, -hw to 0f, 0f to -hh - rotateGap)
        return local.map { localToCanvas(it.first, it.second, cw, ch) }
    }
    /** Drags handle [h] to canvas point (x, y), resizing about the centre (Blender's median pivot). */
    fun dragHandle(h: Int, x: Float, y: Float, cw: Int, ch: Int) {
        if (pixW <= 0 || pixH <= 0 || !x.isFinite() || !y.isFinite()) return
        val k = fit(cw, ch)
        val (u, v) = canvasToLocal(x, y, cw, ch)
        val baseW = pixW * k * scale * 0.5f; val baseH = pixH * k * scale * 0.5f
        fun sx(hw: Float) { stretchX = (kotlin.math.abs(hw) / baseW).coerceIn(0.02f, 50f) }
        fun sy(hh: Float) { stretchY = (kotlin.math.abs(hh) / baseH).coerceIn(0.02f, 50f) }
        when (h) {
            0, 1, 2, 3 -> { sx(u); sy(v) }
            4, 6 -> sy(v)
            5, 7 -> sx(u)
            8 -> { val (mx, my) = center(cw, ch); rotation = kotlin.math.atan2(y - my, x - mx) + (Math.PI / 2).toFloat() }
        }
    }
    /** The handle near canvas point (x, y) within [radius], 9 inside the box, -1 none. */
    fun handleAt(x: Float, y: Float, radius: Float, cw: Int, ch: Int, rotateGap: Float): Int {
        val pts = handlePoints(cw, ch, rotateGap)
        val best = pts.indices.minByOrNull { (pts[it].first - x).let { d -> d * d } + (pts[it].second - y).let { d -> d * d } } ?: return -1
        val d = kotlin.math.hypot(pts[best].first - x, pts[best].second - y)
        if (d <= radius) return best
        val (u, v) = canvasToLocal(x, y, cw, ch); val (hw, hh) = halfSize(cw, ch)
        return if (kotlin.math.abs(u) <= hw && kotlin.math.abs(v) <= hh) 9 else -1
    }
    /** A copy placed over the image pixels [x0, y0, w, h] of this one, at the same spot and size on
     *  the canvas (for a cut-out piece of [pw] x [ph] pixels). */
    fun pieceAt(x0: Int, y0: Int, w: Int, h: Int, cw: Int, ch: Int): Rotoscope {
        val r = copy(); r.pixW = w; r.pixH = h
        val k = fit(cw, ch); val k2 = minOf(cw.toFloat() / w, ch.toFloat() / h)
        r.stretchX = stretchX * k / k2; r.stretchY = stretchY * k / k2
        val (hw, hh) = halfSize(cw, ch)
        val u = (x0 + w / 2f) / pixW * 2 * hw - hw; val v = (y0 + h / 2f) / pixH * 2 * hh - hh
        val (cx, cy) = localToCanvas(u, v, cw, ch)
        r.dx = cx - cw * 0.5f; r.dy = cy - ch * 0.5f
        return r
    }

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
    /** The photo's file after an edit (placement and timing kept). */
    fun replaceFile(uri: String) { if (kind == Kind.IMAGE) uris = listOf(uri) }
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
        .put("onFrames", JSONArray(onFrames.toList())).put("inExport", inExport)

    fun fromJson(o: JSONObject?) {
        if (o == null) { clear(); return }
        val list = o.optJSONArray("uris")?.let { a -> (0 until a.length()).map { a.getString(it) } } ?: emptyList()
        set(runCatching { Kind.valueOf(o.optString("kind")) }.getOrDefault(Kind.IMAGE), list, o.optInt("frames", list.size))
        visible = o.optBoolean("visible", true); inExport = o.optBoolean("inExport", false); layer = o.optString("layer", ""); setOpacity(o.optDouble("opacity", 0.5).toFloat())
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
