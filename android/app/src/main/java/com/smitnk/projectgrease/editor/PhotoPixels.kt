package com.smitnk.projectgrease.editor

/**
 * Pixel edits on an imported photo (ARGB, row-major, top row first): the eraser clears an ellipse
 * (a round brush seen through the photo's squash / stretch), the lasso cuts a polygon out.
 */
object PhotoPixels {
    /** Clears the pixels inside the ellipse at (cx, cy) with radii (rx, ry), softened over the outer
     *  [soft] fraction (0 = hard). Returns whether anything changed. */
    fun eraseEllipse(px: IntArray, w: Int, h: Int, cx: Float, cy: Float, rx: Float, ry: Float, soft: Float = 0.25f): Boolean {
        if (rx <= 0f || ry <= 0f || !cx.isFinite() || !cy.isFinite()) return false
        val x0 = (cx - rx).toInt().coerceAtLeast(0); val x1 = kotlin.math.ceil(cx + rx).toInt().coerceAtMost(w - 1)
        val y0 = (cy - ry).toInt().coerceAtLeast(0); val y1 = kotlin.math.ceil(cy + ry).toInt().coerceAtMost(h - 1)
        var changed = false
        for (y in y0..y1) for (x in x0..x1) {
            val dx = (x + 0.5f - cx) / rx; val dy = (y + 0.5f - cy) / ry
            val d = kotlin.math.sqrt(dx * dx + dy * dy)
            if (d >= 1f) continue
            val keep = if (soft <= 0f || d < 1f - soft) 0f else (d - (1f - soft)) / soft
            val i = y * w + x; val a = (px[i] ushr 24) and 0xFF
            val na = (a * keep).toInt()
            if (na != a) { px[i] = (na shl 24) or (px[i] and 0xFFFFFF); changed = true }
        }
        return changed
    }

    /** Even-odd point-in-polygon ([poly] = x0, y0, x1, y1, ...). */
    fun inside(poly: FloatArray, x: Float, y: Float): Boolean {
        var c = false; val n = poly.size / 2; var j = n - 1
        for (i in 0 until n) {
            val xi = poly[2 * i]; val yi = poly[2 * i + 1]; val xj = poly[2 * j]; val yj = poly[2 * j + 1]
            if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) c = !c
            j = i
        }
        return c
    }

    /** A cut-out: pixels [px] of [w] x [h] with their top-left at ([x], [y]) in the source. */
    class Piece(val px: IntArray, val w: Int, val h: Int, val x: Int, val y: Int)

    /** The pixels inside [poly] (image pixel coordinates) as their own image; with [clear] they are
     *  removed from the source. Null when the lasso covers no pixel. */
    fun cut(src: IntArray, w: Int, h: Int, poly: FloatArray, clear: Boolean): Piece? {
        if (poly.size < 6) return null
        var minX = Float.MAX_VALUE; var minY = Float.MAX_VALUE; var maxX = -Float.MAX_VALUE; var maxY = -Float.MAX_VALUE
        for (i in 0 until poly.size / 2) { minX = minOf(minX, poly[2 * i]); maxX = maxOf(maxX, poly[2 * i]); minY = minOf(minY, poly[2 * i + 1]); maxY = maxOf(maxY, poly[2 * i + 1]) }
        val x0 = minX.toInt().coerceIn(0, w); val x1 = kotlin.math.ceil(maxX).toInt().coerceIn(0, w)
        val y0 = minY.toInt().coerceIn(0, h); val y1 = kotlin.math.ceil(maxY).toInt().coerceIn(0, h)
        val pw = x1 - x0; val ph = y1 - y0
        if (pw <= 0 || ph <= 0) return null
        val out = IntArray(pw * ph)
        var any = false
        for (y in y0 until y1) for (x in x0 until x1) {
            if (!inside(poly, x + 0.5f, y + 0.5f)) continue
            val i = y * w + x
            out[(y - y0) * pw + (x - x0)] = src[i]
            if ((src[i] ushr 24) != 0) any = true
            if (clear) src[i] = 0
        }
        return if (any) Piece(out, pw, ph, x0, y0) else null
    }
}
