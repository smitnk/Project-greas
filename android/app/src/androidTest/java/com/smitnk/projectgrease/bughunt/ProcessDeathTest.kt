package com.smitnk.projectgrease.bughunt

import android.os.SystemClock
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.smitnk.projectgrease.MainActivity
import com.smitnk.projectgrease.SweepBase
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

/** Where the expected document is kept between the two runs (the test package's files survive). */
private fun expectedFile() = File(InstrumentationRegistry.getInstrumentation().context.filesDir, "process_death_expected.json")
private const val PROJECT = "ProcessDeath"
private fun signature(raw: String) = JSONObject(raw).also { it.remove("frame") }.toString()

/**
 * Process death, part 1: draw into a named project, send the app to the background and stop. The
 * bug-hunt workflow then kills the process (adb shell am kill) and runs [ProcessDeathVerifyTest].
 */
@BugHunt
@RunWith(AndroidJUnit4::class)
class ProcessDeathSetupTest : SweepBase() {
    @Test fun drawThenBackground() {
        onUi { controller.document.projectName = PROJECT }
        line(250f); line(450f); drag(300f to 600f, 700f to 700f, 1000f to 600f)
        val expected = onUi { controller.saveDocumentJson() }!!
        expectedFile().writeText(expected)
        shell("input keyevent KEYCODE_HOME")
        SystemClock.sleep(1000) // in the background (onStop), well inside the 5 s autosave period
    }
}

/** Process death, part 2: after the kill, the relaunched app must still have the drawing. */
@BugHunt
@RunWith(AndroidJUnit4::class)
class ProcessDeathVerifyTest {
    @get:Rule val rule = createAndroidComposeRule<MainActivity>()

    @Test fun documentSurvivesProcessDeath() {
        val expected = expectedFile().takeIf { it.exists() }?.readText()
        assertTrue("ProcessDeathSetupTest ran first", expected != null)
        rule.onNodeWithText(PROJECT).performClick()
        rule.waitUntil(15_000) { rule.activity.controller.rendererReady }
        rule.waitForIdle()
        SystemClock.sleep(500)
        val restored = InstrumentationRegistry.getInstrumentation().let { inst ->
            var out: String? = null
            inst.runOnMainSync { out = rule.activity.controller.saveDocumentJson() }
            out
        }
        assertEquals("document after process death", signature(expected!!), signature(restored!!))
    }
}
