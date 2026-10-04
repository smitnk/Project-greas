package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** In-memory stand-in for the native document, with the same starting state (layer 0, material 0). */
internal class FakeDocument : DocumentNative {
    class Frame(val number: Int, val strokes: MutableList<StrokeRecord> = mutableListOf(), var keyType: Int = 0)
    class Layer(
        var record: LayerRecord,
        val frames: MutableList<Frame> = mutableListOf(),
        val modifiers: MutableList<ModifierRecord> = mutableListOf(),
        val effects: MutableList<FxRecord> = mutableListOf(),
        var useMask: Boolean = false,
        val masks: MutableList<MaskRecord> = mutableListOf()
    )

    val layers = mutableListOf(Layer(LayerRecord("GP_Layer", true, false, 1f)))
    val materials = mutableListOf(MaterialRecord(floatArrayOf(0f, 0f, 0f, 1f), floatArrayOf(0f, 0f, 0f, 1f), true, false))
    private var layer = 0
    private var frame: Frame? = null
    /** Mirrors the editing rules: a locked layer refuses new strokes. */
    var lockedLayersRejectStrokes = false
    var failCreateLayer = false
    val vertexGroups = mutableListOf<String>()
    var activeGroup = -1
    override fun vertexGroups() = vertexGroups.toList()
    override fun activeVertexGroup() = activeGroup
    override fun restoreVertexGroups(names: List<String>, active: Int): Boolean {
        vertexGroups.addAll(names)
        activeGroup = if (active in names.indices) active else -1
        return true
    }

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

    override fun layerUseMask(layer: Int) = layers.getOrNull(layer)?.useMask ?: false
    override fun layerMasks(layer: Int) = layers.getOrNull(layer)?.masks?.toList() ?: emptyList()
    /** Mirrors the native rule: a mask must name another existing layer, unknown names are dropped. */
    override fun restoreLayerMasks(layer: Int, useMask: Boolean, masks: List<MaskRecord>): Boolean {
        val target = layers.getOrNull(layer) ?: return false
        for (mask in masks) {
            if (mask.name == target.record.name || layers.none { it.record.name == mask.name }) continue
            target.masks.add(mask)
        }
        target.useMask = useMask
        return true
    }
    override fun modifierCount(layer: Int) = layers.getOrNull(layer)?.modifiers?.size ?: 0
    override fun modifierRecord(layer: Int, index: Int) = layers.getOrNull(layer)?.modifiers?.getOrNull(index)
    override fun addModifier(layer: Int, record: ModifierRecord): Boolean {
        val target = layers.getOrNull(layer) ?: return false
        target.modifiers.add(record)
        return true
    }

