package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** Blender exporter parity (gpencil_io_export_svg.cc / _pdf.cc) and the gradient-fill extension. */
class VectorExportParityTest {
    private fun pt(x: Float, y: Float, strength: Float = 1f, vc: FloatArray = FloatArray(4)) =
        floatArrayOf(x, y, 0f, 1f, strength, 0f, vc[0], vc[1], vc[2], vc[3])

    private val square = listOf(pt(0f, 0f), pt(10f, 0f), pt(10f, 10f), pt(0f, 10f))

    private fun twoLayers(): ExportDoc {
        val doc = ExportDoc()
        doc.materials.add(MaterialRecord(floatArrayOf(1f, 0f, 0f, 1f), floatArrayOf(1f, 1f, 1f, 1f), true, false))
        doc.layers.add(ExportDoc.Layer(LayerRecord("A", true, false, 1f), listOf(ExportDoc.Frame(1, listOf(StrokeRecord(listOf(pt(0f, 0f), pt(5f, 5f)), 0, 2f))))))
        doc.layers.add(ExportDoc.Layer(LayerRecord("B", true, false, 1f), listOf(ExportDoc.Frame(1, listOf(StrokeRecord(listOf(pt(1f, 1f), pt(9f, 9f)), 0, 2f))))))
        return doc
    }

    @Test
    fun opacityFollowsBlenderStrokeAndFillFormulas() {
        val mat = MaterialRecord(floatArrayOf(0f, 0f, 1f, 0.8f), floatArrayOf(0f, 1f, 0f, 0.6f), true, true)
        val stroke = StrokeRecord(square.map { pt(it[0], it[1], 0.5f) }, 0, 4f, cyclic = true)
        val shapes = VectorExport.shapesOf(stroke, mat, 0.5f)
        assertEquals(2, shapes.size)
        // fill_color_[3] * gpl->opacity: strength does not apply
        assertEquals(0.3f, shapes[0].fill!![3], 1e-5f)
        // stroke_color_[3] * avg strength * gpl->opacity
        assertEquals(0.2f, shapes[1].stroke!![3], 1e-5f)
        val svg = VectorExport.toSvg(VectorPage(1, 20, 20, listOf(VectorLayer("L", shapes))))
        assertTrue(svg, svg.contains("fill-opacity=\"0.3\""))
        assertTrue(svg, svg.contains("stroke-opacity=\"0.2\""))
    }

    @Test
    fun vertexAlphaInterpolatesTheStrokeAlphaAndTinyAlphaIsSkipped() {
        val mat = MaterialRecord(floatArrayOf(1f, 0f, 0f, 0.5f), floatArrayOf(1f, 1f, 1f, 1f), true, false)
        val vc = floatArrayOf(0f, 0f, 1f, 1f)
        val s = VectorExport.shapesOf(StrokeRecord(listOf(pt(0f, 0f, vc = vc), pt(5f, 0f, vc = vc)), 0, 2f), mat, 1f)
        assertEquals(1f, s.single().stroke!![3], 1e-5f) // interpolate(0.5, 1, 1) = 1
        // GPENCIL_ALPHA_OPACITY_THRESH: a zero-opacity layer writes nothing
        assertTrue(VectorExport.shapesOf(StrokeRecord(listOf(pt(0f, 0f), pt(5f, 0f)), 0, 2f), mat, 0f).isEmpty())
    }

    @Test
    fun layerTintAndLineChangeApply() {
        val mat = MaterialRecord(floatArrayOf(1f, 0f, 0f, 1f), floatArrayOf(1f, 1f, 1f, 1f), true, false)
        val s = VectorExport.shapesOf(StrokeRecord(listOf(pt(0f, 0f), pt(5f, 0f)), 0, 2f), mat, 1f, floatArrayOf(0f, 0f, 1f, 0.5f), 3).single()
        assertEquals(0.5f, s.stroke!![0], 1e-5f); assertEquals(0.5f, s.stroke!![2], 1e-5f)
        assertEquals(5f, s.strokeWidth, 1e-5f)
    }

    @Test
    fun frameRangeGivesOnePdfPagePerFrame() {
        val frames = VectorExport.frameRange(3, 7)
        assertEquals(listOf(3, 4, 5, 6, 7), frames)
        assertTrue(VectorExport.frameRange(5, 4).isEmpty())
        val pdf = String(VectorExport.toPdf(VectorExport.pages(twoLayers(), 20, 20, frames)), Charsets.ISO_8859_1)
        assertEquals(5, Regex("/Type /Page\\b(?!s)").findAll(pdf).count())
        assertTrue(pdf.contains("/Count 5"))
    }

