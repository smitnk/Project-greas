package com.smitnk.projectgrease.bughunt

import android.os.Build
import android.os.SystemClock
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.SweepBase
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Frame-time budget (the bug-hunt workflow records a Perfetto trace around this class): drawing on a
 * 200-stroke frame and animation playback. Budget 16 ms median / 32 ms p95, enforced on API 34; other
 * API levels report their numbers.
 */
@BugHunt
@RunWith(AndroidJUnit4::class)
class PerfTest : SweepBase() {
    private fun p95(v: List<Double>) = v.sorted()[(v.size * 95 / 100).coerceAtMost(v.size - 1)]
    private fun budget(name: String, times: List<Double>) {
        val med = median(times); val p = p95(times)
        Log.i(TAG, "PERF %s api=%d median=%.2f p95=%.2f max=%.2f n=%d".format(name, Build.VERSION.SDK_INT, med, p, times.maxOrNull() ?: 0.0, times.size))
        if (Build.VERSION.SDK_INT >= 34) {
            assertTrue("$name median $med ms <= 16", med <= 16.0)
            assertTrue("$name p95 $p ms <= 32", p <= 32.0)
        }
    }

    @Test fun drawingOn200Strokes() {
        repeat(200) { i -> val y = 50f + (i % 50) * 18f; val x0 = 50f + (i / 50) * 300f; drag(x0 to y, x0 + 240f to y, steps = 3) }
        // per input sample: the time the surface takes to handle it, redraw included
        val times = timedStroke(400, 600f)
        shot("perf_draw_200")
        budget("draw_200_strokes", times)
    }

    @Test fun playback() {
        // 24 frames of 10 strokes each, then every frame shown in turn (what playback does per tick)
        for (f in 1..24) {
            onUi { if (f > 1) controller.createFrame(f); controller.selectFrame(f) }
            repeat(10) { i -> val y = 80f + i * 70f + f; drag(150f to y, 600f + f * 10f to y + 30f, 1000f to y, steps = 4) }
        }
        val times = ArrayList<Double>()
        repeat(3) {
            for (f in 1..24) times += onUi { val t0 = System.nanoTime(); controller.selectFrame(f); controller.render(); (System.nanoTime() - t0) / 1e6 }
        }
        SystemClock.sleep(200)
        shot("perf_playback")
        budget("playback", times)
    }
}