    override fun fxCount(layer: Int) = layers.getOrNull(layer)?.effects?.size ?: 0
    override fun fxRecord(layer: Int, index: Int) = layers.getOrNull(layer)?.effects?.getOrNull(index)
    override fun addFx(layer: Int, record: FxRecord): Boolean {
        val target = layers.getOrNull(layer) ?: return false
        target.effects.add(record)
        return true
    }

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
        target.record = LayerRecord(name, record.visible, record.locked, record.opacity.coerceIn(0f, 1f),
            record.blendMode, record.tint.copyOf(), record.lineChange, record.passIndex)
        return true
    }
    override fun frameKeyTypes(): Map<Int, Int> = layers[layer].frames.associate { it.number to it.keyType }
    override fun setFrameKeyType(frame: Int, type: Int): Boolean {
        val f = layers[layer].frames.firstOrNull { it.number == frame } ?: return false
        f.keyType = type
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
        // vertex groups and per-point weights (sparse: point 0 in groups 0 and 1, point 2 in group 1)
        doc.vertexGroups.addAll(listOf("Arm", "Leg"))
        doc.activeGroup = 1
        doc.layers[0].frames.last().strokes[0] = StrokeRecord(
            doc.layers[0].frames.last().strokes[0].points, 1, 3f, false, 0.9f, floatArrayOf(0f, 0f, 0f, 0f), true,
            mapOf(0 to floatArrayOf(0f, 0.25f, 1f, 1f), 2 to floatArrayOf(1f, 0.5f))
        )

        doc.createLayer("Inks")
        doc.createFrame(1)
        doc.addStroke(stroke(2, 1, 40f, true, 1f, floatArrayOf(1f, 1f, 1f, 1f)))
        doc.applyLayerRecord(1, LayerRecord("Inks", false, true, 0.5f))

        doc.createLayer("Colors")
        doc.createFrame(2)
        doc.applyLayerRecord(2, LayerRecord("Colors", true, true, 0.125f))

        // live modifier stacks: two on layer 0 (the second disabled), one on layer 2, none on layer 1
        doc.addModifier(0, ModifierRecord(ModifierType.OFFSET, true, FloatArray(ModifierSpecs.paramCount(ModifierType.OFFSET)) { 0.25f * it - 1f }))
        doc.addModifier(0, ModifierRecord(ModifierType.NOISE, false, FloatArray(ModifierSpecs.paramCount(ModifierType.NOISE)) { 0.5f + it }))
        doc.addModifier(2, ModifierRecord(ModifierType.SMOOTH, true, floatArrayOf(0.75f, 3f, 1f, 0f, 1f, 0f, 1f)))

        // masks: "Colors" is masked by "Sketch" (inverted) and "Inks" (hidden); "Inks" has the flag off
        doc.layers[2].useMask = true
        doc.layers[2].masks.add(MaskRecord("Sketch", hidden = false, inverted = true))
        doc.layers[2].masks.add(MaskRecord("Inks", hidden = true, inverted = false))
        doc.layers[0].effects.add(FxRecord(FxType.PIXEL, true, floatArrayOf(7f, 3f, 1f)))
        doc.layers[0].effects.add(FxRecord(FxType.SHADOW, false, FxSpecs.defaults(FxType.SHADOW).also { it[0] = -12f; it[13] = 0.5f }))
        doc.layers[2].effects.add(FxRecord(FxType.SWIRL, true, floatArrayOf(0.25f, 0.75f, 80f, -1.5f)))
        doc.layers[1].masks.add(MaskRecord("Sketch")) // a mask list with use-mask off still round-trips
        return doc
    }

    private fun assertSameDocument(expected: FakeDocument, actual: FakeDocument) {
        assertEquals("vertex groups", expected.vertexGroups, actual.vertexGroups)
        assertEquals("active vertex group", expected.activeGroup, actual.activeGroup)
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
            assertEquals("layer $li use mask", el.useMask, al.useMask)
            assertEquals("layer $li masks", el.masks.map { Triple(it.name, it.hidden, it.inverted) },
                al.masks.map { Triple(it.name, it.hidden, it.inverted) })
            assertEquals("layer $li modifier count", el.modifiers.size, al.modifiers.size)
            el.modifiers.forEachIndexed { mi, em ->
                val am = al.modifiers[mi]
                assertEquals("layer $li modifier $mi type", em.type, am.type)
                assertEquals("layer $li modifier $mi enabled", em.enabled, am.enabled)
                // stored entries are MAX_PARAMS long (own + curve + filter blocks, zero padded)
                assertArrayEquals("layer $li modifier $mi params", em.params.copyOf(ModifierType.MAX_PARAMS), am.params.copyOf(ModifierType.MAX_PARAMS), 0f)
            }
            assertEquals("layer $li effect count", el.effects.size, al.effects.size)
            el.effects.forEachIndexed { xi, ex ->
                val ax = al.effects[xi]
                assertEquals("layer $li effect $xi type", ex.type, ax.type)
                assertEquals("layer $li effect $xi enabled", ex.enabled, ax.enabled)
                assertArrayEquals("layer $li effect $xi params", ex.params, ax.params, 0f)
            }
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
                    assertEquals("$where weighted points", es.weights.keys, a.weights.keys)
                    es.weights.forEach { (point, values) -> assertArrayEquals("$where weights of point $point", values, a.weights[point]!!, 0f) }
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
    fun strokeCapsSurviveARoundTrip() {
        val doc = FakeDocument().apply {
            createFrame(1)
            addStroke(StrokeRecord(listOf(FloatArray(10), FloatArray(10) { if (it == 0) 5f else 0f }), caps = intArrayOf(1, 0)))
            addStroke(StrokeRecord(listOf(FloatArray(10), FloatArray(10))))
        }
        val raw = save(doc)
        val restored = load(raw)
        val strokes = restored.layers[0].frames[0].strokes
        assertArrayEquals(intArrayOf(1, 0), strokes[0].caps)
        assertArrayEquals(intArrayOf(0, 0), strokes[1].caps)
        assertEquals("default caps write no key", 1, Regex("\"caps\"").findAll(raw).count())
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
    fun theModifierStackOfEachLayerSurvivesARoundTripInOrder() {
        val restored = load(save(sampleDocument()))
        assertEquals(listOf(ModifierType.OFFSET, ModifierType.NOISE), restored.layers[0].modifiers.map { it.type })
        assertEquals(listOf(true, false), restored.layers[0].modifiers.map { it.enabled })
        assertTrue(restored.layers[1].modifiers.isEmpty())
        assertEquals(listOf(ModifierType.SMOOTH), restored.layers[2].modifiers.map { it.type })
        assertArrayEquals(floatArrayOf(0.75f, 3f, 1f, 0f, 1f, 0f, 1f), restored.layers[2].modifiers[0].params.copyOf(7), 0f)
    }

    @Test
    fun theEffectListOfEachLayerSurvivesARoundTripInOrder() {
        val restored = load(save(sampleDocument()))
        assertEquals(listOf(FxType.PIXEL, FxType.SHADOW), restored.layers[0].effects.map { it.type })
        assertEquals(listOf(true, false), restored.layers[0].effects.map { it.enabled })
        assertTrue(restored.layers[1].effects.isEmpty())
        assertArrayEquals(floatArrayOf(0.25f, 0.75f, 80f, -1.5f), restored.layers[2].effects[0].params, 0f)
    }

    @Test
    fun oldFilesAndLayersWithoutEffectsCarryNoEffectsKey() {
        val doc = ProjectDocumentCodec.parse(save(sampleDocument()))!!
        assertTrue(doc.version >= 5)
        val noFx = FakeDocument()
        noFx.applyLayerRecord(0, LayerRecord("A", true, false, 1f))
        assertFalse(save(noFx).contains("effects"))
        val v4 = """{"version":4,"layers":[{"index":0,"frames":[],"modifiers":[]}]}"""
        assertTrue(load(v4).layers[0].effects.isEmpty())
    }

    @Test
    fun unknownEffectTypesAreDroppedAndMissingParametersKeepBlendersDefaults() {
        val raw = """{"version":5,"layers":[{"index":0,"frames":[],"effects":[
            {"type":99,"enabled":true,"params":[1]},{"type":3,"enabled":true},
            {"type":${FxType.BLUR},"enabled":false,"params":[12]},{"enabled":true}]}]}"""
        val effects = load(raw).layers[0].effects
        assertEquals(listOf(FxType.BLUR), effects.map { it.type })
        assertFalse(effects[0].enabled)
        // radius X given; the rest (radius Y 50, samples 8) come from Blender's defaults, not zero
        assertArrayEquals(floatArrayOf(12f, 50f, 8f, 0f), effects[0].params, 0f)
    }

    @Test
    fun vertexGroupsAndWeightsSurviveARoundTripAndOldFilesHaveNone() {
        val restored = load(save(sampleDocument()))
        assertEquals(listOf("Arm", "Leg"), restored.vertexGroups)
        assertEquals(1, restored.activeGroup)
        val weighted = restored.layers[0].frames.last().strokes[0]
        assertEquals(setOf(0, 2), weighted.weights.keys)
        assertArrayEquals(floatArrayOf(0f, 0.25f, 1f, 1f), weighted.weights[0]!!, 0f)
        // strokes without weights write no "weights" key; a file without groups restores none
        val plain = FakeDocument().apply { createFrame(1); addStroke(StrokeRecord(listOf(FloatArray(10)))) }
        assertFalse(org.json.JSONObject(save(plain)).has("vertexGroups"))
        val stroke = org.json.JSONObject(save(plain)).getJSONArray("layers").getJSONObject(0).getJSONArray("frames")
            .getJSONObject(0).getJSONArray("strokes").getJSONObject(0)
        assertFalse(stroke.has("weights"))
        val v3 = """{"version":3,"layers":[{"index":0,"frames":[{"number":1,"strokes":[{"points":[[1,2,0,1,1,0]]}]}]}]}"""
        assertTrue(load(v3).vertexGroups.isEmpty())
        // malformed rows are dropped: bad point index, odd pair count
        val bad = """{"version":4,"vertexGroups":["A"],"layers":[{"index":0,"frames":[{"number":1,"strokes":[
            {"points":[[1,2,0,1,1,0],[3,4,0,1,1,0]],"weights":[[0,0,0.5],[9,0,1],[1,0],[1,0,1,2]]}]}]}]}"""
        val doc = load(bad)
        assertEquals(setOf(0), doc.layers[0].frames[0].strokes[0].weights.keys)
    }

    @Test
    fun layerMasksSurviveARoundTripAndRefOnlyLaterLayers() {
        val restored = load(save(sampleDocument()))
        assertTrue(restored.layers[2].useMask)
        assertEquals(listOf("Sketch" to true, "Inks" to false), restored.layers[2].masks.map { it.name to it.inverted })
        assertEquals(listOf(false, true), restored.layers[2].masks.map { it.hidden })
        assertFalse(restored.layers[1].useMask)
        assertEquals(1, restored.layers[1].masks.size)
        // a file whose first layer names a LATER layer as its mask: resolved after all layers exist
        val raw = """{"version":4,"layers":[
            {"index":0,"name":"Top","visible":true,"locked":false,"opacity":1,"frames":[],"useMask":true,
             "masks":[{"name":"Below","invert":true},{"name":"Gone"},{"name":"Top"}]},
            {"index":1,"name":"Below","visible":true,"locked":false,"opacity":1,"frames":[]}]}"""
        val doc = load(raw)
        assertEquals(listOf("Below"), doc.layers[0].masks.map { it.name }) // unknown and self references dropped
        assertTrue(doc.layers[0].masks[0].inverted)
        assertTrue(doc.layers[0].useMask)
    }

    @Test
    fun layersWithoutMasksWriteNoMaskKeysAndOldFilesHaveNone() {
        val plain = FakeDocument().apply { createFrame(1) }
        val layer = org.json.JSONObject(save(plain)).getJSONArray("layers").getJSONObject(0)
        assertFalse(layer.has("masks") || layer.has("useMask"))
        val v3 = """{"version":3,"layers":[{"index":0,"name":"A","visible":true,"locked":false,"opacity":1,"frames":[]}]}"""
        val doc = load(v3)
        assertFalse(doc.layers[0].useMask)
        assertTrue(doc.layers[0].masks.isEmpty())
    }

    @Test
    fun layersWithoutModifiersWriteNoStackAndOldFilesLoadWithAnEmptyStack() {
        val plain = FakeDocument().apply { createFrame(1) }
        val layer = org.json.JSONObject(save(plain)).getJSONArray("layers").getJSONObject(0)
        assertFalse(layer.has("modifiers"))
        // a version 3 file (written before the stack existed)
        val v3 = """{"version":3,"layers":[{"index":0,"name":"A","visible":true,"locked":false,"opacity":1,
            "frames":[{"number":1,"strokes":[{"material":0,"thickness":3,"cyclic":false,"fillOpacity":1,
            "fillColor":[0,0,0,0],"points":[[1,2,0,1,1,0],[3,4,0,1,1,0.5]]}]}]}]}"""
        val doc = load(v3)
        assertTrue(doc.layers[0].modifiers.isEmpty())
        assertEquals(2, doc.layers[0].frames[0].strokes[0].points.size)
    }

    @Test
    fun unknownModifierTypesInAFileAreDropped() {
        val raw = """{"version":4,"layers":[{"index":0,"frames":[],"modifiers":[
            {"type":99,"enabled":true,"params":[1]},{"type":${ModifierType.SUBDIV},"enabled":false,"params":[2,1]},{"enabled":true}]}]}"""
        val doc = load(raw)
        assertEquals(listOf(ModifierType.SUBDIV), doc.layers[0].modifiers.map { it.type })
        assertFalse(doc.layers[0].modifiers[0].enabled)
        assertArrayEquals(floatArrayOf(2f, 1f), doc.layers[0].modifiers[0].params.copyOf(2), 0f)
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

    @Test
    fun version6KeepsKeyTypesLayerLookAndMaterialSettings() {
        val doc = FakeDocument()
        doc.createFrame(1)
        doc.addStroke(StrokeRecord(listOf(floatArrayOf(0f, 0f, 0f, 1f, 1f, 0f), floatArrayOf(5f, 5f, 0f, 1f, 1f, 0f))))
        doc.createFrame(4)
        doc.setFrameKeyType(4, ProjectGreaseSelect.KEY_BREAKDOWN)
        doc.applyLayerRecord(0, LayerRecord("Ink", true, false, 0.8f, blendMode = 4, tint = floatArrayOf(1f, 0f, 0f, 0.5f), lineChange = 7, passIndex = 3))
        doc.applyMaterialRecord(0, MaterialRecord(floatArrayOf(0f, 0f, 1f, 1f), floatArrayOf(1f, 1f, 1f, 1f), true, false,
            name = "Blue dots", locked = true, mode = 1, alignment = 2, rotation = 0.5f, passIndex = 2))
        val raw = save(doc)
        assertTrue(raw.contains("\"version\":6"))
        val back = load(raw)
        assertEquals(ProjectGreaseSelect.KEY_BREAKDOWN, back.layers[0].frames.first { it.number == 4 }.keyType)
        assertEquals(0, back.layers[0].frames.first { it.number == 1 }.keyType)
        val l = back.layers[0].record
        assertEquals(4, l.blendMode); assertEquals(7, l.lineChange); assertEquals(3, l.passIndex)
        assertArrayEquals(floatArrayOf(1f, 0f, 0f, 0.5f), l.tint, 1e-6f)
        val m = back.materials[0]
        assertEquals("Blue dots", m.name); assertTrue(m.locked); assertEquals(1, m.mode); assertEquals(2, m.alignment)
        assertEquals(0.5f, m.rotation, 1e-6f); assertEquals(2, m.passIndex)
        // a version-5 file (no new keys) loads with the defaults
        val old = load(raw.replace("\"keyType\":2", "\"x\":0").replace("\"blend\":4", "\"y\":0"))
        assertEquals(0, old.layers[0].frames.first { it.number == 4 }.keyType)
        assertEquals(0, old.layers[0].record.blendMode)
    }
}
