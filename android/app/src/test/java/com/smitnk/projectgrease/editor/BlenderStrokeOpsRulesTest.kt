package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderStrokeOpsRulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals(listOf(45, 46, 47, 48, 49, 50), listOf(
            ProjectGreaseSelect.CMD_ARRANGE, ProjectGreaseSelect.CMD_SET_MATERIAL, ProjectGreaseSelect.CMD_RESET_VCOLOR,
            ProjectGreaseSelect.CMD_FLIP, ProjectGreaseSelect.CMD_CYCLIC, ProjectGreaseSelect.CMD_SNAP_GRID))
    }

    @Test
    fun packsArguments() {
        assertArrayEquals(floatArrayOf(3f), ProjectGreaseSelect.arrange(ProjectGreaseSelect.ARRANGE_BOTTOM)!!.args, 0f)
        assertArrayEquals(floatArrayOf(2f), ProjectGreaseSelect.setMaterial(2)!!.args, 0f)
        assertArrayEquals(floatArrayOf(1f), ProjectGreaseSelect.resetVertexColor(ProjectGreaseSelect.PAINT_FILL)!!.args, 0f)
        assertEquals(0, ProjectGreaseSelect.flip().args.size)
        assertArrayEquals(floatArrayOf(3f), ProjectGreaseSelect.cyclic(ProjectGreaseSelect.CYCLIC_TOGGLE)!!.args, 0f)
        assertArrayEquals(floatArrayOf(16f), ProjectGreaseSelect.snapToGrid(16f)!!.args, 0f)
    }

    @Test
    fun rejectsInvalidInput() {
        assertNull(ProjectGreaseSelect.arrange(4))
        assertNull(ProjectGreaseSelect.setMaterial(-1))
        assertNull(ProjectGreaseSelect.resetVertexColor(3))
        assertNull(ProjectGreaseSelect.cyclic(0))
        assertNull(ProjectGreaseSelect.snapToGrid(0f))
    }
}
