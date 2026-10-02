package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderColorModifierRulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals(43, ProjectGreaseSelect.CMD_MOD_TINT)
        assertEquals(44, ProjectGreaseSelect.CMD_MOD_COLOR)
        assertEquals(listOf(0, 1, 2), listOf(ProjectGreaseSelect.PAINT_STROKE, ProjectGreaseSelect.PAINT_FILL, ProjectGreaseSelect.PAINT_BOTH))
    }

    @Test
    fun tintPacksModeFactorAndColor() {
        assertArrayEquals(floatArrayOf(2f, 0.5f, 1f, 0f, 0.25f),
            ProjectGreaseSelect.tintModifier(ProjectGreaseSelect.PAINT_BOTH, 0.5f, 1f, 0f, 0.25f)!!.args, 0f)
        assertNull(ProjectGreaseSelect.tintModifier(7, 0.5f, 1f, 0f, 0f))
        assertNull(ProjectGreaseSelect.tintModifier(0, Float.NaN, 1f, 0f, 0f))
    }

    @Test
    fun colorDefaultsAreBlendersIdentity() {
        assertArrayEquals(floatArrayOf(0f, 0.5f, 1f, 1f),
            ProjectGreaseSelect.colorModifier(ProjectGreaseSelect.MODIFY_BOTH)!!.args, 0f)
        assertNull(ProjectGreaseSelect.colorModifier(ProjectGreaseSelect.MODIFY_HARDNESS))
        assertNull(ProjectGreaseSelect.colorModifier(ProjectGreaseSelect.MODIFY_STROKE, hue = Float.NaN))
    }
}
