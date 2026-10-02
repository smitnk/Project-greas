package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderEditRulesTest {

    @Test
    fun idsAndFlagsMatchTheNativeHeader() {
        assertEquals(listOf(1, 2, 4, 8, 16, 32), listOf(
            ProjectGreaseSelect.PICK_EXTEND, ProjectGreaseSelect.PICK_DESELECT, ProjectGreaseSelect.PICK_TOGGLE,
            ProjectGreaseSelect.PICK_ENTIRE, ProjectGreaseSelect.PICK_DESELECT_ALL, ProjectGreaseSelect.PICK_PASSTHROUGH
        ))
        assertEquals(listOf(31, 32, 33, 34, 35, 36, 37), listOf(
            ProjectGreaseSelect.CMD_PICK, ProjectGreaseSelect.CMD_TRANSLATE, ProjectGreaseSelect.CMD_ROTATE,
            ProjectGreaseSelect.CMD_SCALE, ProjectGreaseSelect.CMD_MIRROR,
            ProjectGreaseSelect.CMD_DELETE_STROKES, ProjectGreaseSelect.CMD_DELETE_POINTS
        ))
    }

    @Test
    fun pickRadiusFollowsBlendersWidgetUnitRule() {
        // radius = 0.4 * widget_unit(20) = 8; radius_squared = (int)(8 * 8) = 64
        assertEquals(64, ProjectGreaseSelect.pickRadiusSquared(1f))
        // zooming in shrinks the radius in canvas units: (8 / 2)^2 = 16
        assertEquals(16, ProjectGreaseSelect.pickRadiusSquared(2f))
        // the zoom is clamped at 0.1: (8 / 0.1)^2 = 6400
        assertEquals(6400, ProjectGreaseSelect.pickRadiusSquared(0.01f))
    }

    @Test
    fun pickPacksPositionRadiusFlagsAndMode() {
        val command = ProjectGreaseSelect.pick(
            12f, 34f, 64,
            ProjectGreaseSelect.PICK_ENTIRE or ProjectGreaseSelect.PICK_PASSTHROUGH,
            ProjectGreaseSelect.MODE_POINT
        )
        assertNotNull(command)
        assertEquals(ProjectGreaseSelect.CMD_PICK, command!!.id)
        assertArrayEquals(floatArrayOf(12f, 34f, 64f, 40f, 0f), command.args, 0f)
        assertNull(ProjectGreaseSelect.pick(Float.NaN, 0f, 64, 0, 0))
        assertNull(ProjectGreaseSelect.pick(0f, 0f, -1, 0, 0))
        assertNull(ProjectGreaseSelect.pick(0f, 0f, 64, 0, 9))
    }

    @Test
    fun transformsPackTheirArgumentsWithAnOptionalPivot() {
        assertArrayEquals(floatArrayOf(3f, -4f), ProjectGreaseSelect.translate(3f, -4f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(0.5f), ProjectGreaseSelect.rotate(0.5f)!!.args, 0f)
        assertArrayEquals(floatArrayOf(0.5f, 10f, 20f), ProjectGreaseSelect.rotate(0.5f, floatArrayOf(10f, 20f))!!.args, 0f)
        assertArrayEquals(floatArrayOf(2f, 3f, 1f, 2f), ProjectGreaseSelect.scale(2f, 3f, floatArrayOf(1f, 2f))!!.args, 0f)
        assertArrayEquals(floatArrayOf(1f, 0f), ProjectGreaseSelect.mirror(true, false)!!.args, 0f)
        assertArrayEquals(floatArrayOf(0f, 1f, 5f, 6f), ProjectGreaseSelect.mirror(false, true, floatArrayOf(5f, 6f))!!.args, 0f)
        assertEquals(ProjectGreaseSelect.CMD_MIRROR, ProjectGreaseSelect.mirror(true, true)!!.id)
    }

    @Test
    fun degenerateTransformsAreRejectedBeforeReachingNative() {
        assertNull(ProjectGreaseSelect.translate(Float.NaN, 0f))
        assertNull(ProjectGreaseSelect.rotate(Float.POSITIVE_INFINITY))
        assertNull(ProjectGreaseSelect.scale(0f, 1f))
        assertNull(ProjectGreaseSelect.mirror(false, false))
        assertNull(ProjectGreaseSelect.rotate(1f, floatArrayOf(Float.NaN, 0f)))
        assertNull(ProjectGreaseSelect.rotate(1f, floatArrayOf(1f)))
    }

    @Test
    fun deleteCommandsCarryNoArguments() {
        assertEquals(0, ProjectGreaseSelect.deleteStrokes().args.size)
        assertEquals(ProjectGreaseSelect.CMD_DELETE_POINTS, ProjectGreaseSelect.deletePoints().id)
    }
}
