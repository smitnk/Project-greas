package com.smitnk.projectgrease.bughunt

import android.os.Debug
import android.os.SystemClock
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.smitnk.projectgrease.SweepBase
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Long soak (default 30 min, instrumentation argument soakMinutes): scripted drawing, editing,
 * undo / redo, frames and playback in a loop, with the document reset to the same baseline every few
 * cycles so its own size stays bounded. Memory (PSS, native and Java heap) is sampled every 30 s
 * ("SOAK" lines, plotted by the bug-hunt workflow) and must not keep growing.
 */
@BugHunt
@RunWith(AndroidJUnit4::class)
class SoakTest : SweepBase() {
    private val minutes = (InstrumentationRegistry.getArguments().getString("soakMinutes") ?: "30").toDouble()

    private class Sample(val t: Double, val pssKb: Long, val nativeKb: Long, val javaKb: Long)

    private fun sample(t0: Long): Sample {
        Runtime.getRuntime().gc(); System.runFinalization(); Runtime.getRuntime().gc()
        val rt = Runtime.getRuntime()
        return Sample((SystemClock.uptimeMillis() - t0) / 60000.0, Debug.getPss(), Debug.getNativeHeapAllocatedSize() / 1024,
            (rt.totalMemory() - rt.freeMemory()) / 1024)
    }

    /** Least-squares slope (KB per minute) of the samples. */
    private fun slope(s: List<Sample>, value: (Sample) -> Long): Double {
        if (s.size < 2) return 0.0
        val mx = s.map { it.t }.average(); val my = s.map { value(it).toDouble() }.average()
        val num = s.sumOf { (it.t - mx) * (value(it) - my) }; val den = s.sumOf { (it.t - mx) * (it.t - mx) }
        return if (den == 0.0) 0.0 else num / den
    }

    @Test fun soak() {
        repeat(3) { line(200f + it * 150f) }
        val baseline = onUi { controller.saveDocumentJson() }!!
        val t0 = SystemClock.uptimeMillis()
        val end = t0 + (minutes * 60_000).toLong()
        val samples = mutableListOf(sample(t0))
        var nextSample = t0 + 30_000
        var cycle = 0
        while (SystemClock.uptimeMillis() < end) {
            val y = 150f + (cycle % 6) * 110f
            drag(150f to y, 500f to y + 40f, 900f to y - 20f, 1050f to y)
            onUi { controller.selectAll(); controller.translateSelectedStroke(3f, 2f) }
            onUi { controller.undo(); controller.undo(); controller.redo() }
            onUi { controller.createFrame(2 + cycle % 4); controller.selectFrame(1 + cycle % 4) }
            onUi { controller.animation.togglePlayback() }
            SystemClock.sleep(400)
            onUi { if (controller.animation.playing) controller.animation.togglePlayback(); controller.selectFrame(1) }
            if (cycle % 8 == 7) onUi { controller.loadDocumentJson(baseline) }
            cycle++
            if (SystemClock.uptimeMillis() >= nextSample) {
                val s = sample(t0); samples += s; nextSample += 30_000
                Log.i(TAG, "SOAK t_min=%.2f pss_kb=%d native_kb=%d java_kb=%d cycles=%d".format(s.t, s.pssKb, s.nativeKb, s.javaKb, cycle))
            }
        }
        shot("soak_end")
        // Growth after warm-up: the first 10% of the run (caches, JIT, GL objects) is not counted.
        val steady = samples.filter { it.t >= minutes * 0.1 }
        val pssSlope = slope(steady, Sample::pssKb); val nativeSlope = slope(steady, Sample::nativeKb)
        val growth = steady.last().pssKb - steady.first().pssKb
        Log.i(TAG, "SOAK result minutes=%.1f cycles=%d pss_growth_kb=%d pss_slope_kb_per_min=%.1f native_slope_kb_per_min=%.1f"
            .format(minutes, cycle, growth, pssSlope, nativeSlope))
        assertTrue("PSS grew $growth KB during the soak", growth < 64 * 1024)
        assertTrue("PSS keeps growing: %.1f KB/min".format(pssSlope), pssSlope < 1024)
        assertTrue("native heap keeps growing: %.1f KB/min".format(nativeSlope), nativeSlope < 512)
    }
}
