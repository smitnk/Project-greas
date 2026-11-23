package com.smitnk.projectgrease

import android.os.SystemClock
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.EraserMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import com.smitnk.projectgrease.ui.VideoImportState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Batch 3: frame change closes open sessions, one history stack, eraser preview off the document, single decode thread, import state, busy counter. */
@RunWith(AndroidJUnit4::class)
class SweepL9Batch3Test : SweepBase() {

    /** B2: an open shape / curve / polyline belongs to the frame it was drawn on. */
    @Test fun frameChangeCancelsOpenSessions() {
        onUi { controller.createFrame(3) }
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }
        drag(300f to 200f, 900f to 500f)
        assertTrue(onUi { controller.shapeEditing })
        assertTrue(onUi { controller.animation.setFrame(3) })
        assertFalse("shape cancelled", onUi { controller.shapeEditing })
        assertEquals(0, strokes().size)

        onUi { controller.selectTool(GreaseTool.CURVE) }
        drag(300f to 400f, 900f to 400f)
        assertTrue(onUi { controller.curveEditing })
        assertTrue(onUi { controller.animation.setFrame(1) })
        assertFalse("curve cancelled", onUi { controller.curveEditing })

        onUi { controller.selectTool(GreaseTool.POLYLINE) }
        tap(300f, 300f); tap(600f, 200f)
        assertTrue(onUi { controller.polylineEditing })
        assertTrue(onUi { controller.animation.setFrame(3) })
        assertFalse("polyline cancelled", onUi { controller.polylineEditing })
        assertEquals(0, strokes().size)
    }

    /** B3: the Kotlin-side extras ride the same step as the native snapshot, through undo, redo and a new step after undo. */
    @Test fun historyExtrasFollowTheirStep() {
        var marker = 0; var restored = -1
        onUi { controller.history.captureExtras = { marker }; controller.history.restoreExtras = { restored = it as Int }; controller.history.reset() }
        marker = 1; line(200f); marker = 2; line(300f); marker = 3; line(400f)
        assertEquals(3, strokes().size)
        assertTrue(onUi { controller.undo() }); assertEquals(2, restored)
        assertTrue(onUi { controller.undo() }); assertEquals(1, restored)
        assertTrue(onUi { controller.redo() }); assertEquals(2, restored)
        marker = 4; line(500f) // drops the redo tail
        assertFalse(onUi { controller.redo() })
        assertTrue(onUi { controller.undo() }); assertEquals(2, restored)
        assertEquals(2, onUi { controller.history.stepIndex }); assertEquals(4, onUi { controller.history.stepCount })
    }

    /** B5: the eraser's live preview over a fill is never a document stroke (not saved, not in undo, not exported). */
    @Test fun eraserPreviewIsNotInTheDocument() {
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }
        drag(300f to 200f, 900f to 500f)
        assertTrue(onUi { controller.confirmShape() })
        assertTrue(onUi { controller.selectAll(); controller.fillSelectedStroke() })
        val strokes = strokes().size; val mats = onUi { controller.materialCount() }
        onUi { controller.selectTool(GreaseTool.ERASE); controller.setEraserMode(EraserMode.HARD); controller.setEraserSize(40f) }
        val a = screen(500f, 350f); val b = screen(700f, 350f)
        val down = SystemClock.uptimeMillis()
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_DOWN, down, a.first, a.second))
        for (i in 1..20) {
            SystemClock.sleep(15)
            val t = i / 20f
            instrumentation.sendPointerSync(event(MotionEvent.ACTION_MOVE, down, a.first + (b.first - a.first) * t, a.second))
        }
        assertEquals("no preview stroke while erasing", strokes, strokes().size)
        assertEquals("no eraser material while erasing", mats, onUi { controller.materialCount() })
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_UP, down, b.first, b.second))
        rule.waitForIdle()
        assertEquals("no eraser material after erasing", mats, onUi { controller.materialCount() })
    }

    /** B6: every decode, from any caller thread, runs on the one loader thread. */
    @Test fun rotoscopeDecodesOnOneThread() {
        val ctx = instrumentation.targetContext
        val file = java.io.File(ctx.cacheDir, "b3_roto.png")
        android.graphics.Bitmap.createBitmap(8, 8, android.graphics.Bitmap.Config.ARGB_8888).also { b ->
            file.outputStream().use { b.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, it) }
        }
        val loader = com.smitnk.projectgrease.ui.RotoscopeLoader(ctx)
        val r = com.smitnk.projectgrease.editor.Rotoscope().also { it.set(com.smitnk.projectgrease.editor.Rotoscope.Kind.IMAGE, listOf(android.net.Uri.fromFile(file).toString()), 1) }
        val threads = (1..4).map { Thread { repeat(5) { loader.clearCache(); loader.frame(r, 1) } }.apply { start() } }
        threads.forEach { it.join() }
        loader.decodeAsync(r, 1)
        assertEquals("decode threads ${loader.decodeThreads}", 1, loader.decodeThreads.size)
    }

    /** B8: the trim screen's state after the file is read. */
    @Test fun videoImportStates() {
        assertEquals(VideoImportState.READY, VideoImportState.afterLoad(120))
        assertEquals(VideoImportState.UNREADABLE, VideoImportState.afterLoad(0))
    }

    /** B9: overlapping expected-busy sections keep the watchdog quiet until the last one ends. */
    @Test fun expectedBusyIsACounter() {
        assertFalse(CrashLog.expectedBusy)
        CrashLog.expectBusy {
            CrashLog.expectBusy { assertTrue(CrashLog.expectedBusy) }
            assertTrue("still busy after the inner one ended", CrashLog.expectedBusy)
        }
        assertFalse(CrashLog.expectedBusy)
        runCatching { CrashLog.expectBusy { error("boom") } }
        assertFalse("an error path still decrements", CrashLog.expectedBusy)
    }
}
