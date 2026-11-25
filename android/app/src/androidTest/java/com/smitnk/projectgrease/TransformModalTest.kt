package com.smitnk.projectgrease

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** The stroke transform modal (Blender's transform, native project_grease_transform.c): the total
 *  value applies from the initial points, increment snapping snaps the total, cancel restores, and
 *  the Move tool drag is one undo step. Host parity with Blender: test_transform_blender_parity.cc. */
@RunWith(AndroidJUnit4::class)
class TransformModalTest : SweepBase() {

    private fun firstPoint() = points(strokes()[0])[0]

    @Test fun totalValueSnapAndCancel() {
        line(300f)
        onUi { controller.setMode(GreaseMode.EDIT); controller.setSelectMode(S.MODE_STROKE); controller.selectAll() }
        val p0 = firstPoint()
        val c0 = onUi { controller.selectionPivot() }!!
        assertTrue(onUi { controller.beginTransform(S.XFORM_TRANSLATE) })
        onUi { controller.updateTransform(10f, 0f); controller.updateTransform(30f, 5f) }
        // Read the median, not the saved document: saving ends every open modal (cancelActiveGestures).
        val c1 = onUi { controller.selectionPivot() }!!
        assertEquals("the total, not the sum of updates", c0[0] + 30.0, c1[0].toDouble(), 0.5)
        assertEquals("the total, not the sum of updates", c0[1] + 5.0, c1[1].toDouble(), 0.5)
        assertTrue(onUi { controller.endTransform(cancel = true) })
        assertEquals("cancel restores", p0[0], firstPoint()[0], 0.01)

        onUi { controller.setSnapIncrement(25f) }
        undoable("snapped move") { controller.translateSelectedStroke(37f, 11f) }
        val p2 = firstPoint()
        assertEquals("snapped to the 25 grid", p0[0] + 25.0, p2[0], 0.5)
        assertEquals("snapped to the 25 grid", p0[1] + 0.0, p2[1], 0.5)
        onUi { controller.setSnapIncrement(0f) }
        assertFalse(onUi { controller.transformOpen })
    }

    @Test fun moveToolDragIsOneUndoStep() {
        line(300f)
        onUi { controller.setMode(GreaseMode.EDIT); controller.setSelectMode(S.MODE_STROKE); controller.selectAll()
               controller.selectTool(GreaseTool.MOVE) }
        val p0 = firstPoint()
        undoableGesture("move drag") { drag(600f to 300f, 650f to 360f) }
        val p1 = firstPoint()
        assertEquals(p0[0] + 50.0, p1[0], 3.0)
        assertEquals(p0[1] + 60.0, p1[1], 3.0)
        assertFalse(onUi { controller.transformOpen })
    }

    /** Blender's Move tool keymap: press on an unselected stroke runs gpencil.select (deselect_all),
     *  the drag then runs transform.translate with release_confirm on it: one undo step. */
    @Test fun moveToolTweaksUnselectedStroke() {
        line(300f); line(500f)
        onUi { controller.setMode(GreaseMode.EDIT); controller.deselectAll(); controller.selectTool(GreaseTool.MOVE) }
        val other = points(strokes()[1])[0]
        val p0 = firstPoint()
        undoableGesture("tweak move") { drag(600f to 300f, 650f to 360f) }
        assertEquals(p0[0] + 50.0, firstPoint()[0], 3.0)
        assertEquals(p0[1] + 60.0, firstPoint()[1], 3.0)
        assertEquals("the other stroke stays", other[1], points(strokes()[1])[0][1], 0.01)
        assertFalse(onUi { controller.transformOpen })
    }
}
