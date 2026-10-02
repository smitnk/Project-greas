package com.smitnk.projectgrease.editor

import java.io.File
import org.json.JSONArray
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** In-memory native stack with the same rules as Backend::modifier_* (clamping aside). */
private class FakeModifierNative(layers: Int = 2) : ModifierNative {
    val stacks = MutableList(layers) { mutableListOf<ModifierRecord>() }
    var setParamsCalls = 0
    var lastSetParams: FloatArray? = null

    private fun stack(layer: Int) = stacks.getOrNull(layer)
    override fun modifierCount(layer: Int) = stack(layer)?.size ?: 0
    override fun modifierAdd(layer: Int, type: Int): Int {
        val s = stack(layer) ?: return -1
        if (!ModifierType.isValid(type) || s.size >= ModifierType.MAX_STACK) return -1
        s.add(ModifierRecord(type, true, FloatArray(ModifierSpecs.paramCount(type))))
        return s.size - 1
    }
    override fun modifierRemove(layer: Int, index: Int): Boolean {
        val s = stack(layer) ?: return false
        if (index !in s.indices) return false
        s.removeAt(index)
        return true
    }
    override fun modifierMove(layer: Int, from: Int, to: Int): Boolean {
        val s = stack(layer) ?: return false
        if (from !in s.indices || to !in s.indices) return false
        s.add(to, s.removeAt(from))
        return true
    }
    override fun modifierSetEnabled(layer: Int, index: Int, enabled: Boolean): Boolean {
        val s = stack(layer) ?: return false
        val m = s.getOrNull(index) ?: return false
        s[index] = ModifierRecord(m.type, enabled, m.params)
        return true
    }
    override fun modifierSetParams(layer: Int, index: Int, params: FloatArray): Boolean {
        val s = stack(layer) ?: return false
        val m = s.getOrNull(index) ?: return false
        setParamsCalls++
        lastSetParams = params
        s[index] = ModifierRecord(m.type, m.enabled, FloatArray(m.params.size) { params.getOrElse(it) { 0f } })
        return true
    }
    override fun modifierGet(layer: Int, index: Int): FloatArray? =
        stack(layer)?.getOrNull(index)?.let { ModifierStackPacking.pack(it) }
    override fun modifierApply(layer: Int, index: Int) = modifierRemove(layer, index)
}

class ModifierStackTest {
    private fun repoFile(relative: String): File {
        System.getenv("PG_REPO_ROOT")?.let { root -> File(root, relative).takeIf { it.exists() }?.let { return it } }
        var dir: File? = File(System.getProperty("user.dir")).absoluteFile
        while (dir != null) {
            val candidate = File(dir, relative)
            if (candidate.exists()) return candidate
            dir = dir.parentFile
        }
        throw AssertionError("$relative not found above ${System.getProperty("user.dir")}")
    }

    @Test
    fun parameterCountsMatchTheNativeHeader() {
        val header = repoFile("native/blender_gp/project_grease_modifier_stack.h").readText()
        val counts = Regex("""PG_P_([A-Z]+)_COUNT\s*=\s*(\d+)""").findAll(header).associate { it.groupValues[1] to it.groupValues[2].toInt() }
        val byType = mapOf(
            ModifierType.THICKNESS to "THICK", ModifierType.OPACITY to "OPACITY", ModifierType.TINT to "TINT",
            ModifierType.COLOR to "COLOR", ModifierType.LENGTH to "LENGTH", ModifierType.SMOOTH to "SMOOTH",
            ModifierType.SIMPLIFY to "SIMPLIFY", ModifierType.SUBDIV to "SUBDIV", ModifierType.OFFSET to "OFFSET",
            ModifierType.NOISE to "NOISE"
        )
        assertEquals(ModifierType.all.size, byType.size)
        for ((type, key) in byType) {
            assertEquals("param count of $key", counts[key], ModifierSpecs.paramCount(type))
        }
        val types = Regex("""PG_MOD_[A-Z]+\s*=\s*(\d+)""").findAll(header).map { it.groupValues[1].toInt() }.toList()
        assertEquals(ModifierType.LAST, types.maxOrNull())
        assertTrue(ModifierSpecs.paramCount(ModifierType.OFFSET) <= ModifierType.MAX_PARAMS)
        assertTrue(header.contains("#define PG_MOD_MAX_PARAMS ${ModifierType.MAX_PARAMS}"))
        assertTrue(header.contains("#define PG_MOD_MAX_STACK ${ModifierType.MAX_STACK}"))
    }