    @Test
    fun selectedLayersOnly() {
        val page = VectorExport.pages(twoLayers(), 20, 20, listOf(1), VectorExportOptions(layers = setOf(1))).single()
        assertEquals(listOf("B"), page.layers.map { it.name })
        assertEquals(2, VectorExport.pages(twoLayers(), 20, 20, listOf(1)).single().layers.size)
    }

    @Test
    fun clipToCanvasWritesBlendersClipPathAndAPdfClip() {
        val clipped = VectorExport.pages(twoLayers(), 20, 30, listOf(4), VectorExportOptions(clipToCanvas = true)).single()
        val svg = VectorExport.toSvg(clipped)
        assertTrue(svg, svg.contains("<clipPath id=\"clip-path4\"><rect x=\"0\" y=\"0\" width=\"20\" height=\"30\"/></clipPath>"))
        assertTrue(svg, svg.contains("<g id=\"blender_frame_4\" clip-path=\"url(#clip-path4)\">"))
        assertTrue(String(VectorExport.toPdf(listOf(clipped)), Charsets.ISO_8859_1).contains("0 0 20 30 re W n"))
        val plain = VectorExport.pages(twoLayers(), 20, 30, listOf(4)).single()
        assertFalse(VectorExport.toSvg(plain).contains("clipPath"))
        assertFalse(String(VectorExport.toPdf(listOf(plain)), Charsets.ISO_8859_1).contains("re W n"))
    }

    private fun gradientMaterial(type: Int, flip: Boolean = false) = MaterialRecord(
        floatArrayOf(0f, 0f, 0f, 1f), floatArrayOf(1f, 0f, 0f, 1f), true, true,
        gradient = floatArrayOf(type.toFloat(), 0f, 0f, 1f, 1f, 0f, 0f, 1f, 1f, 0f, 0f, if (flip) 1f else 0f)
    )

    @Test
    fun linearGradientFillMatchesThePresenterUvMapping() {
        val shapes = VectorExport.shapesOf(StrokeRecord(square, 0, 1f, cyclic = true), gradientMaterial(0), 1f)
        val g = shapes[0].gradient
        assertNotNull(g)
        val svg = VectorExport.toSvg(VectorPage(1, 20, 20, listOf(VectorLayer("L", shapes))))
        // bounding square 10 centred at (5, 5): u = x / 10, so uv -> canvas is matrix(10 0 0 10 0 0)
        assertTrue(svg, svg.contains("<linearGradient id=\"gradient-1-1\" gradientUnits=\"userSpaceOnUse\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\" gradientTransform=\"matrix(10 0 0 10 0 0)\">"))
        assertTrue(svg, svg.contains("<stop offset=\"0\" stop-color=\"#ff0000\" stop-opacity=\"1\"/><stop offset=\"1\" stop-color=\"#0000ff\" stop-opacity=\"1\"/>"))
        assertTrue(svg, svg.contains("fill=\"url(#gradient-1-1)\""))
        val pdf = String(VectorExport.toPdf(listOf(VectorPage(1, 20, 20, listOf(VectorLayer("L", shapes))))), Charsets.ISO_8859_1)
        assertTrue(pdf, pdf.contains("/ShadingType 2"))
        assertTrue(pdf, pdf.contains("/Coords [0 0 1 0]"))
        assertTrue(pdf, pdf.contains("/Sh0 sh"))
        assertTrue(pdf, pdf.contains("/Shading << /Sh0"))
    }

    @Test
    fun radialAndFlippedGradients() {
        val shapes = VectorExport.shapesOf(StrokeRecord(square, 0, 1f, cyclic = true), gradientMaterial(1, flip = true), 1f)
        val svg = VectorExport.toSvg(VectorPage(1, 20, 20, listOf(VectorLayer("L", shapes))))
        assertTrue(svg, svg.contains("<radialGradient id=\"gradient-1-1\" gradientUnits=\"userSpaceOnUse\" cx=\"0.5\" cy=\"0.5\" r=\"0.5\""))
        assertTrue(svg, svg.contains("<stop offset=\"0\" stop-color=\"#0000ff\""))
        val pdf = String(VectorExport.toPdf(listOf(VectorPage(1, 20, 20, listOf(VectorLayer("L", shapes))))), Charsets.ISO_8859_1)
        assertTrue(pdf, pdf.contains("/ShadingType 3"))
        // a zero scale has no inverse: flat fill
        val zero = MaterialRecord(floatArrayOf(0f, 0f, 0f, 1f), floatArrayOf(1f, 0f, 0f, 1f), true, true,
            gradient = floatArrayOf(0f, 0f, 0f, 1f, 1f, 0f, 0f, 0f, 1f, 0f, 0f, 0f))
        assertNull(VectorExport.shapesOf(StrokeRecord(square, 0, 1f), zero, 1f)[0].gradient)
    }
}
