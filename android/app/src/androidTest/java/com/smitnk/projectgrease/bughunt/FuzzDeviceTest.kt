package com.smitnk.projectgrease.bughunt

import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.smitnk.projectgrease.SweepBase
import com.smitnk.projectgrease.nativebridge.GPNative
import com.smitnk.projectgrease.nativebridge.ProjectGreaseEglSurface
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.random.Random

/**
 * On-device fuzzing through the real JNI boundary (in the bug-hunt build: under ASan and the GL error
 * checks): random edit command ids / arguments into nativeApplyEditCommand, random and broken project
 * JSON into the loader, random SVG into Import SVG, random images into Trace. No crash, no ASan report,
 * and the document must still save and load to itself afterwards.
 */
@BugHunt
@RunWith(AndroidJUnit4::class)
class FuzzDeviceTest : SweepBase() {
    private val iterations = (InstrumentationRegistry.getArguments().getString("fuzzIterations") ?: "3000").toInt()
    private val seed = (InstrumentationRegistry.getArguments().getString("fuzzSeed") ?: "20240501").toInt()

    private fun gpHandle(): Long = onUi {
        GPNative.nativeGetGpHandle((surface() as ProjectGreaseEglSurface).getRendererHandle())
    }

    /** The document survives a save/load round trip unchanged (no corrupted state). */
    private fun assertRoundTrip(where: String) {
        val once = onUi { controller.saveDocumentJson() } ?: error("$where: save failed")
        assertTrue("$where: reload", onUi { controller.loadDocumentJson(once) })
        val twice = onUi { controller.saveDocumentJson() } ?: error("$where: second save failed")
        val a = JSONObject(once).also { it.remove("frame") }.toString()
        val b = JSONObject(twice).also { it.remove("frame") }.toString()
        assertEquals("$where: save/load round trip", a, b)
    }

    private fun value(r: Random): Float = when (r.nextInt(12)) {
        0 -> 0f; 1 -> 1f; 2 -> -1f; 3 -> 0.5f; 4 -> r.nextInt(0, 6).toFloat(); 5 -> r.nextInt(-3, 40).toFloat()
        6 -> 1e9f; 7 -> Float.NaN; 8 -> Float.POSITIVE_INFINITY; 9 -> r.nextFloat() * 1000f
        10 -> r.nextInt(100, 100000).toFloat(); else -> r.nextFloat()
    }

    @Test fun editCommands() {
        val r = Random(seed)
        repeat(6) { line(150f + it * 100f) }
        val gp = gpHandle()
        for (i in 0 until iterations) {
            val cmd = r.nextInt(0, 131)
            val args = FloatArray(r.nextInt(0, 12)) { value(r) }
            onUi {
                if (r.nextInt(3) != 0) GPNative.nativeApplyEditCommand(gp, 1, floatArrayOf(1f))
                GPNative.nativeApplyEditCommand(gp, cmd, args)
                GPNative.nativeHistoryRecord(gp)
                if (r.nextInt(10) == 0) GPNative.nativeHistoryUndo(gp)
                controller.render()
            }
            if (onUi { controller.strokeCount() } < 3) repeat(3) { line(200f + it * 120f) }
            if (i % 500 == 499) { assertRoundTrip("edit fuzz iteration $i"); Log.i(TAG, "FUZZ edit $i ok") }
        }
        assertRoundTrip("edit fuzz end")
        shot("fuzz_edit_commands")
    }

    @Test fun projectJson() {
        repeat(4) { line(200f + it * 100f) }
        val base = onUi { controller.saveDocumentJson() }!!
        val r = Random(seed)
        val tokens = listOf("0", "-1", "1e9", "1e309", "\"x\"", "null", "[]", "{}", "2147483648", "true")
        for (i in 0 until iterations / 3) {
            val sb = StringBuilder(base)
            repeat(r.nextInt(1, 5)) {
                if (sb.isEmpty()) return@repeat
                val at = r.nextInt(sb.length)
                when (r.nextInt(4)) {
                    0 -> sb.deleteRange(at, minOf(sb.length, at + r.nextInt(1, 30)))
                    1 -> sb.insert(at, tokens.random(r))
                    2 -> sb.setLength(at)
                    else -> Regex("-?\\d+(\\.\\d+)?").find(sb, at)?.let { m -> sb.replace(m.range.first, m.range.last + 1, tokens.random(r)) }
                }
            }
            val loaded = onUi { controller.loadDocumentJson(sb.toString()) }
            if (loaded) assertRoundTrip("json mutation $i")
            onUi { controller.loadDocumentJson(base) }
        }
        shot("fuzz_project_json")
    }

    @Test fun svgImport() {
        val r = Random(seed)
        val cmds = "MmLlHhVvCcSsQqTtAaZz"
        for (i in 0 until iterations / 3) {
            val d = (0 until r.nextInt(1, 20)).joinToString(" ") {
                "${cmds[r.nextInt(cmds.length)]}" + (0 until r.nextInt(0, 8)).joinToString(",") {
                    listOf("1e999", "-", ".5.5", "${r.nextInt(-3000, 3000)}", "${r.nextDouble(-50.0, 50.0)}").random(r)
                }
            }
            val svg = "<svg viewBox=\"0 0 ${r.nextInt(-5, 1500)} ${r.nextInt(-5, 1500)}\"><path d=\"$d\"/>" +
                "<circle cx=\"9\" cy=\"9\" r=\"${listOf("0", "-2", "1e300", "40").random(r)}\"/></svg>"
            onUi { controller.importSvg(svg); controller.render() }
            if (i % 100 == 99) assertRoundTrip("svg import $i")
        }
        assertRoundTrip("svg import end")
        shot("fuzz_svg_import")
    }

    @Test fun imageTrace() {
        val r = Random(seed)
        for (i in 0 until iterations / 20) {
            val w = r.nextInt(1, 160); val h = r.nextInt(1, 160)
            val argb = IntArray(w * h) { if (r.nextInt(3) == 0) 0xFF000000.toInt() else r.nextInt() }
            onUi { controller.traceImage(argb, w, h, listOf(0f, 0.5f, 1f, Float.NaN).random(r), r.nextBoolean(), listOf(0f, 1f, 5f, -1f).random(r)); controller.render() }
            if (i % 25 == 24) assertRoundTrip("trace $i")
        }
        assertRoundTrip("trace end")
        shot("fuzz_image_trace")
    }
}
