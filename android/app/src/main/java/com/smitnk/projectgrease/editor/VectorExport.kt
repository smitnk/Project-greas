package com.smitnk.projectgrease.editor

import java.io.ByteArrayOutputStream
import java.util.Locale
import kotlin.math.max

/**
 * SVG and PDF export of the document (Blender's io/gpencil_legacy export behaves the same way: each
 * visible layer becomes a group, each stroke a path with the material color mixed with the vertex
 * color, width from thickness and pressure, opacity from strength).
 *
 * Both formats are written from one geometry model ([VectorPage]) so they cannot disagree. Colors
 * follow the on-screen presenter (project_grease_gp_color.h): the stroke color is the mean over the
 * points of mix(material, vertex color, vertex alpha); a vertex color's alpha is never multiplied
 * into the opacity. Adaptations, all deliberate:
 *  - stroke width is thickness x the mean pressure (one width per stroke; a polyline cannot taper);
 *  - a stroke has one color (no per-point gradient), as in the presenter;
 *  - fills are drawn under their stroke (Blender's order) as closed polygons of the stroke points
 *    when the material has Fill enabled; the polygon is the stroke outline, not Blender's
 *    triangulation (self-overlapping outlines fill with the non-zero rule);
 *  - the active frame of a layer is its last keyframe at or before the exported frame (hold);
 *  - live modifiers, masks and shader effects are not applied (the saved strokes are exported);
 *  - the PDF is written by a small writer instead of android.graphics.pdf.PdfDocument so the output
 *    can be unit-tested on the JVM (page count, structure) without an Android runtime.
 */
class VectorShape(
    /** Canvas coordinates; one point is a dot. */
    val points: List<FloatArray>,
    val closed: Boolean,
    /** r, g, b, a in 0..1 or null. */
    val fill: FloatArray?,
    val stroke: FloatArray?,
    val strokeWidth: Float
)

class VectorLayer(val name: String, val shapes: List<VectorShape>)

class VectorPage(val frame: Int, val width: Int, val height: Int, val layers: List<VectorLayer>)

object VectorExport {
    private val defaultStroke = floatArrayOf(0.05f, 0.05f, 0.05f, 1f)
    private val defaultFill = floatArrayOf(1f, 1f, 1f, 1f)

    private fun clamp01(v: Float) = if (v > 0f) (if (v > 1f) 1f else v) else 0f

    /** mix(base, vert.rgb, vert.a) per point, averaged: the presenter's one color per stroke. */
    private fun meanStrokeRgb(base: FloatArray, points: List<FloatArray>): FloatArray {
        if (points.isEmpty()) return floatArrayOf(base[0], base[1], base[2])
        val sum = FloatArray(3)
        for (p in points) {
            val f = clamp01(clamp01(p.getOrElse(9) { 0f }))
            for (c in 0 until 3) sum[c] += base[c] * (1f - f) + clamp01(p.getOrElse(6 + c) { 0f }) * f
        }
        return FloatArray(3) { sum[it] / points.size }
    }

    private fun mixFill(base: FloatArray, vert: FloatArray): FloatArray {
        val f = clamp01(vert.getOrElse(3) { 0f })
        return FloatArray(3) { base[it] * (1f - f) + clamp01(vert.getOrElse(it) { 0f }) * f }
    }

    /** The shapes of one stroke (fill first, then the stroke), or empty when the material is hidden. */
    fun shapesOf(stroke: StrokeRecord, material: MaterialRecord?, layerOpacity: Float): List<VectorShape> {
        if (stroke.points.isEmpty() || material?.visible == false) return emptyList()
        val strokeBase = material?.stroke ?: defaultStroke
        val fillBase = material?.fill ?: defaultFill
        val avgStrength = stroke.points.map { clamp01(it.getOrElse(4) { 1f }) }.average().toFloat()
        val alphaScale = clamp01(layerOpacity) * avgStrength
        val avgPressure = stroke.points.map { max(it.getOrElse(3) { 1f }, 0.01f) }.average().toFloat()
        val xy = stroke.points.map { floatArrayOf(it[0], it[1]) }
        val out = ArrayList<VectorShape>(2)
        if (material?.fillEnabled == true && xy.size >= 3) {
            val rgb = mixFill(fillBase, stroke.fillColor)
            out.add(VectorShape(xy, true, floatArrayOf(rgb[0], rgb[1], rgb[2], clamp01(fillBase[3] * alphaScale)), null, 0f))
        }
        val rgb = meanStrokeRgb(strokeBase, stroke.points)
        out.add(
            VectorShape(
                xy, stroke.cyclic && xy.size >= 3, null,
                floatArrayOf(rgb[0], rgb[1], rgb[2], clamp01(strokeBase[3] * alphaScale)),
                max(stroke.thickness * avgPressure, 0.1f)
            )
        )
        return out
    }

