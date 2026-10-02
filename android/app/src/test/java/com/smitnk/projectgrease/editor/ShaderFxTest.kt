package com.smitnk.projectgrease.editor

import java.io.File
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** In-memory native effect list with the same rules as Backend::fx_* (clamping aside). */
private class FakeFxNative(layers: Int = 2) : FxNative {
    val lists = MutableList(layers) { mutableListOf<FxRecord>() }
    var lastSetParams: FloatArray? = null

    private fun list(layer: Int) = lists.getOrNull(layer)
    override fun fxCount(layer: Int) = list(layer)?.size ?: 0
    override fun fxAdd(layer: Int, type: Int): Int {
        val l = list(layer) ?: return -1
        if (!FxType.isValid(type) || l.size >= FxType.MAX_STACK) return -1
        l.add(FxRecord(type, true, FxSpecs.defaults(type)))
        return l.size - 1
    }
    override fun fxRemove(layer: Int, index: Int): Boolean {
        val l = list(layer) ?: return false
        if (index !in l.indices) return false
        l.removeAt(index)
        return true
    }
    override fun fxMove(layer: Int, from: Int, to: Int): Boolean {
        val l = list(layer) ?: return false
        if (from !in l.indices || to !in l.indices) return false
        l.add(to, l.removeAt(from))
        return true
    }
    override fun fxSetEnabled(layer: Int, index: Int, enabled: Boolean): Boolean {
        val l = list(layer) ?: return false
        val e = l.getOrNull(index) ?: return false
        l[index] = FxRecord(e.type, enabled, e.params)
        return true
    }
    override fun fxSetParams(layer: Int, index: Int, params: FloatArray): Boolean {
        val l = list(layer) ?: return false
        val e = l.getOrNull(index) ?: return false
        lastSetParams = params
        l[index] = FxRecord(e.type, e.enabled, FloatArray(e.params.size) { params.getOrElse(it) { 0f } })
        return true
    }
    override fun fxGet(layer: Int, index: Int): FloatArray? = list(layer)?.getOrNull(index)?.let { FxPacking.pack(it) }
}

class ShaderFxTest {
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
    fun parameterCountsAndTypesMatchTheNativeHeader() {
        val header = repoFile("native/blender_gp/project_grease_shader_fx.h").readText()
        val counts = Regex("""PG_FXP_([A-Z]+)_COUNT\s*=\s*(\d+)""").findAll(header).associate { it.groupValues[1] to it.groupValues[2].toInt() }
        val byType = mapOf(
            FxType.BLUR to "BLUR", FxType.COLORIZE to "COLORIZE", FxType.FLIP to "FLIP", FxType.GLOW to "GLOW",
            FxType.PIXEL to "PIXEL", FxType.RIM to "RIM", FxType.SHADOW to "SHADOW", FxType.SWIRL to "SWIRL",
            FxType.WAVE to "WAVE"
        )
        assertEquals(FxType.all.toSet(), byType.keys)
        for ((type, key) in byType) assertEquals("param count of $key", counts[key], FxSpecs.paramCount(type))
        val types = Regex("""PG_FX_([A-Z]+)\s*=\s*(\d+)""").findAll(header)
            .filter { it.groupValues[1] != "NONE" }.associate { it.groupValues[1] to it.groupValues[2].toInt() }
        for ((type, key) in byType) assertEquals("type value of $key", types[key], type)
        assertTrue(header.contains("#define PG_FX_MAX_PARAMS ${FxType.MAX_PARAMS}"))
        assertTrue(header.contains("#define PG_FX_MAX_STACK ${FxType.MAX_STACK}"))
        assertTrue(FxType.all.all { FxSpecs.paramCount(it) <= FxType.MAX_PARAMS })
    }

