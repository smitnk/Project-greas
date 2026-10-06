package com.smitnk.projectgrease.editor

import java.io.File
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** A tiny in-memory document for the export tests. */
internal class ExportDoc : DocumentNative {
    class Frame(val number: Int, val strokes: List<StrokeRecord>)
    class Layer(val record: LayerRecord, val frames: List<Frame>)

    val layers = mutableListOf<Layer>()
    val materials = mutableListOf<MaterialRecord>()
    private var layer = 0
    private var frame: Frame? = null
    override fun layerCount() = layers.size
    override fun layerRecord(index: Int) = layers.getOrNull(index)?.record
    override fun selectLayer(index: Int): Boolean { if (index !in layers.indices) return false; layer = index; frame = null; return true }
    override fun frameNumbers() = layers[layer].frames.map { it.number }.toIntArray()
    override fun selectFrame(frame: Int): Boolean { this.frame = layers[layer].frames.firstOrNull { it.number == frame }; return this.frame != null }
    override fun strokeCount() = frame?.strokes?.size ?: 0
    override fun strokeRecord(index: Int) = frame?.strokes?.getOrNull(index)
    override fun materialCount() = materials.size
    override fun materialRecord(index: Int) = materials.getOrNull(index)
    override fun createLayer(name: String) = false
    override fun applyLayerRecord(index: Int, record: LayerRecord) = false
    override fun createFrame(frame: Int) = false
    override fun addStroke(record: StrokeRecord) = false
    override fun createMaterial() = false
    override fun applyMaterialRecord(index: Int, record: MaterialRecord) = false
}

class VectorExportTest {
    private fun pt(x: Float, y: Float, pressure: Float = 0.5f, strength: Float = 1f, vc: FloatArray = FloatArray(4)) =
        floatArrayOf(x, y, 0f, pressure, strength, 0f, vc[0], vc[1], vc[2], vc[3])

    /** Two strokes on "Sketch" (frame 1), one on "Ink" (frame 5, hidden layer "Hidden" skipped). */
    private fun document(): ExportDoc {
        val doc = ExportDoc()
        doc.materials.add(MaterialRecord(floatArrayOf(1f, 0f, 0f, 1f), floatArrayOf(1f, 1f, 1f, 1f), true, false)) // red line
        doc.materials.add(MaterialRecord(floatArrayOf(0f, 0f, 1f, 1f), floatArrayOf(0f, 1f, 0f, 0.5f), true, true)) // blue line, green 50% fill
        doc.materials.add(MaterialRecord(floatArrayOf(0f, 0f, 0f, 1f), floatArrayOf(0f, 0f, 0f, 1f), false, false)) // hidden material
        doc.layers.add(ExportDoc.Layer(LayerRecord("Sketch", true, false, 1f), listOf(ExportDoc.Frame(1, listOf(
            StrokeRecord(listOf(pt(10f, 20f), pt(30f, 40f)), 0, 8f),
            StrokeRecord(listOf(pt(50f, 50f, 1f, 0.5f), pt(70f, 50f, 1f, 0.5f), pt(60f, 70f, 1f, 0.5f)), 1, 6f, cyclic = true),
            StrokeRecord(listOf(pt(1f, 1f), pt(2f, 2f)), 2, 5f) // hidden material: not exported
        )))))
        doc.layers.add(ExportDoc.Layer(LayerRecord("Hidden", false, false, 1f), listOf(ExportDoc.Frame(1, listOf(StrokeRecord(listOf(pt(0f, 0f), pt(9f, 9f)), 0, 4f))))))
        doc.layers.add(ExportDoc.Layer(LayerRecord("Ink & <more>", true, false, 0.5f), listOf(ExportDoc.Frame(5, listOf(
            StrokeRecord(listOf(pt(100f, 100f)), 0, 10f) // a single point: a dot
        )))))
        return doc
    }

