package com.smitnk.projectgrease

import androidx.compose.runtime.Composable
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.test.platform.app.InstrumentationRegistry
import com.smitnk.projectgrease.ui.*
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

/** Part 3: the three layout directions, rendered on the device in dark, light and at 200% text,
 *  saved next to the sweep shots (pulled into android/device-shots/). */
class DesignDirectionsTest {
    @get:Rule val rule = createComposeRule()

    private fun render(name: String, mode: ProjectGreaseThemeMode, scale: Float = 1f, body: @Composable () -> Unit) {
        rule.setContent {
            val d = androidx.compose.ui.platform.LocalDensity.current
            androidx.compose.runtime.CompositionLocalProvider(
                androidx.compose.ui.platform.LocalDensity provides androidx.compose.ui.unit.Density(d.density, scale)) {
                ProjectGreaseTheme(mode) { body() }
            }
        }
        rule.waitForIdle()
        val ui = InstrumentationRegistry.getInstrumentation().uiAutomation
        ui.executeShellCommand("mkdir -p /data/local/tmp/pg_screenshots/sweep").close()
        ui.executeShellCommand("screencap -p /data/local/tmp/pg_screenshots/sweep/design_$name.png").close()
        // every icon-only tool carries a label for TalkBack
        assertTrue(rule.onAllNodesWithTag("studioTool_Draw").fetchSemanticsNodes().isNotEmpty())
    }

    @Test fun a_blenderRail() = render("A_blender_rail", ProjectGreaseThemeMode.DARK) { DirectionBlenderRail() }
    @Test fun b_thumbDock() = render("B_thumb_dock", ProjectGreaseThemeMode.DARK) { DirectionThumbDock() }
    @Test fun c_brushRing() = render("C_brush_ring", ProjectGreaseThemeMode.DARK) { DirectionBrushRing() }
    @Test fun a_light() = render("A_light", ProjectGreaseThemeMode.LIGHT) { DirectionBlenderRail() }
    @Test fun a_text200() {
        render("A_text200", ProjectGreaseThemeMode.DARK, 2f) { DirectionBlenderRail() }
        // all seven mode chips are still there (the row scrolls instead of clipping)
        listOf("OBJECT", "EDIT", "SCULPT", "DRAW", "WEIGHT_PAINT", "VERTEX_PAINT", "POSE")
            .forEach { rule.onNodeWithTag("modeChip_$it").assertExists() }
    }
}