    /**
     * Reads the pages for [frames] from the document. Moves the native layer/frame selection; the
     * caller restores it (see EditorController.exportPages).
     */
    fun pages(native: DocumentNative, width: Int, height: Int, frames: List<Int>): List<VectorPage> {
        val materials = (0 until native.materialCount()).map { native.materialRecord(it) }
        val layerCount = native.layerCount()
        // Per layer: keyframe number -> strokes, read once (a frame range reuses them).
        val cache = HashMap<Pair<Int, Int>, List<StrokeRecord>>()
        fun strokesOf(layer: Int, key: Int): List<StrokeRecord> = cache.getOrPut(layer to key) {
            if (!native.selectLayer(layer) || !native.selectFrame(key)) emptyList()
            else (0 until native.strokeCount()).mapNotNull { native.strokeRecord(it) }
        }
        val layerInfo = (0 until layerCount).map { native.layerRecord(it) }
        val keys = (0 until layerCount).map { layer ->
            if (native.selectLayer(layer)) native.frameNumbers().sorted() else emptyList()
        }
        return frames.map { frame ->
            val layers = ArrayList<VectorLayer>()
            for (layer in 0 until layerCount) {
                val info = layerInfo[layer] ?: continue
                if (!info.visible) continue
                val key = keys[layer].lastOrNull { it <= frame } ?: continue
                val shapes = strokesOf(layer, key).flatMap { stroke ->
                    shapesOf(stroke, materials.getOrNull(stroke.materialIndex), info.opacity)
                }
                layers.add(VectorLayer(info.name.ifBlank { "Layer ${layer + 1}" }, shapes))
            }
            VectorPage(frame, width, height, layers)
        }
    }

    // ---- SVG --------------------------------------------------------------------------------

    private fun num(v: Float): String {
        val s = String.format(Locale.ROOT, "%.3f", if (v.isNaN() || v.isInfinite()) 0f else v)
        return s.trimEnd('0').trimEnd('.').ifEmpty { "0" }.let { if (it == "-0") "0" else it }
    }

    private fun hex(c: FloatArray): String =
        String.format(Locale.ROOT, "#%02x%02x%02x", (clamp01(c[0]) * 255f + 0.5f).toInt(), (clamp01(c[1]) * 255f + 0.5f).toInt(), (clamp01(c[2]) * 255f + 0.5f).toInt())

    private fun xml(s: String) = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;")

    /** One SVG document for one frame: a `<g>` per visible layer, canvas size as the viewBox. */
    fun toSvg(page: VectorPage): String {
        val sb = StringBuilder()
        sb.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
        sb.append("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"${page.width}\" height=\"${page.height}\" viewBox=\"0 0 ${page.width} ${page.height}\">\n")
        page.layers.forEachIndexed { index, layer ->
            sb.append("<g id=\"layer-${index + 1}\" data-name=\"${xml(layer.name)}\">\n")
            for (shape in layer.shapes) sb.append(svgShape(shape)).append('\n')
            sb.append("</g>\n")
        }
        sb.append("</svg>\n")
        return sb.toString()
    }

    private fun svgShape(shape: VectorShape): String {
        val first = shape.points.first()
        if (shape.points.size == 1) {
            val c = shape.stroke ?: return ""
            return "<circle cx=\"${num(first[0])}\" cy=\"${num(first[1])}\" r=\"${num(shape.strokeWidth / 2f)}\" fill=\"${hex(c)}\" fill-opacity=\"${num(c[3])}\"/>"
        }
        val pts = shape.points.joinToString(" ") { "${num(it[0])},${num(it[1])}" }
        return if (shape.fill != null) {
            val d = "M " + shape.points.joinToString(" L ") { "${num(it[0])} ${num(it[1])}" } + " Z"
            "<path d=\"$d\" fill=\"${hex(shape.fill)}\" fill-opacity=\"${num(shape.fill[3])}\" stroke=\"none\"/>"
        } else {
            val c = shape.stroke!!
            val tag = if (shape.closed) "polygon" else "polyline"
            "<$tag points=\"$pts\" fill=\"none\" stroke=\"${hex(c)}\" stroke-opacity=\"${num(c[3])}\" stroke-width=\"${num(shape.strokeWidth)}\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>"
        }
    }

