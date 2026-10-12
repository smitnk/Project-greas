package com.smitnk.projectgrease

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseTool
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** The Studio layout: floating toolbar, option popup on a second tap, layer panel, frame strip. */
@RunWith(AndroidJUnit4::class)
class DeviceStudioTest : SweepBase() {
    @org.junit.Before fun studio() {
        onUi { controller.classicLayout = false }
        rule.waitForIdle()
        rule.waitUntil(15_000) { onUi { controller.rendererReady } }
    }

    @Test fun toolbarSelectsToolsAndOpensOptions() {
        rule.onNodeWithTag("studioToolbar").assertIsDisplayed()
        rule.onNodeWithTag("studioTool_Eraser").performClick()
        assertEquals(GreaseTool.ERASE, onUi { controller.tools.activeTool })
        rule.onNodeWithTag("studioTool_Eraser").performClick()
        rule.onNodeWithTag("studioPopup").assertIsDisplayed()
        shot("studio_eraser_options")
    }

    @Test fun canvasFillsTheScreen() {
        val canvas = rule.onNodeWithTag("studioCanvas").fetchSemanticsNode().boundsInWindow
        val root = rule.activity.window.decorView
        assertTrue("canvas ${canvas.width}x${canvas.height}", canvas.width >= root.width * 0.95f && canvas.height >= root.height * 0.8f)
    }

    @Test fun layerPanelShowsActiveLayerAndAdds() {
        rule.onNodeWithTag("studioTool_Layers").performClick()
        rule.onNodeWithTag("studioLayers").assertIsDisplayed()
        val before = onUi { controller.layerCount() }
        rule.onNodeWithTag("layerAdd").performClick()
        assertEquals(before + 1, onUi { controller.layerCount() })
        rule.onNodeWithTag("layerRow_0").performClick()
        assertEquals(0, onUi { controller.selectedLayer })
        shot("studio_layers")
    }

    @Test fun frameStripAddsAndSelectsFrame() {
        val f = onUi { controller.animation.currentFrame }
        rule.onNodeWithTag("stripAdd").performClick()
        assertEquals(f + 1, onUi { controller.animation.currentFrame })
        shot("studio_frames")
    }

    @Test fun colourPopupSwitchesColourMode() {
        rule.onNodeWithTag("studioColor").performClick()
        rule.onNodeWithTag("colorModeVertex").performClick()
        assertTrue(onUi { controller.vertexColorMode })
    }
}
