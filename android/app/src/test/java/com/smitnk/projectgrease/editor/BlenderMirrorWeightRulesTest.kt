package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderMirrorWeightRulesTest {
    @Test
    fun idsMatchTheNativeHeader() {
        assertEquals(56, ProjectGreaseSelect.CMD_MIRROR_COPY)
        assertEquals(57, ProjectGreaseSelect.CMD_WEIGHT_PAINT)
        assertEquals(9, ProjectGreaseSelect.EASE_BOUNCE)
        assertEquals(2, ProjectGreaseSelect.EASE_IN_OUT)
    }

    @Test
    fun packsArguments() {
        assertArrayEquals(floatArrayOf(1f, 0f, 50f, 60f), ProjectGreaseSelect.mirrorCopy(true, false, 50f, 60f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(2f, 1f, 2f, 20f, 0.5f, 0.75f),
            ProjectGreaseSelect.weightPaint(2, 1f, 2f, 20f, 0.5f, 0.75f)!!.args, 0f)
    }

    @Test
    fun rejectsInvalidInput() {
        assertNull(ProjectGreaseSelect.mirrorCopy(false, false, 0f, 0f))
        assertNull(ProjectGreaseSelect.mirrorCopy(true, false, Float.NaN, 0f))
        assertNull(ProjectGreaseSelect.weightPaint(-1, 0f, 0f, 10f, 1f, 1f))
        assertNull(ProjectGreaseSelect.weightPaint(0, 0f, 0f, 0f, 1f, 1f))
    }
}
