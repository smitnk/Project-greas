package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class BlenderSelectRulesTest {

    @Test
    fun valuesMatchBlenderEnums() {
        // ED_select_utils.h
        assertEquals(0, ProjectGreaseSelect.ACTION_TOGGLE)
        assertEquals(1, ProjectGreaseSelect.ACTION_SELECT)
        assertEquals(2, ProjectGreaseSelect.ACTION_DESELECT)
        assertEquals(3, ProjectGreaseSelect.ACTION_INVERT)
        assertEquals(1, ProjectGreaseSelect.OP_ADD)
        assertEquals(2, ProjectGreaseSelect.OP_SUB)
        assertEquals(3, ProjectGreaseSelect.OP_SET)
        assertEquals(4, ProjectGreaseSelect.OP_AND)
        assertEquals(5, ProjectGreaseSelect.OP_XOR)
    }

    @Test
    fun opModalOnlyExtendsLaterSamplesOfSet() {
        assertEquals(ProjectGreaseSelect.OP_SET, ProjectGreaseSelect.opModal(ProjectGreaseSelect.OP_SET, true))
        assertEquals(ProjectGreaseSelect.OP_ADD, ProjectGreaseSelect.opModal(ProjectGreaseSelect.OP_SET, false))
        assertEquals(ProjectGreaseSelect.OP_SUB, ProjectGreaseSelect.opModal(ProjectGreaseSelect.OP_SUB, false))
    }

    @Test
    fun lassoPacksOpModeThenPoints() {
        val command = ProjectGreaseSelect.lasso(
            ProjectGreaseSelect.OP_ADD,
            ProjectGreaseSelect.MODE_STROKE,
            listOf(1f to 2f, 3f to 4f, 5f to 6f)
        )
        assertNotNull(command)
        assertEquals(ProjectGreaseSelect.CMD_LASSO, command!!.id)
        assertArrayEquals(floatArrayOf(1f, 1f, 1f, 2f, 3f, 4f, 5f, 6f), command.args, 0f)

        assertNull(ProjectGreaseSelect.lasso(
            ProjectGreaseSelect.OP_SET, ProjectGreaseSelect.MODE_SEGMENT,
            listOf(1f to 2f, 3f to 4f, 5f to 6f)
        ))
        assertEquals(ProjectGreaseSelect.MODE_POINT, ProjectGreaseSelect.areaMode(ProjectGreaseSelect.MODE_SEGMENT))
    }

    @Test
    fun lassoRejectsBadInput() {
        val triangle = listOf(0f to 0f, 1f to 0f, 0f to 1f)
        assertNull(ProjectGreaseSelect.lasso(ProjectGreaseSelect.OP_SET, ProjectGreaseSelect.MODE_POINT, triangle.take(2)))
        assertNull(ProjectGreaseSelect.lasso(0, ProjectGreaseSelect.MODE_POINT, triangle))
        assertNull(ProjectGreaseSelect.lasso(ProjectGreaseSelect.OP_SET, ProjectGreaseSelect.MODE_SEGMENT, triangle))
        assertNull(
            ProjectGreaseSelect.lasso(
                ProjectGreaseSelect.OP_SET, ProjectGreaseSelect.MODE_POINT,
                listOf(0f to 0f, Float.NaN to 0f, 0f to 1f)
            )
        )
    }

    @Test
    fun boxAndCirclePackExpectedLayouts() {
        val box = ProjectGreaseSelect.box(ProjectGreaseSelect.OP_SET, ProjectGreaseSelect.MODE_POINT, 1f, 2f, 3f, 4f)!!
        assertEquals(ProjectGreaseSelect.CMD_BOX, box.id)
        assertArrayEquals(floatArrayOf(3f, 0f, 1f, 2f, 3f, 4f), box.args, 0f)

        val circle = ProjectGreaseSelect.circle(
            ProjectGreaseSelect.OP_SUB, ProjectGreaseSelect.MODE_STROKE, 10f, 20f, 5f, true
        )!!
        assertEquals(ProjectGreaseSelect.CMD_CIRCLE, circle.id)
        assertArrayEquals(floatArrayOf(2f, 1f, 10f, 20f, 5f, 1f), circle.args, 0f)
        assertEquals(0f, ProjectGreaseSelect.circle(1, 0, 0f, 0f, 1f, false)!!.args[5], 0f)
        assertNull(ProjectGreaseSelect.circle(1, 0, 0f, 0f, -1f, true))
    }

    @Test
    fun simpleCommandsCarryTheirFlags() {
        assertArrayEquals(floatArrayOf(1f, 0f), ProjectGreaseSelect.first(true, false).args, 0f)
        assertArrayEquals(floatArrayOf(0f, 1f), ProjectGreaseSelect.last(false, true).args, 0f)
        assertArrayEquals(floatArrayOf(1f), ProjectGreaseSelect.alternate(true).args, 0f)
        assertEquals(ProjectGreaseSelect.CMD_MORE, ProjectGreaseSelect.more().id)
        assertEquals(ProjectGreaseSelect.CMD_LESS, ProjectGreaseSelect.less().id)
        assertNull(ProjectGreaseSelect.all(4))
        assertNull(ProjectGreaseSelect.grouped(2))
        assertNotNull(ProjectGreaseSelect.grouped(ProjectGreaseSelect.GROUP_MATERIAL))
    }
}
