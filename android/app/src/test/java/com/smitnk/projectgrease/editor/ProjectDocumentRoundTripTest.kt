package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** In-memory stand-in for the native document, with the same starting state (layer 0, material 0). */
private class FakeDocument : DocumentNative {
    class Frame(val number: Int, val strokes: MutableList<StrokeRecord> = mutableListOf())
    class Layer(var record: LayerRecord, val frames: MutableList<Frame> = mutableListOf())

    val layers = mutableListOf(Layer(LayerRecord("GP_Layer", true, false, 1f)))
    val materials = mutableListOf(MaterialRecord(floatArrayOf(0f, 0f, 0f, 1f), floatArrayOf(0f, 0f, 0f, 1f), true, false))
    private var layer = 0
    private var frame: Frame? = null
    /** Mirrors the editing rules: a locked layer refuses new strokes. */
    var lockedLayersRejectStrokes = false
    var failCreateLayer = false

    override fun layerCount() = layers.size
    override fun layerRecord(index: Int) = layers.getOrNull(index)?.record
    override fun selectLayer(index: Int): Boolean {
        if (index !in layers.indices) return false
        layer = index
        frame = null
        return true
    }
    override fun frameNumbers() = layers[layer].frames.map { it.number }.toIntArray()
    override fun selectFrame(frame: Int): Boolean {
        this.frame = layers[layer].frames.firstOrNull { it.number == frame }
        return this.frame != null
    }
    override fun strokeCount() = frame?.strokes?.size ?: 0
    override fun strokeRecord(index: Int) = frame?.strokes?.getOrNull(index)
    override fun materialCount() = materials.size
    override fun materialRecord(index: Int) = materials.getOrNull(index)

    override fun createLayer(name: String): Boolean {
        if (failCreateLayer) return false
        layers.add(Layer(LayerRecord(name, true, false, 1f)))
        layer = layers.size - 1
        frame = null
        return true
    }
    override fun applyLayerRecord(index: Int, record: LayerRecord): Boolean {
        val target = layers.getOrNull(index) ?: return false
        val name = if (record.name.isNotBlank()) record.name else target.record.name
        target.record = LayerRecord(name, record.visible, record.locked, record.opacity.coerceIn(0f, 1f))
        return true
    }
    override fun createFrame(frame: Int): Boolean {
        if (layers[layer].frames.any { it.number == frame }) return false
        layers[layer].frames.add(Frame(frame))
        this.frame = layers[layer].frames.last()
        return true
    }
    override fun addStroke(record: StrokeRecord): Boolean {
        if (lockedLayersRejectStrokes && !layers[layer].record.let { it.visible && !it.locked }) return false
        val target = frame ?: return false
        target.strokes.add(record)
        return true
    }
    override fun createMaterial(): Boolean {
        materials.add(MaterialRecord(floatArrayOf(0f, 0f, 0f, 1f), floatArrayOf(0f, 0f, 0f, 1f), true, false))
        return true
    }
    override fun applyMaterialRecord(index: Int, record: MaterialRecord): Boolean {
        if (index !in materials.indices) return false
        materials[index] = record
        return true
    }
}

class ProjectDocumentRoundTripTest {

    /** [x, y, z, pressure, strength, time, r, g, b, a]; every other point carries a vertex color. */
    private fun point(i: Int) = floatArrayOf(
        10f + i, 20f - i, 0f, 0.25f + 0.1f * i, 0.5f + 0.05f * i, 0.016f * i,
        if (i % 2 == 1) 0.2f * i else 0f, if (i % 2 == 1) 0.1f else 0f,
        if (i % 2 == 1) 0.9f - 0.1f * i else 0f, if (i % 2 == 1) 0.5f else 0f
    )

    private fun stroke(points: Int, material: Int, thickness: Float, cyclic: Boolean, fillOpacity: Float, fill: FloatArray) =
        StrokeRecord(List(points) { point(it) }, material, thickness, cyclic, fillOpacity, fill)

    /** 3 layers (names, hidden, locked, opacity), 2 frames on layer 0, 3 materials, varied strokes. */
    private fun sampleDocument(): FakeDocument {
        val doc = FakeDocument()
        doc.materials[0] = MaterialRecord(floatArrayOf(0.1f, 0.2f, 0.3f, 1f), floatArrayOf(0.4f, 0.5f, 0.6f, 0.5f), true, true)
        doc.createMaterial()
        doc.applyMaterialRecord(1, MaterialRecord(floatArrayOf(1f, 0f, 0f, 0.75f), floatArrayOf(0f, 1f, 0f, 0.25f), false, false))
        doc.createMaterial()
        doc.applyMaterialRecord(2, MaterialRecord(floatArrayOf(0.3f, 0.3f, 0.9f, 1f), floatArrayOf(0.9f, 0.9f, 0.1f, 1f), true, true))

        doc.applyLayerRecord(0, LayerRecord("Sketch", true, false, 1f))
        doc.createFrame(1)
        doc.addStroke(stroke(5, 0, 8f, false, 1f, floatArrayOf(0f, 0f, 0f, 0f)))
        doc.addStroke(stroke(4, 2, 17f, true, 0.4f, floatArrayOf(0.2f, 0.4f, 0.6f, 0.8f)))
        doc.createFrame(5)
        doc.addStroke(stroke(3, 1, 3f, false, 0.9f, floatArrayOf(0f, 0f, 0f, 0f)))

        doc.createLayer("Inks")
        doc.createFrame(1)
        doc.addStroke(stroke(2, 1, 40f, true, 1f, floatArrayOf(1f, 1f, 1f, 1f)))
        doc.applyLayerRecord(1, LayerRecord("Inks", false, true, 0.5f))

        doc.createLayer("Colors")
        doc.createFrame(2)
        doc.applyLayerRecord(2, LayerRecord("Colors", true, true, 0.125f))
        return doc
    }