    // ---- PDF --------------------------------------------------------------------------------

    /** A PDF with one page per [VectorPage], each page the size of the canvas (1 px = 1 pt). */
    fun toPdf(pages: List<VectorPage>): ByteArray {
        require(pages.isNotEmpty()) { "a PDF needs at least one page" }
        val out = ByteArrayOutputStream()
        val offsets = ArrayList<Int>()
        fun write(s: String) = out.write(s.toByteArray(Charsets.ISO_8859_1))
        fun obj(id: Int, body: String) {
            while (offsets.size < id) offsets.add(0)
            offsets[id - 1] = out.size()
            write("$id 0 obj\n$body\nendobj\n")
        }
        write("%PDF-1.4\n%âãÏÓ\n")
        // objects: 1 catalog, 2 pages, then per page: page, contents
        val kids = pages.indices.joinToString(" ") { "${3 + it * 2} 0 R" }
        obj(1, "<< /Type /Catalog /Pages 2 0 R >>")
        obj(2, "<< /Type /Pages /Kids [$kids] /Count ${pages.size} >>")
        pages.forEachIndexed { i, page ->
            val pageId = 3 + i * 2
            val contentId = pageId + 1
            val alphas = LinkedHashMap<String, Int>() // "stroke,fill" alphas -> /GSn
            val content = StringBuilder()
            content.append("1 0 0 -1 0 ${page.height} cm\n1 J 1 j\n") // y down like the canvas, round caps/joins
            fun gs(strokeAlpha: Float, fillAlpha: Float): String {
                val key = "${num(strokeAlpha)} ${num(fillAlpha)}"
                return "/GS${alphas.getOrPut(key) { alphas.size }} gs\n"
            }
            for (layer in page.layers) for (shape in layer.shapes) {
                val a = shape.points
                if (a.size == 1) {
                    val c = shape.stroke ?: continue
                    content.append(gs(c[3], c[3]))
                    content.append("${num(c[0])} ${num(c[1])} ${num(c[2])} rg\n")
                    val r = shape.strokeWidth / 2f
                    // a dot as a circle made of four Bezier arcs
                    val k = 0.5522847f * r
                    val x = a[0][0]; val y = a[0][1]
                    content.append("${num(x + r)} ${num(y)} m ${num(x + r)} ${num(y + k)} ${num(x + k)} ${num(y + r)} ${num(x)} ${num(y + r)} c ")
                    content.append("${num(x - k)} ${num(y + r)} ${num(x - r)} ${num(y + k)} ${num(x - r)} ${num(y)} c ")
                    content.append("${num(x - r)} ${num(y - k)} ${num(x - k)} ${num(y - r)} ${num(x)} ${num(y - r)} c ")
                    content.append("${num(x + k)} ${num(y - r)} ${num(x + r)} ${num(y - k)} ${num(x + r)} ${num(y)} c f\n")
                    continue
                }
                val path = StringBuilder()
                a.forEachIndexed { index, p -> path.append("${num(p[0])} ${num(p[1])} ${if (index == 0) "m" else "l"} ") }
                if (shape.fill != null) {
                    val c = shape.fill
                    content.append(gs(c[3], c[3]))
                    content.append("${num(c[0])} ${num(c[1])} ${num(c[2])} rg\n$path h f\n")
                } else {
                    val c = shape.stroke ?: continue
                    content.append(gs(c[3], c[3]))
                    content.append("${num(c[0])} ${num(c[1])} ${num(c[2])} RG\n${num(shape.strokeWidth)} w\n$path${if (shape.closed) "h " else ""}S\n")
                }
            }
            val gsDict = alphas.entries.joinToString(" ") { (key, index) ->
                val (s, f) = key.split(" ")
                "/GS$index << /Type /ExtGState /CA $s /ca $f >>"
            }
            val resources = if (alphas.isEmpty()) "<< >>" else "<< /ExtGState << $gsDict >> >>"
            obj(pageId, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${page.width} ${page.height}] /Resources $resources /Contents $contentId 0 R >>")
            val bytes = content.toString()
            obj(contentId, "<< /Length ${bytes.length} >>\nstream\n$bytes\nendstream")
        }
        val xref = out.size()
        val total = offsets.size + 1
        write("xref\n0 $total\n0000000000 65535 f \n")
        for (o in offsets) write(String.format(Locale.ROOT, "%010d 00000 n \n", o))
        write("trailer\n<< /Size $total /Root 1 0 R >>\nstartxref\n$xref\n%%EOF\n")
        return out.toByteArray()
    }
}