    @Test
    fun svgOfAKnownTwoStrokeDocumentMatchesTheExpectedText() {
        val page = VectorExport.pages(document(), 200, 100, listOf(1)).single()
        val expected = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100" viewBox="0 0 200 100">
<g id="layer-1" data-name="Sketch">
<polyline points="10,20 30,40" fill="none" stroke="#ff0000" stroke-opacity="1" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M 50 50 L 70 50 L 60 70 Z" fill="#00ff00" fill-opacity="0.5" stroke="none"/>
<polygon points="50,50 70,50 60,70" fill="none" stroke="#0000ff" stroke-opacity="0.5" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
</g>
</svg>
"""
        assertEquals(expected, VectorExport.toSvg(page))
    }

    @Test
    fun heldFramesLayerOpacityDotsAndEscaping() {
        // frame 7: "Sketch" holds its frame 1, "Ink" shows its frame 5 at half opacity as a dot
        val svg = VectorExport.toSvg(VectorExport.pages(document(), 200, 100, listOf(7)).single())
        assertTrue(svg.contains("<g id=\"layer-1\" data-name=\"Sketch\">"))
        assertTrue(svg.contains("data-name=\"Ink &amp; &lt;more&gt;\""))
        assertTrue(svg.contains("<circle cx=\"100\" cy=\"100\" r=\"2.5\" fill=\"#ff0000\" fill-opacity=\"0.5\"/>")) // 10 x 0.5 pressure / 2
        // frame 3: "Ink" has no keyframe yet, so it is not in the page
        val early = VectorExport.toSvg(VectorExport.pages(document(), 200, 100, listOf(3)).single())
        assertFalse(early.contains("Ink"))
        // the hidden layer never appears
        assertFalse(svg.contains("Hidden"))
    }

    @Test
    fun vertexColorMixesIntoTheStrokeColorLikeThePresenter() {
        val doc = ExportDoc()
        doc.materials.add(MaterialRecord(floatArrayOf(1f, 0f, 0f, 1f), floatArrayOf(1f, 1f, 1f, 1f), true, false))
        // one point blue with alpha 1, one with no vertex color (alpha 0): the mean of (0,0,1) and (1,0,0)
        doc.layers.add(ExportDoc.Layer(LayerRecord("L", true, false, 1f), listOf(ExportDoc.Frame(1, listOf(
            StrokeRecord(listOf(pt(0f, 0f, vc = floatArrayOf(0f, 0f, 1f, 1f)), pt(10f, 0f)), 0, 2f)
        )))))
        val svg = VectorExport.toSvg(VectorExport.pages(doc, 10, 10, listOf(1)).single())
        assertTrue(svg, svg.contains("stroke=\"#800080\"")) // (0.5, 0, 0.5)
    }

    @Test
    fun frameRangeGivesOnePagePerFrameAndThePdfHasThatManyPages() {
        val pages = VectorExport.pages(document(), 200, 100, listOf(1, 2, 5))
        assertEquals(3, pages.size)
        val pdf = VectorExport.toPdf(pages)
        val text = String(pdf, Charsets.ISO_8859_1)
        assertTrue(text.startsWith("%PDF-1.4"))
        assertTrue(text.trimEnd().endsWith("%%EOF"))
        assertEquals(3, Regex("/Type /Page\\b(?!s)").findAll(text).count())
        assertTrue(text.contains("/Count 3"))
        assertTrue(text.contains("/MediaBox [0 0 200 100]"))
        // every cross-reference offset points at its object
        val xrefAt = text.lastIndexOf("startxref").let { text.substring(it + 9).trim().lineSequence().first().toInt() }
        assertTrue(text.substring(xrefAt).startsWith("xref"))
        val entries = Regex("(\\d{10}) 00000 n ").findAll(text.substring(xrefAt)).map { it.groupValues[1].toInt() }.toList()
        assertEquals(2 + 3 * 2, entries.size) // catalog, pages, 3 x (page, contents)
        entries.forEachIndexed { index, offset -> assertTrue("object ${index + 1}", text.startsWith("${index + 1} 0 obj", offset)) }
        // the content uses a transparency state for the half-opaque fill and for strokes
        assertTrue(text.contains("/ExtGState"))
        System.getenv("PG_DUMP_PDF")?.let { File(it).writeBytes(pdf) }
    }

    @Test
    fun anEmptyPageStillExports() {
        val empty = ExportDoc()
        val page = VectorExport.pages(empty, 64, 48, listOf(1)).single()
        assertTrue(page.layers.isEmpty())
        assertTrue(VectorExport.toSvg(page).contains("viewBox=\"0 0 64 48\""))
        assertEquals(1, Regex("/Type /Page\\b(?!s)").findAll(String(VectorExport.toPdf(listOf(page)), Charsets.ISO_8859_1)).count())
    }
}