    private fun assertSameDocument(expected: FakeDocument, actual: FakeDocument) {
        assertEquals("material count", expected.materials.size, actual.materials.size)
        expected.materials.forEachIndexed { i, e ->
            val a = actual.materials[i]
            assertArrayEquals("material $i stroke", e.stroke, a.stroke, 0f)
            assertArrayEquals("material $i fill", e.fill, a.fill, 0f)
            assertEquals("material $i visible", e.visible, a.visible)
            assertEquals("material $i fillEnabled", e.fillEnabled, a.fillEnabled)
        }
        assertEquals("layer count", expected.layers.size, actual.layers.size)
        expected.layers.forEachIndexed { li, el ->
            val al = actual.layers[li]
            assertEquals("layer $li name", el.record.name, al.record.name)
            assertEquals("layer $li visible", el.record.visible, al.record.visible)
            assertEquals("layer $li locked", el.record.locked, al.record.locked)
            assertEquals("layer $li opacity", el.record.opacity, al.record.opacity, 0f)
            assertEquals("layer $li frames", el.frames.map { it.number }, al.frames.map { it.number })
            el.frames.forEachIndexed { fi, ef ->
                val af = al.frames[fi]
                assertEquals("layer $li frame ${ef.number} stroke count", ef.strokes.size, af.strokes.size)
                ef.strokes.forEachIndexed { si, es ->
                    val a = af.strokes[si]
                    val where = "layer $li frame ${ef.number} stroke $si"
                    assertEquals("$where material", es.materialIndex, a.materialIndex)
                    assertEquals("$where thickness", es.thickness, a.thickness, 0f)
                    assertEquals("$where cyclic", es.cyclic, a.cyclic)
                    assertEquals("$where fill opacity", es.fillOpacity, a.fillOpacity, 0f)
                    assertArrayEquals("$where fill color", es.fillColor, a.fillColor, 0f)
                    assertEquals("$where points", es.points.size, a.points.size)
                    es.points.forEachIndexed { pi, ep -> assertArrayEquals("$where point $pi", ep, a.points[pi], 0f) }
                }
            }
        }
    }

    private fun save(doc: FakeDocument) = ProjectDocumentCodec.encode(doc, 1920, 1080, 24, 1)

    private fun load(raw: String, into: FakeDocument = FakeDocument(), legacyThickness: Float = 6f): FakeDocument {
        val parsed = ProjectDocumentCodec.parse(raw)
        assertNotNull(parsed)
        assertTrue(ProjectDocumentCodec.restore(parsed!!, into, legacyThickness))
        return into
    }

    @Test
    fun saveThenLoadRestoresStrokeMaterialThicknessCyclicFillLayersAndPalette() {
        val original = sampleDocument()
        val restored = load(save(original))
        assertSameDocument(original, restored)
    }

    @Test
    fun savedTextIsStableAcrossAFullRoundTrip() {
        val first = save(sampleDocument())
        val second = save(load(first))
        assertEquals(first, second)
    }

    @Test
    fun fileCarriesVersionAndCanvasSettings() {
        val parsed = ProjectDocumentCodec.parse(save(sampleDocument()))!!
        assertEquals(ProjectDocumentCodec.VERSION, parsed.version)
        assertEquals(1920, parsed.width)
        assertEquals(1080, parsed.height)
        assertEquals(24, parsed.fps)
        assertEquals(1, parsed.frame)
    }

    @Test
    fun hiddenAndLockedStateIsAppliedAfterTheStrokesAreAdded() {
        // a locked layer would refuse strokes, so the layer state has to be applied last
        val original = sampleDocument()
        val target = FakeDocument().apply { lockedLayersRejectStrokes = true }
        assertSameDocument(original, load(save(original), target))
    }

