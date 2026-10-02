package com.smitnk.projectgrease.editor

import kotlin.math.abs
import kotlin.math.min

/**
 * Maps imported outlines (SvgImport strokes, ImageTrace outlines) onto the document: fits the
 * source rectangle into the canvas keeping the aspect ratio, and assigns each stroke a material
 * slot, reusing an existing slot with the same stroke / fill colors and adding one per new pair.
 * Pure data so it runs in JVM tests; EditorController.importStrokes() applies the plan natively.
 */
object StrokeImport {
    class Source(
        val points: List<FloatArray>,
        val closed: Boolean,
        val strokeArgb: Int,
        val fillArgb: Int?,
        val width: Float
    )

    class NewMaterial(val stroke: FloatArray, val fill: FloatArray, val fillEnabled: Boolean)

    class PlannedStroke(val xy: FloatArray, val material: Int, val thickness: Float, val cyclic: Boolean)

    class Plan(val newMaterials: List<NewMaterial>, val strokes: List<PlannedStroke>)

    /** Uniform scale and offset placing [srcW] x [srcH] at [srcX],[srcY] centered in the canvas. */
    class Fit(val scale: Float, val offsetX: Float, val offsetY: Float, val srcX: Float, val srcY: Float) {
        fun x(v: Float) = (v - srcX) * scale + offsetX
        fun y(v: Float) = (v - srcY) * scale + offsetY
    }

    fun fit(srcX: Float, srcY: Float, srcW: Float, srcH: Float, canvasW: Int, canvasH: Int): Fit {
        val w = if (srcW > 0f && srcW.isFinite()) srcW else 1f
        val h = if (srcH > 0f && srcH.isFinite()) srcH else 1f
        val scale = min(canvasW / w, canvasH / h)
        return Fit(scale, (canvasW - w * scale) * 0.5f, (canvasH - h * scale) * 0.5f, srcX, srcY)
    }

    private val VIEW_BOX = Regex("""viewBox\s*=\s*"([^"]*)"""", RegexOption.IGNORE_CASE)
    private val SVG_TAG = Regex("""<svg\b[^>]*>""", RegexOption.IGNORE_CASE)
    private fun attr(tag: String, name: String) =
        Regex("""\b$name\s*=\s*"\s*([-+]?[0-9]*\.?[0-9]+)""").find(tag)?.groupValues?.get(1)?.toFloatOrNull()

    /**
     * The SVG's user-space rectangle (minX, minY, width, height): its viewBox, else width/height
     * of the svg element, else null (the caller then fits the strokes' bounds).
     */
    fun svgViewBox(svg: String): FloatArray? {
        val tag = SVG_TAG.find(svg)?.value ?: return null
        VIEW_BOX.find(tag)?.groupValues?.get(1)?.let { raw ->
            val v = raw.trim().split(Regex("[\\s,]+")).mapNotNull { it.toFloatOrNull() }
            if (v.size == 4 && v[2] > 0f && v[3] > 0f) return v.toFloatArray()
        }
        val w = attr(tag, "width")
        val h = attr(tag, "height")
        return if (w != null && h != null && w > 0f && h > 0f) floatArrayOf(0f, 0f, w, h) else null
    }

    /** Bounds (minX, minY, width, height) of all points, or null when there are none. */
    fun bounds(sources: List<Source>): FloatArray? {
        var minX = Float.MAX_VALUE; var minY = Float.MAX_VALUE
        var maxX = -Float.MAX_VALUE; var maxY = -Float.MAX_VALUE
        sources.forEach { s -> s.points.forEach { p ->
            minX = min(minX, p[0]); minY = min(minY, p[1]); maxX = maxOf(maxX, p[0]); maxY = maxOf(maxY, p[1])
        } }
        if (minX > maxX) return null
        return floatArrayOf(minX, minY, maxX - minX, maxY - minY)
    }

    fun argbToFloats(argb: Int) = floatArrayOf(
        ((argb ushr 16) and 255) / 255f, ((argb ushr 8) and 255) / 255f, (argb and 255) / 255f, ((argb ushr 24) and 255) / 255f
    )

    private fun same(a: FloatArray, b: FloatArray) = a.size == b.size && a.indices.all { abs(a[it] - b[it]) <= 0.5f / 255f }

    /**
     * Builds the strokes and material slots. [existing] are the document's materials in slot
     * order; new slots get indices after them. Sources with fewer than two points are skipped.
     */
    fun plan(sources: List<Source>, fit: Fit, existing: List<MaterialRecord>): Plan {
        val newMaterials = ArrayList<NewMaterial>()
        fun materialFor(strokeArgb: Int, fillArgb: Int?): Int {
            val stroke = argbToFloats(strokeArgb)
            val fill = fillArgb?.let(::argbToFloats)
            existing.forEachIndexed { i, m ->
                if (same(m.stroke, stroke) && m.fillEnabled == (fill != null) && (fill == null || same(m.fill, fill))) return i
            }
            newMaterials.forEachIndexed { i, m ->
                if (same(m.stroke, stroke) && m.fillEnabled == (fill != null) && (fill == null || same(m.fill, fill))) return existing.size + i
            }
            newMaterials += NewMaterial(stroke, fill ?: stroke, fill != null)
            return existing.size + newMaterials.size - 1
        }
        val strokes = sources.filter { it.points.size >= 2 }.map { s ->
            val xy = FloatArray(s.points.size * 2)
            s.points.forEachIndexed { i, p -> xy[i * 2] = fit.x(p[0]); xy[i * 2 + 1] = fit.y(p[1]) }
            PlannedStroke(xy, materialFor(s.strokeArgb, s.fillArgb), s.width.coerceAtLeast(1f), s.closed)
        }
        return Plan(newMaterials, strokes)
    }

    fun fromSvg(strokes: List<SvgImport.Stroke>): List<Source> =
        strokes.map { Source(it.points, it.closed, it.strokeArgb, it.fillArgb, it.width) }
}
