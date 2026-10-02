package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderEdit2RulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals(listOf(58, 59, 60, 61, 62, 63), listOf(
            ProjectGreaseSelect.CMD_MOD_THICKNESS_VGROUP, ProjectGreaseSelect.CMD_SELECT_VCOLOR, ProjectGreaseSelect.CMD_NORMALIZE,
            ProjectGreaseSelect.CMD_SIMPLIFY_FIXED, ProjectGreaseSelect.CMD_SAMPLE, ProjectGreaseSelect.CMD_EXTRUDE))
    }

    @Test
    fun packsArguments() {
        assertArrayEquals(floatArrayOf(2f, 1f, 0f, 0f, 1.5f), ProjectGreaseSelect.thicknessModifierVGroup(2, true, 1.5f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(1f, 0f, 0f, 0.05f, 0f), ProjectGreaseSelect.selectVertexColor(1f, 0f, 0f, 0.05f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(1f, 0.5f), ProjectGreaseSelect.normalize(ProjectGreaseSelect.NORMALIZE_OPACITY, 0.5f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(3f), ProjectGreaseSelect.simplifyFixed(3)!!.args, 0f)
        assertArrayEquals(floatArrayOf(8f, 0.1f), ProjectGreaseSelect.sample(8f)!!.args, 0f)
        assertEquals(0, ProjectGreaseSelect.extrude().args.size)
    }

    @Test
    fun rejectsInvalidInput() {
        assertNull(ProjectGreaseSelect.thicknessModifierVGroup(-2, false, 1f))
        assertNull(ProjectGreaseSelect.selectVertexColor(1f, 0f, 0f, -0.1f))
        assertNull(ProjectGreaseSelect.normalize(2, 1f))
        assertNull(ProjectGreaseSelect.simplifyFixed(0))
        assertNull(ProjectGreaseSelect.sample(0f))
    }
}