    @Test
    fun defaultsMatchTheNativeHeaderEnumsAndBlendersInitData() {
        // indices come from the header (PG_FXP_*), values from Blender's FX_shader_*.c initData()
        val header = repoFile("native/blender_gp/project_grease_shader_fx.h").readText()
        fun idx(name: String) = Regex("""PG_FXP_$name\s*=\s*(\d+)""").find(header)!!.groupValues[1].toInt()
        val blur = FxSpecs.defaults(FxType.BLUR)
        assertEquals(50f, blur[idx("BLUR_RADIUS_X")], 0f)
        assertEquals(50f, blur[idx("BLUR_RADIUS_Y")], 0f)
        assertEquals(8f, blur[idx("BLUR_SAMPLES")], 0f)
        val colorize = FxSpecs.defaults(FxType.COLORIZE)
        assertEquals(0.5f, colorize[idx("COLORIZE_FACTOR")], 0f)
        assertEquals(1f, colorize[idx("COLORIZE_HIGH")], 0f)
        assertEquals(0f, colorize[idx("COLORIZE_LOW")], 0f)
        assertEquals(1f, FxSpecs.defaults(FxType.FLIP)[idx("FLIP_HORIZONTAL")], 0f)
        val glow = FxSpecs.defaults(FxType.GLOW)
        assertEquals(0.75f, glow[idx("GLOW_COLOR")], 0f)
        assertEquals(0.1f, glow[idx("GLOW_THRESHOLD")], 0f)
        assertEquals(8f, glow[idx("GLOW_SAMPLES")], 0f)
        assertEquals(5f, FxSpecs.defaults(FxType.PIXEL)[idx("PIXEL_SIZE_X")], 0f)
        val rim = FxSpecs.defaults(FxType.RIM)
        assertEquals(50f, rim[idx("RIM_OFFSET_X")], 0f)
        assertEquals(-100f, rim[idx("RIM_OFFSET_Y")], 0f)
        assertEquals(1f, rim[idx("RIM_MODE")], 0f)
        assertEquals(2f, rim[idx("RIM_SAMPLES")], 0f)
        val shadow = FxSpecs.defaults(FxType.SHADOW)
        assertEquals(15f, shadow[idx("SHADOW_OFFSET_X")], 0f)
        assertEquals(20f, shadow[idx("SHADOW_OFFSET_Y")], 0f)
        assertEquals(0.8f, shadow[idx("SHADOW_COLOR") + 3], 0f)
        assertEquals(1f, shadow[idx("SHADOW_SCALE_X")], 0f)
        assertEquals(2f, shadow[idx("SHADOW_SAMPLES")], 0f)
        val swirl = FxSpecs.defaults(FxType.SWIRL)
        assertEquals(0.5f, swirl[idx("SWIRL_CENTER_X")], 0f)
        assertEquals(100f, swirl[idx("SWIRL_RADIUS")], 0f)
        assertEquals(1.5707964f, swirl[idx("SWIRL_ANGLE")], 1e-6f)
        val wave = FxSpecs.defaults(FxType.WAVE)
        assertEquals(10f, wave[idx("WAVE_AMPLITUDE")], 0f)
        assertEquals(20f, wave[idx("WAVE_PERIOD")], 0f)
        assertEquals(1f, wave[idx("WAVE_ORIENTATION")], 0f)
    }

    @Test
    fun specsStayInsideTheParameterArrayAndSliderRangesContainTheDefaults() {
        for (type in FxType.all) {
            val specs = FxSpecs.specs(type)
            assertTrue("type $type has specs", specs.isNotEmpty())
            assertEquals("distinct indices of $type", specs.size, specs.map { it.index }.toSet().size)
            val defaults = FxSpecs.defaults(type)
            for (spec in specs) {
                assertTrue("${spec.label} index", spec.index in 0 until FxSpecs.paramCount(type))
                assertTrue("${spec.label} range", spec.max > spec.min)
                if (spec.kind == ParamKind.ENUM) assertEquals(spec.options.size - 1f, spec.max, 0f)
                val d = defaults[spec.index]
                assertTrue("${FxType.name(type)} ${spec.label} default $d in ${spec.min}..${spec.max}", d >= spec.min && d <= spec.max)
            }
        }
    }

