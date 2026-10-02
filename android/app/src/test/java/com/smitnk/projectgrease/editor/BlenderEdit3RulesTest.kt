package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderEdit3RulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals((66..75).toList(), listOf(
            ProjectGreaseSelect.CMD_SELECT_RANDOM, ProjectGreaseSelect.CMD_BLANK_FRAME, ProjectGreaseSelect.CMD_FILL_COLOR,
            ProjectGreaseSelect.CMD_CLEAN_LOOSE, ProjectGreaseSelect.CMD_CLEAN_DUP_FRAMES, ProjectGreaseSelect.CMD_VCOLOR_SET,
            ProjectGreaseSelect.CMD_VCOLOR_INVERT, ProjectGreaseSelect.CMD_VCOLOR_BC, ProjectGreaseSelect.CMD_VCOLOR_HSV,
            ProjectGreaseSelect.CMD_VCOLOR_LEVELS))
    }

    @Test
    fun packsArguments() {
        assertArrayEquals(floatArrayOf(1f, 7f, 0f), ProjectGreaseSelect.selectRandom(2f, 7, false)!!.args, 0f)
        assertArrayEquals(floatArrayOf(12f), ProjectGreaseSelect.blankFrame(12)!!.args, 0f)
        assertArrayEquals(floatArrayOf(1f, 0.1f, 0.2f, 0.3f, 1f), ProjectGreaseSelect.fillColor(1, 0.1f, 0.2f, 0.3f, 1f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(2f), ProjectGreaseSelect.cleanLoose(2)!!.args, 0f)
        assertEquals(0, ProjectGreaseSelect.cleanDuplicateFrames().args.size)
        assertArrayEquals(floatArrayOf(2f, 0.1f, 0.2f), ProjectGreaseSelect.vcolorBrightnessContrast(2, 0.1f, 0.2f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(0f, 0.5f, 1f, 1f), ProjectGreaseSelect.vcolorHsv(0)!!.args, 0f)
    }

    @Test
    fun rejectsInvalidInput() {
        assertNull(ProjectGreaseSelect.selectRandom(0.5f, -1))
        assertNull(ProjectGreaseSelect.blankFrame(-1))
        assertNull(ProjectGreaseSelect.cleanLoose(0))
        assertNull(ProjectGreaseSelect.vcolorInvert(3))
        assertNull(ProjectGreaseSelect.vcolorSet(0, Float.NaN, 0f, 0f))
    }
}
