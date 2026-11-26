package com.smitnk.projectgrease

import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Axis constraints (transform_constraints.c) and numeric input (numinput.c) of the stroke transform,
 *  through the Move tool's lock chips and Type value keypad. Host parity: test_transform_blender_parity.cc. */
@RunWith(AndroidJUnit4::class)
class TransformConstraintTest : SweepBase() {

    private fun firstPoint() = points(strokes()[0])[0]
    private fun click(tag: String) { rule.onNodeWithTag(tag).performClick(); rule.waitForIdle() }

    private fun moveTool() {
        line(300f)
        onUi { controller.setMode(GreaseMode.EDIT); controller.setSelectMode(S.MODE_STROKE); controller.selectAll()
               controller.selectTool(GreaseTool.MOVE) }
        rule.waitForIdle()
    }

    /** X chip, Type value, "12.5", Confirm: the selection moves exactly 12.5 on X, one undo step. */
    @Test fun lockXAndTypeExactValue() {
        moveTool()
        val p0 = firstPoint()
        click("axisLock_X")
        assertEquals(1, onUi { controller.transformAxes })
        undoableGesture("typed X move") {
            click("typeValue")
            "12.5".forEach { click("numKey_$it") }
            assertTrue(onUi { controller.transformOpen })
            assertTrue(onUi { controller.transformNumText }!!.contains("12.5"))
            click("numConfirm")
        }
        assertFalse(onUi { controller.transformOpen })
        assertEquals(p0[0] + 12.5, firstPoint()[0], 1e-3)
        assertEquals(p0[1], firstPoint()[1], 1e-3)
        onUi { controller.setTransformConstraint(0) }
    }

    /** Y lock: a diagonal drag moves along Y only; '-' negates a typed value. */
    @Test fun lockYDragAndNegate() {
        moveTool()
        val p0 = firstPoint()
        onUi { controller.setTransformConstraint(2) }
        undoableGesture("Y locked drag") { drag(600f to 300f, 650f to 360f) }
        assertEquals(p0[0], firstPoint()[0], 0.01)
        assertEquals(p0[1] + 60.0, firstPoint()[1], 3.0)
        val p1 = firstPoint()
        onUi { assertTrue(controller.transformNumKey('7'.code)); assertTrue(controller.transformNumKey('-'.code)); controller.endTransform() }
        assertEquals(p1[0], firstPoint()[0], 1e-3)
        assertEquals(p1[1] - 7.0, firstPoint()[1], 1e-3)
        onUi { controller.setTransformConstraint(0) }
    }
}