    @Test
    fun packingRoundTripsAndDropsUnknownTypes() {
        val record = FxRecord(FxType.RIM, false, FloatArray(FxSpecs.paramCount(FxType.RIM)) { it * 0.5f })
        val packed = FxPacking.pack(record)
        assertEquals(2 + FxSpecs.paramCount(FxType.RIM), packed.size)
        val back = FxPacking.unpack(packed)!!
        assertEquals(FxType.RIM, back.type)
        assertFalse(back.enabled)
        assertArrayEquals(record.params, back.params, 0f)
        assertNull(FxPacking.unpack(floatArrayOf(99f, 1f)))
        assertNull(FxPacking.unpack(floatArrayOf(3f, 1f))) // the deprecated Light effect
        assertNull(FxPacking.unpack(null))
        assertEquals(FxSpecs.paramCount(FxType.WAVE), FxPacking.paramsFor(FxType.WAVE, FloatArray(30)).size)
    }

    @Test
    fun commandsAddMoveToggleAndEditParameters() {
        val native = FakeFxNative()
        assertEquals(-1, FxCommands.add(native, 0, 99))
        assertEquals(0, FxCommands.add(native, 0, FxType.BLUR))
        assertEquals(1, FxCommands.add(native, 0, FxType.FLIP))
        assertEquals(0, FxCommands.add(native, 1, FxType.WAVE))
        assertFalse(FxCommands.moveBy(native, 0, 0, -1))
        assertTrue(FxCommands.moveBy(native, 0, 0, 1))
        assertEquals(listOf(FxType.FLIP, FxType.BLUR), FxCommands.list(native, 0).map { it.type })
        assertEquals(listOf(FxType.WAVE), FxCommands.list(native, 1).map { it.type })
        assertTrue(FxCommands.setEnabled(native, 0, 1, false))
        assertFalse(FxCommands.list(native, 0)[1].enabled)
        // a parameter write keeps the others and is clamped to the slider range
        assertTrue(FxCommands.setParam(native, 0, 1, 0, 9999f))
        val blur = FxCommands.list(native, 0)[1]
        assertEquals(300f, blur.params[0], 0f)
        assertEquals(50f, blur.params[1], 0f)
        assertEquals(FxSpecs.paramCount(FxType.BLUR), native.lastSetParams!!.size)
        assertFalse(FxCommands.setParam(native, 0, 1, 9, 1f))
        assertFalse(FxCommands.setParam(native, 0, 1, 0, Float.NaN))
        assertFalse(FxCommands.setParam(native, 0, 7, 0, 1f))
        assertTrue(FxCommands.remove(native, 0, 0))
        assertEquals(1, native.fxCount(0))
        assertEquals(0, native.fxCount(5))
        for (i in 0 until FxType.MAX_STACK) FxCommands.add(native, 1, FxType.PIXEL)
        assertEquals(FxType.MAX_STACK, native.fxCount(1))
        assertEquals(-1, FxCommands.add(native, 1, FxType.PIXEL))
    }

    @Test
    fun jsonKeepsOrderEnabledFlagsAndParameters() {
        val records = listOf(
            FxRecord(FxType.GLOW, true, FxSpecs.defaults(FxType.GLOW).also { it[7] = 0.33f }),
            FxRecord(FxType.COLORIZE, false, FxSpecs.defaults(FxType.COLORIZE))
        )
        val back = FxJson.fromJson(FxJson.toJson(records))
        assertEquals(listOf(FxType.GLOW, FxType.COLORIZE), back.map { it.type })
        assertEquals(listOf(true, false), back.map { it.enabled })
        assertEquals(0.33f, back[0].params[7], 1e-6f)
        assertTrue(FxJson.fromJson(null).isEmpty())
        assertNotNull(FxJson.toJson(emptyList()))
    }
}
