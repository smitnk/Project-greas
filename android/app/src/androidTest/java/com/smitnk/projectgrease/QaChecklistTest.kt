package com.smitnk.projectgrease

import android.media.MediaExtractor
import android.media.MediaFormat
import android.os.SystemClock
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ProjectSettings
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.math.abs
import kotlin.math.hypot

/**
 * The QA checklist from the emulator test brief, each item checked against real output (native
 * document, encoded file, playback position), never against the UI alone.
 */
class QaChecklistTest : SweepBase() {

    private fun twelveFrames() {
        // one stroke per frame at a different height, so every frame is distinguishable
        for (f in 1..12) {
            onUi { controller.createFrame(f); controller.selectFrame(f) }
            line(80f + f * 45f)
        }
        assertEquals(null, onUi {
            controller.applyProjectSettings(ProjectSettings().apply {
                width = controller.document.canvasWidth; height = controller.document.canvasHeight
                fps = 12; frameStart = 1; frameEnd = 12
            })
        })
    }

    @Test fun twelveFrameAnimationHasTwelveKeys() {
        twelveFrames()
        val keys = onUi { controller.frameNumbers() }.toList()
        assertEquals((1..12).toList(), keys)
        for (f in 1..12) {
            onUi { controller.selectFrame(f) }
            assertEquals("frame $f keeps exactly its own stroke", 1, onUi { controller.strokeCount() })
        }
    }

    @Test fun playbackVisitsEveryFrameInAboutOneSecond() {
        twelveFrames()
        onUi { controller.selectFrame(1) }
        val seen = LinkedHashSet<Int>()
        val t0 = SystemClock.uptimeMillis()
        onUi { controller.animation.togglePlayback() }
        while (SystemClock.uptimeMillis() - t0 < 1400) {
            seen += onUi { controller.animation.currentFrame }
            SystemClock.sleep(10)
        }
        onUi { if (controller.animation.playing) controller.animation.togglePlayback() }
        // frame dropping may skip a frame on a slow emulator, but playback must reach the end in ~1 s
        assertTrue("reached frame 12, saw $seen", 12 in seen)
        assertTrue("most frames shown, saw $seen", seen.size >= 9)
    }

    @Test fun videoExportIsTwelveFramesOneSecond() {
        twelveFrames()
        val file = File(rule.activity.cacheDir, "qa12.mp4")
        val frames = onUi { com.smitnk.projectgrease.ui.VideoExport.exportToFile(controller, file) }
        shell("cp ${file.absolutePath} $SHOT_DIR/qa12.mp4")
        assertEquals(12, frames)
        val ex = MediaExtractor()
        try {
            ex.setDataSource(file.absolutePath)
            val track = (0 until ex.trackCount).first { ex.getTrackFormat(it).getString(MediaFormat.KEY_MIME)!!.startsWith("video/") }
            ex.selectTrack(track)
            var samples = 0; var lastUs = 0L
            while (ex.sampleTime >= 0) { samples++; lastUs = ex.sampleTime; ex.advance() }
            assertEquals("encoded samples", 12, samples)
            // 12 frames at 12 fps: the last frame starts at 11/12 s
            assertTrue("last frame at ${lastUs}us", abs(lastUs - 916_666L) < 60_000)
        } finally { ex.release() }
        // each exported frame shows its own stroke: frame f has dark pixels on row 80+45f only
        val w = onUi { controller.document.canvasWidth }
        val rows = ArrayList<Int>()
        assertTrue(onUi { controller.renderExportFrames(false) { _, p ->
            rows += (1..12).first { f -> val y = (80 + f * 45); isDark(p[y * w + 600]) }
        } })
        assertEquals((1..12).toList(), rows)
    }

    @Test fun saveCloseReopenRestoresEveryFrame() {
        twelveFrames()
        val saved = onUi { controller.saveDocumentJson() }!!
        val before = signature()
        assertTrue(onUi { controller.resetDocument() })
        assertEquals(0, onUi { controller.strokeCount() })
        assertTrue(onUi { controller.loadDocumentJson(saved) })
        assertEquals(before, signature())
        assertEquals((1..12).toList(), onUi { controller.frameNumbers() }.toList())
    }

    @Test fun undoRedoAcrossFrames() {
        line(300f)
        onUi { controller.createFrame(2); controller.selectFrame(2) }
        val withOne = signature()
        line(500f)
        val withTwo = signature()
        assertTrue(onUi { controller.undo() })
        assertEquals(withOne, signature())
        assertTrue(onUi { controller.redo() })
        assertEquals(withTwo, signature())
        assertEquals("redo stays on frame 2", 2, onUi { controller.animation.currentFrame })
    }

    /** Drawing on a rotated canvas lands where the finger went, at 90, 180 and 270 degrees. */
    @Test fun drawingOnRotatedCanvasIsAccurate() {
        for (deg in listOf(90, 180, 270)) {
            onUi { controller.resetDocument(); controller.selectTool(GreaseTool.DRAW); controller.view.setRotation(Math.toRadians(deg.toDouble()).toFloat()) }
            rule.waitForIdle()
            drag(400f to 300f, 800f to 400f)
            val s = strokes().single()
            val p = points(s)
            fun near(x: Double, y: Double) = p.minOf { hypot(it[0] - x, it[1] - y) }
            assertTrue("start at $deg°: ${near(400.0, 300.0)}", near(400.0, 300.0) < 12)
            assertTrue("end at $deg°: ${near(800.0, 400.0)}", near(800.0, 400.0) < 12)
        }
        onUi { controller.view.setRotation(0f) }
    }

    /** Erasing on a rotated canvas removes the part under the finger only. */
    @Test fun erasingOnRotatedCanvasIsAccurate() {
        onUi { controller.view.setRotation(Math.toRadians(90.0).toFloat()) }
        line(400f, 200f, 1000f)
        onUi { controller.selectTool(GreaseTool.ERASE) }
        drag(600f to 330f, 600f to 470f)
        val pts = strokes().flatMap { points(it) }
        assertTrue("left part kept", pts.any { it[0] < 500 })
        assertTrue("right part kept", pts.any { it[0] > 700 })
        assertTrue("middle erased", pts.none { abs(it[0] - 600) < 5 && abs(it[1] - 400) < 5 })
        onUi { controller.view.setRotation(0f) }
    }

    /** Rapid repeated operations do not crash or leak strokes. */
    @Test fun rapidRepeatedUndoRedo() {
        repeat(5) { line(200f + it * 80f) }
        val full = signature()
        repeat(30) { onUi { controller.undo() } }
        assertEquals(0, onUi { controller.strokeCount() })
        repeat(30) { onUi { controller.redo() } }
        assertEquals(full, signature())
    }
}
