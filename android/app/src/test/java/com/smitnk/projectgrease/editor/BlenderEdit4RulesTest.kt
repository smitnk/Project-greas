package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderEdit4RulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals((76..85).toList(), listOf(
            ProjectGreaseSelect.CMD_DASH, ProjectGreaseSelect.CMD_MULTIPLY, ProjectGreaseSelect.CMD_ARRAY,
            ProjectGreaseSelect.CMD_MERGE_DISTANCE, ProjectGreaseSelect.CMD_CAPS, ProjectGreaseSelect.CMD_START_SET,
            ProjectGreaseSelect.CMD_SEPARATE_LAYER, ProjectGreaseSelect.CMD_MOVE_TO_LAYER, ProjectGreaseSelect.CMD_COPY,
            ProjectGreaseSelect.CMD_PASTE))
    }

    @Test
    fun packsArguments() {
        assertArrayEquals(floatArrayOf(3f, 2f, 1f), ProjectGreaseSelect.dash(3, 2, 1)!!.args, 0f)
        assertArrayEquals(floatArrayOf(2f, 8f), ProjectGreaseSelect.multiply(2, 8f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(3f, 60f, 0f), ProjectGreaseSelect.array(3, 60f, 0f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(2f, 0f), ProjectGreaseSelect.mergeByDistance(2f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(3f), ProjectGreaseSelect.caps(ProjectGreaseSelect.CAPS_DEFAULT)!!.args, 0f)
        assertArrayEquals(floatArrayOf(1f), ProjectGreaseSelect.moveToLayer(1)!!.args, 0f)
    }

    @Test
    fun rejectsInvalidInput() {
        assertNull(ProjectGreaseSelect.dash(0, 2)); assertNull(ProjectGreaseSelect.dash(3, 0))
        assertNull(ProjectGreaseSelect.multiply(0, 1f)); assertNull(ProjectGreaseSelect.array(1, 1f, 0f))
        assertNull(ProjectGreaseSelect.mergeByDistance(0f)); assertNull(ProjectGreaseSelect.caps(4))
        assertNull(ProjectGreaseSelect.moveToLayer(-1))
    }
}