    @Test
    fun parameterSpecsStayInsideTheParameterArray() {
        for (type in ModifierType.all) {
            val specs = ModifierSpecs.specs(type)
            assertTrue("type $type has specs", specs.isNotEmpty())
            assertEquals("distinct indices of $type", specs.size, specs.map { it.index }.toSet().size)
            for (spec in specs) {
                assertTrue("${spec.label} index", spec.index in 0 until ModifierSpecs.paramCount(type))
                assertTrue("${spec.label} range", spec.max > spec.min)
                if (spec.kind == ParamKind.ENUM) assertEquals(spec.options.size - 1f, spec.max, 0f)
            }
        }
        assertTrue(ModifierSpecs.specs(0).isEmpty() && ModifierSpecs.specs(99).isEmpty())
    }

    @Test
    fun packingRoundTripsAndPadsOrTruncatesToTheTypesParameterCount() {
        val noise = ModifierRecord(ModifierType.NOISE, false, FloatArray(10) { it * 0.5f })
        val packed = ModifierStackPacking.pack(noise)
        assertEquals(2 + 10, packed.size)
        assertEquals(ModifierType.NOISE.toFloat(), packed[0], 0f)
        assertEquals(0f, packed[1], 0f)
        val back = ModifierStackPacking.unpack(packed)!!
        assertEquals(ModifierType.NOISE, back.type)
        assertFalse(back.enabled)
        assertArrayEquals(noise.params, back.params, 0f)
        // too few values are zero-padded, too many are cut
        val short = ModifierStackPacking.pack(ModifierRecord(ModifierType.SUBDIV, true, floatArrayOf(3f)))
        assertArrayEquals(floatArrayOf(ModifierType.SUBDIV.toFloat(), 1f, 3f, 0f), short, 0f)
        assertEquals(3, ModifierStackPacking.paramsFor(ModifierType.THICKNESS, FloatArray(24) { 1f }).size)
        // malformed arrays
        assertNull(ModifierStackPacking.unpack(null))
        assertNull(ModifierStackPacking.unpack(floatArrayOf(1f)))
        assertNull(ModifierStackPacking.unpack(floatArrayOf(99f, 1f)))
        assertNull(ModifierStackPacking.unpack(floatArrayOf(0f, 1f)))
    }

    @Test
    fun commandsAddRemoveMoveAndToggle() {
        val native = FakeModifierNative()
        assertEquals(-1, ModifierStackCommands.add(native, 0, 0))
        assertEquals(-1, ModifierStackCommands.add(native, 0, 11))
        assertEquals(-1, ModifierStackCommands.add(native, 7, ModifierType.OFFSET))
        assertEquals(0, ModifierStackCommands.add(native, 0, ModifierType.OFFSET))
        assertEquals(1, ModifierStackCommands.add(native, 0, ModifierType.SMOOTH))
        assertEquals(2, ModifierStackCommands.add(native, 0, ModifierType.NOISE))
        assertEquals(0, ModifierStackCommands.add(native, 1, ModifierType.THICKNESS))
        assertEquals(listOf(ModifierType.OFFSET, ModifierType.SMOOTH, ModifierType.NOISE), ModifierStackCommands.list(native, 0).map { it.type })

        assertFalse(ModifierStackCommands.moveBy(native, 0, 0, -1)) // already first
        assertFalse(ModifierStackCommands.moveBy(native, 0, 2, 1)) // already last
        assertTrue(ModifierStackCommands.moveBy(native, 0, 2, -1))
        assertEquals(listOf(ModifierType.OFFSET, ModifierType.NOISE, ModifierType.SMOOTH), ModifierStackCommands.list(native, 0).map { it.type })

        assertTrue(ModifierStackCommands.setEnabled(native, 0, 1, false))
        assertEquals(listOf(true, false, true), ModifierStackCommands.list(native, 0).map { it.enabled })
        assertFalse(ModifierStackCommands.setEnabled(native, 0, 5, false))

        assertTrue(ModifierStackCommands.remove(native, 0, 0))
        assertFalse(ModifierStackCommands.remove(native, 0, 2))
        assertEquals(listOf(ModifierType.NOISE, ModifierType.SMOOTH), ModifierStackCommands.list(native, 0).map { it.type })
        assertEquals(1, native.modifierCount(1)) // the other layer is untouched

        assertTrue(ModifierStackCommands.apply(native, 0, 0)) // Apply removes the entry from the stack
        assertEquals(listOf(ModifierType.SMOOTH), ModifierStackCommands.list(native, 0).map { it.type })
    }

