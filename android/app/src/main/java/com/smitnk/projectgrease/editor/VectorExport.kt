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
 *
 * Blender parity (io/gpencil_legacy, gpencil_io_export_svg.cc / _pdf.cc / gpencil_io_base.cc):
 *  - opacity: prepare_stroke_export_colors + color_string_set / color_set. stroke-opacity =
 *    mix(material stroke rgba, mean vertex color, mean vertex alpha).a x mean strength x layer
 *    opacity; fill-opacity = mix(material fill rgba, vert_color_fill, its alpha).a x layer opacity
 *    (strength does not touch the fill). The rgb is mixed toward the layer tint by its factor;
 *    shapes at or under GPENCIL_ALPHA_OPACITY_THRESH (0.001) are skipped like the PDF exporter does;
 *  - thickness gets the layer's line_change (thickness change), clamped to at least 1;
 *  - frame range: one SVG / one PDF page per frame of frame_start..frame_end (Blender's PDF pages);
 *  - "selected objects only": here selected layers only ([VectorExportOptions.layers]);
 *  - use_clip_camera: clip to the canvas rect (SVG clipPath "clip-path<frame>" on a
 *    "blender_frame_<frame>" group, PDF "re W n");
 *  - kept as in the presenter instead of Blender: the stroke rgb is the mean of the per-point
 *    mixes, single points export as dots (Blender skips strokes under 2 points), no linear->sRGB
 *    conversion (the app stores display colors).
 * Extension beyond Blender: gradient fills (material fill_style GRADIENT) are written as SVG
 * linearGradient / radialGradient and PDF axial / radial shadings that reproduce the presenter's
 * gradient render (android_gp_presentation.cpp append_gradient_fill + grad_fs_src); Blender's
 * exporters write the flat fill color. A PDF shading has one alpha: stops with different alphas use
 * their mean.
 */
class VectorShape(
    /** Canvas coordinates; one point is a dot. */
    val points: List<FloatArray>,
    val closed: Boolean,
    /** r, g, b, a in 0..1 or null. */
    val fill: FloatArray?,
    val stroke: FloatArray?,
    val strokeWidth: Float,
    /** Gradient fill (extension beyond Blender); [fill] stays the flat fallback color. */
    val gradient: VectorGradient? = null
)

/**
 * A two-stop fill gradient in the presenter's uv space: linear runs along u from 0 to 1, radial is
 * centred at (0.5, 0.5) with radius 0.5 (gpencil_frag.glsl: radial ? length(uv * 2 - 1) : uv.x),
 * padded outside. [toCanvas] = [a, b, c, d, e, f] maps uv to canvas (SVG matrix order).
 */
class VectorGradient(val radial: Boolean, val stop0: FloatArray, val stop1: FloatArray, val toCanvas: FloatArray)

/**
 * Export options. [layers]: layer indices to export (Blender's "selected objects only" maps to
 * selected layers here), null = every visible layer. [clipToCanvas]: Blender's use_clip_camera.
 */
class VectorExportOptions(val layers: Set<Int>? = null, val clipToCanvas: Boolean = false)

class VectorLayer(val name: String, val shapes: List<VectorShape>)

class VectorPage(val frame: Int, val width: Int, val height: Int, val layers: List<VectorLayer>, val clip: Boolean = false)

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

    /** GPENCIL_ALPHA_OPACITY_THRESH (BKE_gpencil_legacy.h). */
    const val ALPHA_THRESH = 0.001f

    private fun tinted(rgb: FloatArray, tint: FloatArray?): FloatArray {
        val f = clamp01(tint?.getOrElse(3) { 0f } ?: 0f)
        if (tint == null || f <= 0f) return rgb
        return FloatArray(3) { rgb[it] + (clamp01(tint[it]) - rgb[it]) * f }
    }

    /** The frames frame_start..frame_end (one SVG / PDF page each), empty when end < start. */
    fun frameRange(start: Int, end: Int): List<Int> = if (end < start) emptyList() else (start..end).toList()

    /**
     * The shapes of one stroke (fill first, then the stroke), or empty when the material is hidden.
     * Opacity follows Blender's exporters (see the class notes); [tint] is the layer tint rgb +
     * factor, [lineChange] the layer thickness change.
     */
    fun shapesOf(
        stroke: StrokeRecord, material: MaterialRecord?, layerOpacity: Float,
        tint: FloatArray? = null, lineChange: Int = 0
    ): List<VectorShape> {
        if (stroke.points.isEmpty() || material?.visible == false) return emptyList()
        val strokeBase = material?.stroke ?: defaultStroke
        val fillBase = material?.fill ?: defaultFill
        val layerAlpha = clamp01(layerOpacity)
        val avgStrength = stroke.points.map { clamp01(it.getOrElse(4) { 1f }) }.average().toFloat()
        val avgPressure = stroke.points.map { max(it.getOrElse(3) { 1f }, 0.01f) }.average().toFloat()
        val xy = stroke.points.map { floatArrayOf(it[0], it[1]) }
        val out = ArrayList<VectorShape>(2)
        if (material?.fillEnabled == true && xy.size >= 3) {
            // fill_color_ = interpolate(fill_rgba, vert_color_fill, vert_color_fill.a); opacity x layer
            val vcfA = clamp01(stroke.fillColor.getOrElse(3) { 0f })
            val fillAlpha = clamp01((fillBase[3] * (1f - vcfA) + vcfA * vcfA) * layerAlpha)
            val rgb = tinted(mixFill(fillBase, stroke.fillColor), tint)
            val gradient = material.gradient?.let { gradientOf(it, fillBase, stroke.fillColor, tint, layerAlpha * avgStrength, xy) }
            if (fillAlpha > ALPHA_THRESH || gradient != null) {
                out.add(VectorShape(xy, true, floatArrayOf(rgb[0], rgb[1], rgb[2], fillAlpha), null, 0f, gradient))
            }
        }
        // stroke_color_ = interpolate(stroke_rgba, mean vert_color, its alpha); opacity x strength x layer
        val vcA = stroke.points.map { clamp01(it.getOrElse(9) { 0f }) }.average().toFloat()
        val strokeAlpha = clamp01((strokeBase[3] * (1f - vcA) + vcA * vcA) * avgStrength * layerAlpha)
        if (strokeAlpha > ALPHA_THRESH) {
            val rgb = tinted(meanStrokeRgb(strokeBase, stroke.points), tint)
            val thickness = max(stroke.thickness + lineChange, 1f)
            out.add(
                VectorShape(
                    xy, stroke.cyclic && xy.size >= 3, null,
                    floatArrayOf(rgb[0], rgb[1], rgb[2], strokeAlpha),
                    max(thickness * avgPressure, 0.1f)
                )
            )
        }
        return out
    }

    /**
     * The presenter's gradient fill as a two-stop gradient. [g] = MaterialRecord.gradient
     * ([type, mix r, g, b, a, mix factor, angle, scale x, y, offset x, y, flip]). Colors: c1 = fill,
     * c2 = mix color (swapped by flip); the shader mixes mix(c1, c2, f) with the vertex-colored c1 by
     * 1 - mix factor, alpha = c.a x layer opacity x mean strength. UV: append_gradient_fill (centred
     * bounding square) then gradient_uv_transform (gpencil_uv_transform_get). Null when the uv
     * transform is singular (a zero scale).
     */
    fun gradientOf(
        g: FloatArray, fillBase: FloatArray, vertexFill: FloatArray, tint: FloatArray?, alphaScale: Float,
        xy: List<FloatArray>
    ): VectorGradient? {
        if (g.size < 12 || xy.isEmpty()) return null
        var c1 = floatArrayOf(fillBase[0], fillBase[1], fillBase[2], fillBase[3])
        var c2 = floatArrayOf(g[1], g[2], g[3], g[4])
        if (g[11] != 0f) { val t = c1; c1 = c2; c2 = t }
        val base = tinted(mixFill(c1, vertexFill), tint)
        val m = clamp01(1f - g[5])
        fun stop(c: FloatArray) = floatArrayOf(
            clamp01(c[0] * m + base[0] * (1f - m)), clamp01(c[1] * m + base[1] * (1f - m)),
            clamp01(c[2] * m + base[2] * (1f - m)), clamp01(c[3] * alphaScale)
        )
        // gradient_uv_transform: columns of S(1 / scale) * R(-angle), offset through it plus 0.5
        val sx = if (g[7] != 0f) 1.0 / g[7] else 0.0
        val sy = if (g[8] != 0f) 1.0 / g[8] else 0.0
        val ca = kotlin.math.cos(-g[6].toDouble()); val sa = kotlin.math.sin(-g[6].toDouble())
        val m00 = sx * ca; val m01 = sy * sa; val m10 = -sx * sa; val m11 = sy * ca
        val off0 = m00 * g[9] + m10 * g[10] + 0.5
        val off1 = m01 * g[9] + m11 * g[10] + 0.5
        var minX = Float.MAX_VALUE; var minY = Float.MAX_VALUE; var maxX = -Float.MAX_VALUE; var maxY = -Float.MAX_VALUE
        for (p in xy) { minX = minOf(minX, p[0]); minY = minOf(minY, p[1]); maxX = maxOf(maxX, p[0]); maxY = maxOf(maxY, p[1]) }
        val size = maxOf(maxX - minX, maxY - minY, 1e-6f).toDouble()
        val cx = 0.5 * (minX + maxX); val cy = 0.5 * (minY + maxY)
        // canvas -> uv: u = (m00 dx + m10 dy) / size + off0, v = (m01 dx + m11 dy) / size + off1
        val a00 = m00 / size; val a01 = m10 / size; val a10 = m01 / size; val a11 = m11 / size
        val det = a00 * a11 - a01 * a10
        if (!det.isFinite() || kotlin.math.abs(det) < 1e-12) return null
        val i00 = a11 / det; val i01 = -a01 / det; val i10 = -a10 / det; val i11 = a00 / det
        val e = cx - i00 * off0 - i01 * off1
        val f = cy - i10 * off0 - i11 * off1
        return VectorGradient(
            g[0].toInt() == 1, stop(c1), stop(c2),
            floatArrayOf(i00.toFloat(), i10.toFloat(), i01.toFloat(), i11.toFloat(), e.toFloat(), f.toFloat())
        )
    }

    /**
     * Reads the pages for [frames] from the document. Moves the native layer/frame selection; the
     * caller restores it (see EditorController.exportPages).
     */
    fun pages(
        native: DocumentNative, width: Int, height: Int, frames: List<Int>,
        options: VectorExportOptions = VectorExportOptions()
    ): List<VectorPage> {
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
                if (options.layers != null && layer !in options.layers) continue
                val key = keys[layer].lastOrNull { it <= frame } ?: continue
                val shapes = strokesOf(layer, key).flatMap { stroke ->
                    shapesOf(stroke, materials.getOrNull(stroke.materialIndex), info.opacity, info.tint, info.lineChange)
                }
                layers.add(VectorLayer(info.name.ifBlank { "Layer ${layer + 1}" }, shapes))
            }
            VectorPage(frame, width, height, layers, options.clipToCanvas)
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
        if (page.clip) {
            // export_gpencil_layers with GP_EXPORT_CLIP_CAMERA: a clipPath of the render rect
            sb.append("<clipPath id=\"clip-path${page.frame}\"><rect x=\"0\" y=\"0\" width=\"${page.width}\" height=\"${page.height}\"/></clipPath>\n")
            sb.append("<g id=\"blender_frame_${page.frame}\" clip-path=\"url(#clip-path${page.frame})\">\n")
        }
        var gradients = 0
        page.layers.forEachIndexed { index, layer ->
            sb.append("<g id=\"layer-${index + 1}\" data-name=\"${xml(layer.name)}\">\n")
            for (shape in layer.shapes) {
                val gradient = shape.gradient
                val id = if (gradient != null && shape.fill != null && shape.points.size > 1) "gradient-${page.frame}-${++gradients}" else null
                if (id != null && gradient != null) sb.append(svgGradient(id, gradient)).append('\n')
                sb.append(svgShape(shape, id)).append('\n')
            }
            sb.append("</g>\n")
        }
        if (page.clip) sb.append("</g>\n")
        sb.append("</svg>\n")
        return sb.toString()
    }

    private fun svgGradient(id: String, g: VectorGradient): String {
        val m = g.toCanvas.joinToString(" ") { num6(it) }
        val tag = if (g.radial) "radialGradient" else "linearGradient"
        val geometry = if (g.radial) "cx=\"0.5\" cy=\"0.5\" r=\"0.5\"" else "x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\""
        fun stop(offset: Int, c: FloatArray) = "<stop offset=\"$offset\" stop-color=\"${hex(c)}\" stop-opacity=\"${num(c[3])}\"/>"
        return "<defs><$tag id=\"$id\" gradientUnits=\"userSpaceOnUse\" $geometry gradientTransform=\"matrix($m)\">" +
            "${stop(0, g.stop0)}${stop(1, g.stop1)}</$tag></defs>"
    }

    private fun num6(v: Float): String {
        val s = String.format(Locale.ROOT, "%.6f", if (v.isNaN() || v.isInfinite()) 0f else v)
        return s.trimEnd('0').trimEnd('.').ifEmpty { "0" }.let { if (it == "-0") "0" else it }
    }

    private fun svgShape(shape: VectorShape, gradientId: String? = null): String {
        val first = shape.points.first()
        if (shape.points.size == 1) {
            val c = shape.stroke ?: return ""
            return "<circle cx=\"${num(first[0])}\" cy=\"${num(first[1])}\" r=\"${num(shape.strokeWidth / 2f)}\" fill=\"${hex(c)}\" fill-opacity=\"${num(c[3])}\"/>"
        }
        val pts = shape.points.joinToString(" ") { "${num(it[0])},${num(it[1])}" }
        return if (shape.fill != null) {
            val d = "M " + shape.points.joinToString(" L ") { "${num(it[0])} ${num(it[1])}" } + " Z"
            if (gradientId != null) "<path d=\"$d\" fill=\"url(#$gradientId)\" stroke=\"none\"/>"
            else "<path d=\"$d\" fill=\"${hex(shape.fill)}\" fill-opacity=\"${num(shape.fill[3])}\" stroke=\"none\"/>"
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
            if (page.clip) content.append("0 0 ${page.width} ${page.height} re W n\n") // use_clip_camera
            val shadings = ArrayList<String>() // /ShN dictionaries (gradient fills, an extension)
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
                val g = shape.gradient
                if (shape.fill != null && g != null) {
                    // axial (type 2) / radial (type 3) shading in uv space, mapped by the uv->canvas matrix
                    val coords = if (g.radial) "0.5 0.5 0 0.5 0.5 0.5" else "0 0 1 0"
                    val fn = "<< /FunctionType 2 /Domain [0 1] /C0 [${num(g.stop0[0])} ${num(g.stop0[1])} ${num(g.stop0[2])}] " +
                        "/C1 [${num(g.stop1[0])} ${num(g.stop1[1])} ${num(g.stop1[2])}] /N 1 >>"
                    shadings.add("<< /ShadingType ${if (g.radial) 3 else 2} /ColorSpace /DeviceRGB /Coords [$coords] /Extend [true true] /Function $fn >>")
                    val alpha = (g.stop0[3] + g.stop1[3]) / 2f
                    val mtx = g.toCanvas.joinToString(" ") { num6(it) }
                    content.append("q\n").append(gs(alpha, alpha)).append("${path}h W n\n$mtx cm\n/Sh${shadings.size - 1} sh\nQ\n")
                } else if (shape.fill != null) {
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
            val shDict = shadings.withIndex().joinToString(" ") { (index, sh) -> "/Sh$index $sh" }
            val resources = buildString {
                append("<<")
                if (alphas.isNotEmpty()) append(" /ExtGState << $gsDict >>")
                if (shadings.isNotEmpty()) append(" /Shading << $shDict >>")
                append(" >>")
            }
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
