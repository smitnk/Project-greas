package com.smitnk.projectgrease

import androidx.compose.ui.test.assertHasClickAction
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import org.junit.Test

/**
 * Part 3 inventory: every screen, button and dialog the studio had before the redesign must still
 * be reachable. Inventory (ProjectGreaseUI.kt StudioLayout / StudioMenu, Batch21UI.kt and
 * SelectionActions.kt panels open from the Options card / Edit actions / Layers sheets):
 *  top bar: Menu, Mode, Fit canvas, All tools;
 *  rail: hide/show, Brush, Eraser, Lasso, Fill, Color, Brush size (new), Layers, Perspective,
 *        Undo, Redo, Copy, Paste, Options;
 *  popups: Options, Color, Mode (now chips incl. Object/Pose), Brush quick panel (new);
 *  timeline: strip, full timeline (was a sheet, now drag-up);
 *  menu: every item below (each opens its sheet / picker / dialog).
 */
class StudioReachabilityTest : SweepBase() {
    private val menuItems = listOf("save", "project", "tools", "materials", "draw", "advanced", "layers", "effects",
        "onion", "timeline", "edit", "importPhoto", "importVideo", "importAudio", "rotoscope", "audio", "settings", "crash", "activity", "rigGuide")
    // pickers, leaving the editor, and dialogs nested in the menu: presence only (one Back must close what opened)
    private val noClick = setOf("save", "importPhoto", "importVideo", "importAudio", "settings", "crash", "activity")

    private fun back() { instrumentation.sendKeyDownUpSync(android.view.KeyEvent.KEYCODE_BACK); rule.waitForIdle() }

    @Test fun railAndTopBarReachable() {
        listOf("Menu", "Mode", "Fit canvas", "All tools", "Hide tools", "Undo", "Redo", "Copy", "Paste")
            .forEach { rule.onNodeWithContentDescription(it).assertExists() }
        listOf("Brush", "Eraser", "Lasso", "Fill", "Brush size", "Layers", "Rig", "Perspective", "Options")
            .forEach { rule.onNodeWithTag("studioTool_$it").performScrollTo().assertHasClickAction() }
        rule.onNodeWithTag("studioColor").performScrollTo().assertHasClickAction()
        rule.onNodeWithTag("toolbarHide").performScrollTo().performClick(); rule.onNodeWithTag("toolbarShow").assertExists()
    }

    @Test fun popupsReachable() {
        rule.onNodeWithTag("studioTool_Options").performScrollTo().performClick(); rule.onNodeWithTag("studioPopup").assertExists(); back()
        rule.onNodeWithTag("studioColor").performScrollTo().performClick(); rule.onNodeWithTag("colorModeMaterial").assertExists(); back()
        rule.onNodeWithTag("studioTool_Brush size").performScrollTo().performClick(); rule.onNodeWithTag("brushQuick").assertExists()
        rule.onNodeWithTag("brushQuickColor").performClick(); rule.onNodeWithTag("colorModeMaterial").assertExists(); back()
        rule.onNodeWithTag("studioMode").performClick()
        listOf("OBJECT", "EDIT", "SCULPT", "DRAW", "WEIGHT_PAINT", "VERTEX_PAINT", "POSE")
            .forEach { rule.onNodeWithTag("modeChip_$it").assertExists() }
        rule.onNodeWithTag("modeChip_EDIT").performClick()
        rule.waitUntil(5_000) { onUi { controller.mode.name == "EDIT" } }
        shot("StudioReachability_modes")
    }

    @Test fun rigDiscoverable() {
        rule.onNodeWithTag("studioTool_Rig").performScrollTo().performClick(); rule.onNodeWithTag("rigSheet").assertExists()
        rule.onNodeWithTag("rigHelp").performClick(); rule.onNodeWithTag("rigGuide").assertExists()
        rule.onNodeWithTag("rigGuideClose").performClick(); back()
        rule.onNodeWithTag("studioMode").performClick(); rule.onNodeWithTag("modeChip_POSE").performScrollTo().performClick() // last chip: clipped in the scrolling row
        rule.onNodeWithTag("needRig").assertExists(); rule.onNodeWithTag("needRigAdd").performClick()
        rule.waitUntil(5_000) { onUi { controller.rig.exists } }
        rule.onNodeWithTag("rigSheet").assertExists()
    }

    @Test fun timelineDragsToFull() {
        rule.onNodeWithTag("studioFrames").assertExists()
        rule.onNodeWithTag("timelineHandle").performClick()
        rule.onNodeWithTag("timelineFull").assertExists()
        shot("StudioReachability_timelineFull")
        rule.onNodeWithTag("timelineHandle").performClick()
    }

    @Test fun everyMenuItemReachable() {
        menuItems.forEach { item ->
            rule.onNodeWithContentDescription("Menu").performClick()
            rule.onNodeWithTag("menu_$item").performScrollTo().assertHasClickAction()
            if (item !in noClick) { rule.onNodeWithTag("menu_$item").performClick(); rule.waitForIdle() }
            back(); rule.waitForIdle()
        }
    }
}
