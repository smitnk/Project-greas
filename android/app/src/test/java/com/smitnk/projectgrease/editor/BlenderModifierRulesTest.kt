package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderModifierRulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals(40, ProjectGreaseSelect.CMD_MOD_THICKNESS)
        assertEquals(41, ProjectGreaseSelect.CMD_MOD_OPACITY)
        assertEquals(listOf(0, 1, 2, 3), listOf(ProjectGreaseSelect.MODIFY_BOTH, ProjectGreaseSelect.MODIFY_STROKE,
            ProjectGreaseSelect.MODIFY_FILL, ProjectGreaseSelect.MODIFY_HARDNESS))
    }

    @Test
    fun packsArguments() {
        assertArrayEquals(floatArrayOf(0f, 0f, 1.5f), ProjectGreaseSelect.thicknessModifier(1.5f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(1f, 10f, 1f), ProjectGreaseSelect.thicknessModifier(1f, true, 10)!!.args, 0f)
        assertArrayEquals(floatArrayOf(2f, 0.5f, 1f, 1f),
            ProjectGreaseSelect.opacityModifier(ProjectGreaseSelect.MODIFY_FILL, 0.5f, true)!!.args, 0f)
    }

    @Test
    fun rejectsInvalidInput() {
        assertNull(ProjectGreaseSelect.thicknessModifier(Float.NaN))
        assertNull(ProjectGreaseSelect.thicknessModifier(1f, true, -1))
        assertNull(ProjectGreaseSelect.opacityModifier(9, 1f))
    }
}
