package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderStructureOpsRulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals(listOf(51, 52, 53, 54), listOf(ProjectGreaseSelect.CMD_DUPLICATE, ProjectGreaseSelect.CMD_DISSOLVE,
            ProjectGreaseSelect.CMD_SPLIT, ProjectGreaseSelect.CMD_JOIN))
        assertEquals(listOf(0, 1, 2), listOf(ProjectGreaseSelect.DISSOLVE_POINTS, ProjectGreaseSelect.DISSOLVE_BETWEEN,
            ProjectGreaseSelect.DISSOLVE_UNSELECT))
    }

    @Test
    fun packsArguments() {
        assertEquals(0, ProjectGreaseSelect.duplicate().args.size)
        assertArrayEquals(floatArrayOf(1f), ProjectGreaseSelect.dissolve(ProjectGreaseSelect.DISSOLVE_BETWEEN)!!.args, 0f)
        assertEquals(0, ProjectGreaseSelect.split().args.size)
        assertArrayEquals(floatArrayOf(1f), ProjectGreaseSelect.join(leaveGaps = true).args, 0f)
        assertNull(ProjectGreaseSelect.dissolve(3))
    }
}
