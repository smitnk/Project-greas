package com.smitnk.projectgrease.editor

import java.io.File
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** Parity phase 1 commands (edit9, interpolation) against the native headers. */
class Parity1RulesTest {
    private fun header(name: String): String {
        var dir: File? = File("").absoluteFile
        while (dir != null && !File(dir, "native/blender_gp/$name").exists()) dir = dir.parentFile
        return File(dir, "native/blender_gp/$name").readText()
    }

    private fun id(text: String, name: String) = Regex("""$name\s*=\s*(\d+)""").find(text)!!.groupValues[1].toInt()

    @Test
    fun idsMatchTheNativeHeaders() {
        val h = header("project_grease_blender_edit9.h")
        assertEquals(id(h, "PG_EDIT9_CMD_TRANSFORM"), ProjectGreaseSelect.CMD_TRANSFORM)
        assertEquals(id(h, "PG_EDIT9_CMD_FRAMES_SELECT_RANGE"), ProjectGreaseSelect.CMD_FRAMES_SELECT_RANGE)
        assertEquals(id(h, "PG_EDIT9_CMD_FRAMES_MOVE"), ProjectGreaseSelect.CMD_FRAMES_MOVE)
        assertEquals(id(h, "PG_EDIT9_CMD_FRAMES_SCALE"), ProjectGreaseSelect.CMD_FRAMES_SCALE)
        assertEquals(id(h, "PG_EDIT9_CMD_FRAMES_COPY"), ProjectGreaseSelect.CMD_FRAMES_COPY)
        assertEquals(id(h, "PG_EDIT9_CMD_FRAMES_PASTE"), ProjectGreaseSelect.CMD_FRAMES_PASTE)
        assertEquals(id(h, "PG_EDIT9_CMD_DASH_SEGMENTS"), ProjectGreaseSelect.CMD_DASH_SEGMENTS)
        assertEquals(id(h, "PG_EDIT9_CMD_MATERIAL_GRADIENT"), ProjectGreaseSelect.CMD_MATERIAL_GRADIENT)
        assertEquals(id(h, "PG_EDIT9_CMD_MATERIAL_OPTIONS"), ProjectGreaseSelect.CMD_MATERIAL_OPTIONS)
        assertEquals(id(h, "PG_PIVOT_CURSOR"), ProjectGreaseSelect.PIVOT_CURSOR)
        assertEquals(id(h, "PG_PROP_INVSQUARE"), ProjectGreaseSelect.FALLOFF_VALUES.last())
        assertTrue(header("project_grease_blender_interp.h").contains("#define PG_INTERP_CMD ${ProjectGreaseSelect.CMD_INTERPOLATE}"))
        val s = header("project_grease_tool_session.h")
        val names = Regex("""PG_TOOL_P_[A-Z_]+""").findAll(s.substringAfter("PG_TOOL_P_BRUSH = 0").substringBefore("};"))
            .map { it.value }.toList()
        assertEquals(names.indexOf("PG_TOOL_P_AUTOMASK") + 1, ToolSession.P_AUTOMASK)
        assertEquals(names.indexOf("PG_TOOL_P_COUNT") + 1, ToolSession.P_COUNT)
    }

    @Test
    fun transformPacksElevenArguments() {
        val s = ProjectGreaseSelect.TransformSettings(pivot = ProjectGreaseSelect.PIVOT_CURSOR, cursorX = 5f, cursorY = 6f,
            proportional = true, connected = true, falloff = 4, size = 50f)
        val c = ProjectGreaseSelect.transform(ProjectGreaseSelect.XFORM_ROTATE, 0.5f, 0f, s)!!
        assertArrayEquals(floatArrayOf(1f, 0.5f, 0f, 3f, 5f, 6f, 1f, 1f, 4f, 50f, 0f), c.args, 0f)
        assertTrue(s.needsEdit9)
        assertFalse(ProjectGreaseSelect.TransformSettings().needsEdit9)
        assertNull(ProjectGreaseSelect.transform(7, 0f, 0f, s))
        assertNull(ProjectGreaseSelect.transform(0, Float.NaN, 0f, s))
    }

    @Test
    fun frameDashGradientAndInterpolationArguments() {
        assertArrayEquals(floatArrayOf(2f, 9f, 1f, 0f), ProjectGreaseSelect.framesSelectRange(9, 2, extend = true).args, 0f)
        assertNull(ProjectGreaseSelect.framesScale(1, 0f))
        assertArrayEquals(floatArrayOf(1f, 2f, 3f, 1f, 2f, 0f), ProjectGreaseSelect.dashSegments(1, listOf(3 to 1, 2 to 0))!!.args, 0f)
        assertNull(ProjectGreaseSelect.dashSegments(0, emptyList()))
        assertNull(ProjectGreaseSelect.dashSegments(0, listOf(0 to 1)))
        val g = FloatArray(12).also { it[0] = 1f }
        assertEquals(14, ProjectGreaseSelect.materialGradient(0, true, g)!!.args.size)
        assertNull(ProjectGreaseSelect.materialGradient(0, true, FloatArray(12).also { it[0] = 2f }))
        val i = ProjectGreaseSelect.interpolate(4, 0, 9, true, false, 1, 2, 5f, 7, single = true)
        assertArrayEquals(floatArrayOf(4f, 1f, 2f, 1f, 0f, 0f, 1f, 2f, 2f, 3f, 1f), i.args, 0f)
    }

    @Test
    fun brushParamsCarryMaskingAndCurve() {
        val p = ToolSession.brushParams(ToolSession.GPSCULPT_CLONE, 10f, 1f, 1f, automask = ToolSession.AUTOMASK_STROKE,
            selectMask = ToolSession.SELECT_MASK_POINT, curvePreset = 8, activeMaterial = 2)
        assertEquals(8f, p[ToolSession.P_BRUSH], 0f)
        assertEquals(16f, p[ToolSession.P_AUTOMASK], 0f)
        assertEquals(1f, p[ToolSession.P_SELECT_MASK], 0f)
        assertEquals(8f, p[ToolSession.P_CURVE_PRESET], 0f)
        assertEquals(2f, p[ToolSession.P_ACTIVE_MATERIAL], 0f)
        assertEquals(ToolSession.GPVERTEX_TINT, ToolSession.vertexTool(ProjectGreaseSelect.VPAINT_TINT))
        assertEquals(8, ToolSession.sculptTool(SculptBrush.CLONE))
    }
}