    @Test
    fun setParamWritesOneValueClampedToItsRangeAndKeepsTheOthers() {
        val native = FakeModifierNative()
        ModifierStackCommands.add(native, 0, ModifierType.SMOOTH)
        assertTrue(ModifierStackCommands.setParam(native, 0, 0, 0, 0.8f))
        assertTrue(ModifierStackCommands.setParam(native, 0, 0, 1, 99f)) // repeat is capped at 30
        val params = ModifierStackCommands.list(native, 0)[0].params
        assertEquals(0.8f, params[0], 0f)
        assertEquals(30f, params[1], 0f)
        assertEquals(ModifierSpecs.paramCount(ModifierType.SMOOTH), native.lastSetParams!!.size) // packed to exactly the count
        assertFalse(ModifierStackCommands.setParam(native, 0, 0, 7, 1f)) // out of range index
        assertFalse(ModifierStackCommands.setParam(native, 0, 0, 0, Float.NaN))
        assertFalse(ModifierStackCommands.setParam(native, 0, 3, 0, 1f)) // no such modifier
        assertEquals(2, native.setParamsCalls)
    }

    @Test
    fun jsonRoundTripKeepsOrderEnabledFlagAndParameters() {
        val records = listOf(
            ModifierRecord(ModifierType.OFFSET, true, FloatArray(23) { it * 0.125f - 1f }),
            ModifierRecord(ModifierType.SIMPLIFY, false, floatArrayOf(1f, 2f, 0.5f, 0.25f, 0.1f, 0.2f))
        )
        val back = ModifierStackJson.fromJson(JSONArray(ModifierStackJson.toJson(records).toString()))
        assertEquals(2, back.size)
        for (i in records.indices) {
            assertEquals(records[i].type, back[i].type)
            assertEquals(records[i].enabled, back[i].enabled)
            assertArrayEquals(records[i].params, back[i].params, 0f)
        }
    }

    @Test
    fun jsonDropsUnknownTypesAcceptsMissingKeyAndSurvivesBadNumbers() {
        assertTrue(ModifierStackJson.fromJson(null).isEmpty())
        val array = JSONArray("""[{"type":0},{"type":11},"x",{"type":${ModifierType.TINT},"enabled":true}]""")
        val out = ModifierStackJson.fromJson(array)
        assertEquals(1, out.size)
        assertEquals(ModifierSpecs.paramCount(ModifierType.TINT), out[0].params.size) // missing params read as zeros
        val saved = ModifierStackJson.toJson(listOf(ModifierRecord(ModifierType.SUBDIV, true, floatArrayOf(Float.NaN, Float.POSITIVE_INFINITY))))
        assertEquals(0.0, saved.getJSONObject(0).getJSONArray("params").getDouble(0), 0.0)
        assertEquals(0.0, saved.getJSONObject(0).getJSONArray("params").getDouble(1), 0.0)
        val tooMany = JSONArray().apply { repeat(ModifierType.MAX_STACK + 5) { put(org.json.JSONObject().put("type", ModifierType.SMOOTH)) } }
        assertEquals(ModifierType.MAX_STACK, ModifierStackJson.fromJson(tooMany).size)
        assertNotNull(ModifierType.name(ModifierType.NOISE))
    }
}
