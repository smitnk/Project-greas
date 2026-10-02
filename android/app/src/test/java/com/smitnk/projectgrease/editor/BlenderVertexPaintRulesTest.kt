package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderVertexPaintRulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals(55, ProjectGreaseSelect.CMD_VERTEX_PAINT)
        assertEquals(listOf(0, 1, 2, 3, 4), listOf(ProjectGreaseSelect.VPAINT_DRAW, ProjectGreaseSelect.VPAINT_BLUR,
            ProjectGreaseSelect.VPAINT_AVERAGE, ProjectGreaseSelect.VPAINT_SMEAR, ProjectGreaseSelect.VPAINT_REPLACE))
    }

    @Test
    fun packsElevenArguments() {
        val c = ProjectGreaseSelect.vertexPaint(ProjectGreaseSelect.VPAINT_SMEAR, 1f, 2f, 20f, 0.5f, 1f, 0f, 0f,
            ProjectGreaseSelect.PAINT_BOTH, 3f, -4f)!!
        assertArrayEquals(floatArrayOf(3f, 1f, 2f, 20f, 0.5f, 1f, 0f, 0f, 2f, 3f, -4f), c.args, 0f)
    }

    @Test
    fun rejectsInvalidInput() {
        assertNull(ProjectGreaseSelect.vertexPaint(5, 0f, 0f, 10f, 1f, 0f, 0f, 0f, 0))
        assertNull(ProjectGreaseSelect.vertexPaint(0, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 0))
        assertNull(ProjectGreaseSelect.vertexPaint(0, Float.NaN, 0f, 10f, 1f, 0f, 0f, 0f, 0))
        assertNull(ProjectGreaseSelect.vertexPaint(0, 0f, 0f, 10f, 1f, 0f, 0f, 0f, 3))
    }
}
