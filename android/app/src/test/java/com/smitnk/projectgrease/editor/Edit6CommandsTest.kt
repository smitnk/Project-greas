package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/** Packing of the edit6 commands (project_grease_blender_edit6.h) and the effect target in project JSON. */
class Edit6CommandsTest {
    @Test fun outlineCommand() {
        val c = ProjectGreaseSelect.outline(3, 8)!!
        assertEquals(90, c.id)
        assertArrayEquals(floatArrayOf(3f, 8f), c.args, 0f)
        assertNull(ProjectGreaseSelect.outline(0))
        assertNull(ProjectGreaseSelect.outline(2, 65))
    }

    @Test fun onionStyleCommand() {
        val c = ProjectGreaseSelect.onionStyle(ProjectGreaseSelect.ONION_MODE_SELECTED, true, false, 0xFFFF0000.toInt(), 0xFF0000FF.toInt())!!
        assertEquals(91, c.id)
        assertArrayEquals(floatArrayOf(2f, 1f, 0f, 1f, 0f, 0f, 0f, 0f, 1f), c.args, 1e-6f)
        assertNull(ProjectGreaseSelect.onionStyle(3, false, false, 0, 0))
    }

    @Test fun materialTextureCommand() {
        val c = ProjectGreaseSelect.materialTexture(1, true, true, 2f, 2f, 3f, 0.1f, 0.2f, 0.5f, 50f)!!
        assertEquals(92, c.id)
        assertArrayEquals(floatArrayOf(1f, 1f, 1f, 1f, 2f, 3f, 0.1f, 0.2f, 0.5f, 50f), c.args, 1e-6f) // mix clamped to 1
        assertNull(ProjectGreaseSelect.materialTexture(0, false, true, 0f, scaleX = 0f))
        assertNull(ProjectGreaseSelect.materialTexture(-1, false, true, 0f))
    }

    @Test fun effectTargetSurvivesJson() {
        val records = listOf(
            FxRecord(FxType.FLIP, true, FxSpecs.defaults(FxType.FLIP), FxTarget.FILLS),
            FxRecord(FxType.PIXEL, true, FxSpecs.defaults(FxType.PIXEL))
        )
        val back = FxJson.fromJson(org.json.JSONArray(FxJson.toJson(records).toString()))
        assertEquals(FxTarget.FILLS, back[0].target)
        assertEquals(FxTarget.LAYER, back[1].target)
        val bad = org.json.JSONArray("[{\"type\":${FxType.FLIP},\"target\":9}]")
        assertEquals(FxTarget.LAYER, FxJson.fromJson(bad)[0].target)
    }
}