    @Test
    fun versionOneFilesStillLoadWithTheOldDefaults() {
        val v1 = """{"version":1,"width":800,"height":600,"fps":12,"frame":1,"layers":[
            {"index":0,"frames":[{"number":1,"strokes":[{"points":[[1,2,0,1,1,0],[3,4,0,1,1,0.5]]}]}]},
            {"index":1,"frames":[{"number":1,"strokes":[{"points":[[5,6,0,0.5,1,0]]}]}]}]}"""
        val doc = load(v1, legacyThickness = 9f)
        assertEquals(2, doc.layers.size)
        assertEquals("GP_Layer", doc.layers[0].record.name) // layer 0 keeps the default name
        assertEquals("Layer 2", doc.layers[1].record.name)
        val s0 = doc.layers[0].frames[0].strokes[0]
        assertEquals(0, s0.materialIndex)
        assertEquals(9f, s0.thickness, 0f)
        assertFalse(s0.cyclic)
        assertEquals(2, s0.points.size)
        assertArrayEquals(floatArrayOf(3f, 4f, 0f, 1f, 1f, 0.5f, 0f, 0f, 0f, 0f), s0.points[1], 0f)
        assertEquals(1, doc.materials.size) // no palette in the file: material 0 stays as it was
        // a file without vertex colors reads as "no vertex color" (all zero), not white
        assertEquals(StrokeRecord.POINT_SIZE, s0.points[0].size)
        assertArrayEquals(floatArrayOf(0f, 0f, 0f, 0f), s0.points[0].copyOfRange(6, 10), 0f)
        assertArrayEquals(floatArrayOf(0f, 0f, 0f, 0f), s0.fillColor, 0f)
    }

    @Test
    fun vertexColorsOfPointsAndFillSurviveARoundTrip() {
        val doc = FakeDocument()
        doc.createFrame(1)
        val tinted = StrokeRecord(
            points = listOf(
                floatArrayOf(1f, 2f, 0f, 1f, 1f, 0f, 0.25f, 0.5f, 0.75f, 0.6f),
                floatArrayOf(3f, 4f, 0f, 1f, 1f, 0.1f, 1f, 0f, 0f, 1f),
                floatArrayOf(5f, 6f, 0f, 1f, 1f, 0.2f, 0f, 0f, 0f, 0f)
            ),
            materialIndex = 0, thickness = 4f, cyclic = true, fillOpacity = 1f,
            fillColor = floatArrayOf(0.1f, 0.2f, 0.3f, 0.4f)
        )
        doc.addStroke(tinted)
        val restored = load(save(doc))
        val s = restored.layers[0].frames[0].strokes[0]
        tinted.points.forEachIndexed { i, p -> assertArrayEquals("point $i", p, s.points[i], 0f) }
        assertArrayEquals(tinted.fillColor, s.fillColor, 0f)
    }

    @Test
    fun pointsWithoutVertexColorStayCompactAndLoadAsZero() {
        val doc = FakeDocument()
        doc.createFrame(1)
        doc.addStroke(StrokeRecord(listOf(FloatArray(10) { if (it < 6) it.toFloat() else 0f })))
        val json = org.json.JSONObject(save(doc))
        val point = json.getJSONArray("layers").getJSONObject(0).getJSONArray("frames")
            .getJSONObject(0).getJSONArray("strokes").getJSONObject(0).getJSONArray("points").getJSONArray(0)
        assertEquals(6, point.length())
        val loaded = load(save(doc)).layers[0].frames[0].strokes[0].points[0]
        assertArrayEquals(floatArrayOf(0f, 0f, 0f, 0f), loaded.copyOfRange(6, 10), 0f)
    }

    @Test
    fun versionTwoFilesWithSixValuePointsStillLoad() {
        val v2 = """{"version":2,"layers":[{"index":0,"name":"A","visible":true,"locked":false,"opacity":1,
            "frames":[{"number":1,"strokes":[{"material":0,"thickness":3,"cyclic":false,"fillOpacity":1,
            "fillColor":[0,0,0,0],"points":[[1,2,0,1,1,0],[3,4,0,1,1,0.5]]}]}]}]}"""
        val s = load(v2).layers[0].frames[0].strokes[0]
        assertEquals(3f, s.thickness, 0f)
        assertEquals(2, s.points.size)
        assertArrayEquals(floatArrayOf(3f, 4f, 0f, 1f, 1f, 0.5f, 0f, 0f, 0f, 0f), s.points[1], 0f)
    }

    @Test
    fun nonFiniteValuesDoNotBreakTheSave() {
        val doc = FakeDocument()
        doc.createFrame(1)
        doc.addStroke(stroke(2, 0, Float.NaN, false, Float.POSITIVE_INFINITY, floatArrayOf(0f, 0f, 0f, 0f)))
        doc.applyLayerRecord(0, LayerRecord("L", true, false, 1f))
        val restored = load(save(doc))
        val s = restored.layers[0].frames[0].strokes[0]
        assertEquals(0f, s.thickness, 0f)
        assertEquals(0f, s.fillOpacity, 0f)
    }

    @Test
    fun aDocumentWithoutLayersLoadsAsEmptyAndGarbageIsRejected() {
        val parsed = ProjectDocumentCodec.parse("""{"version":2,"width":10,"height":10}""")!!
        assertEquals(null, parsed.layers)
        assertTrue(ProjectDocumentCodec.restore(parsed, FakeDocument(), 1f))
        assertEquals(null, ProjectDocumentCodec.parse("not json"))
    }

    @Test
    fun restoreReportsAFailingNativeCall() {
        val original = sampleDocument()
        val broken = FakeDocument().apply { failCreateLayer = true }
        assertFalse(ProjectDocumentCodec.restore(ProjectDocumentCodec.parse(save(original))!!, broken, 1f))
    }
}
