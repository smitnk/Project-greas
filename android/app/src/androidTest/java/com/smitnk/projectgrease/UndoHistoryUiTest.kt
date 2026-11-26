package com.smitnk.projectgrease

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.RigController
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Menu > Undo History (Blender's Edit > Undo History, ui/UndoHistory.kt): the steps are listed by
 * their operator names, tapping one loads that document, and redo then goes forward from it.
 */
@RunWith(AndroidJUnit4::class)
class UndoHistoryUiTest : SweepBase() {
    private fun openHistory() {
        rule.onNodeWithContentDescription("Menu").performClick(); rule.waitForIdle()
        rule.onNodeWithTag("menu_undoHistory").performScrollTo().performClick(); rule.waitForIdle()
    }

    @Test fun listsNamedStepsAndJumps() {
        onUi { controller.selectTool(GreaseTool.DRAW) }
        val i0 = onUi { controller.history.stepIndex }
        val docs = ArrayList<String>()
        repeat(3) { line(200f + it * 60f); docs += signature() }
        assertEquals(i0 + 3, onUi { controller.history.stepIndex })
        assertEquals("a drawn stroke is named like GPENCIL_OT_draw", "Grease Pencil Draw",
            onUi { controller.history.stepNames[i0 + 3] })

        openHistory()
        rule.onNodeWithTag("undoHistory").assertIsDisplayed()
        rule.onNodeWithTag("undoStep_${i0 + 3}").assertIsDisplayed()
        rule.onNodeWithTag("undoStep_${i0 + 1}").performClick(); rule.waitForIdle()
        assertEquals("the tapped step is loaded", docs[0], signature())
        assertEquals(i0 + 1, onUi { controller.history.stepIndex })
        assertTrue(onUi { controller.history.canRedo })
        assertTrue(onUi { controller.redo() })
        assertEquals("redo goes on from the jumped-to step", docs[1], signature())

        // jump forward again from the list
        openHistory()
        rule.onNodeWithTag("undoStep_${i0 + 3}").performClick(); rule.waitForIdle()
        assertEquals(docs[2], signature())
        assertTrue(!onUi { controller.history.canRedo })
    }

    /** Leaving armature Edit mode is one "Toggle Edit Mode" step; undo inside Edit mode undoes bones only. */
    @Test fun armatureEditModeStepsAreNamed() {
        val rig = controller.rig
        onUi { rig.create(); controller.history.markEdit() }
        assertEquals("Add Armature", onUi { controller.history.stepNames.last() })
        assertTrue(onUi { controller.setActiveObject(true) })
        assertTrue(onUi { controller.modeToggle("GPENCIL_OT_editmode_toggle") })
        assertEquals("Toggle Edit Mode", onUi { controller.history.stepNames.last() })
        onUi { rig.addBone(100f, 100f, 100f, 300f); controller.history.markEdit() }
        assertEquals("Add Bone", onUi { controller.history.stepNames.last() })
        val bones = onUi { rig.boneCount }
        assertTrue(onUi { controller.modeToggle("OBJECT_OT_mode_set") })
        assertEquals("Toggle Edit Mode", onUi { controller.history.stepNames.last() })
        assertTrue(onUi { controller.undo() })
        assertEquals("one undo is back in Edit mode with the bone", RigController.MODE_EDIT, onUi { rig.mode })
        assertEquals(bones, onUi { rig.boneCount })
        assertTrue(onUi { controller.undo() })
        assertEquals("undo in Edit mode removes the bone", bones - 1, onUi { rig.boneCount })
        openHistory()
        rule.onNodeWithText("Add Bone").assertIsDisplayed()
    }
}
