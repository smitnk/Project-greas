package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderEdit7RulesTest {
    @Test
    fun ids() {
        assertEquals((93..102).toList(), listOf(ProjectGreaseSelect.CMD_VG_ASSIGN, ProjectGreaseSelect.CMD_VG_REMOVE,
            ProjectGreaseSelect.CMD_VG_SELECT, ProjectGreaseSelect.CMD_VG_DESELECT, ProjectGreaseSelect.CMD_VG_INVERT,
            ProjectGreaseSelect.CMD_VG_NORMALIZE, ProjectGreaseSelect.CMD_LAYER_MERGE, ProjectGreaseSelect.CMD_LAYER_ISOLATE,
            ProjectGreaseSelect.CMD_LOCK_ALL, ProjectGreaseSelect.CMD_UNLOCK_ALL))
    }

    @Test
    fun packing() {
        assertArrayEquals(floatArrayOf(2f, 1f), ProjectGreaseSelect.vgAssign(2, 3f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(1f), ProjectGreaseSelect.vgOp(ProjectGreaseSelect.CMD_VG_INVERT, 1)!!.args, 0f)
        assertEquals(0, ProjectGreaseSelect.layerOp(ProjectGreaseSelect.CMD_LOCK_ALL)!!.args.size)
        assertNull(ProjectGreaseSelect.vgAssign(-1)); assertNull(ProjectGreaseSelect.vgOp(ProjectGreaseSelect.CMD_LOCK_ALL, 0))
        assertNull(ProjectGreaseSelect.layerOp(ProjectGreaseSelect.CMD_VG_ASSIGN))
    }
}
