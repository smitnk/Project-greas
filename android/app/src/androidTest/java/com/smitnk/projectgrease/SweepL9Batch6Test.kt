package com.smitnk.projectgrease

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.nativebridge.GPNative
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Batch 6: optimisation. Each test checks the cause of a measured cost on the real renderer. */
@RunWith(AndroidJUnit4::class)
class SweepL9Batch6Test : SweepBase() {
    private fun frameInfo() = onUi { GPNative.nativeLastFrameInfo(0L) }

    /** The SLOW render lines: the presenter asked GL for the stencil bits once per stroke (a stall each). */
    @Test fun glQueriesDoNotGrowWithStrokes() {
        repeat(20) { line(80f + it * 30f) }
        tap(640f, 700f) // one more redraw of the whole document
        val info = frameInfo()
        val n = Regex("stroke GL queries (\\d+)").find(info)?.groupValues?.get(1)?.toInt()
        assertTrue("no query count in: $info", n != null)
        assertTrue("21 strokes cost $n GL queries in one frame: $info", n!! <= 4)
    }
}
