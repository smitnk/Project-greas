package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import kotlin.random.Random

/**
 * Bug-hunt fuzzing of the importers on the JVM (fixed seeds, bounded time): project JSON (mutated
 * real documents and edge cases) through parse -> restore -> encode, random SVG into SvgImport, random
 * images into ImageTrace. Nothing may throw; whatever loads must re-save to the same document, and
 * imported / traced geometry must be finite.
 */
class InputFuzzTest {
    private val seeds = listOf(20240501, 7, 1234)

    // ---- project files --------------------------------------------------------------------------
    private fun sampleDocument(): String {
        val doc = FakeDocument()
        doc.createMaterial(); doc.createMaterial()
        doc.applyMaterialRecord(1, MaterialRecord(floatArrayOf(1f, 0f, 0f, 1f), floatArrayOf(0f, 1f, 0f, 0.5f), true, true, "Ink", mode = 1))
        doc.selectLayer(0)
        for (f in listOf(1, 3, 7)) {
            doc.createFrame(f)
            for (s in 0 until 3) {
                doc.addStroke(StrokeRecord(List(5) { i -> FloatArray(10) { k -> when (k) { 0 -> 10f * i + s; 1 -> 5f * f; 2 -> 0f; 3 -> 0.7f; 4 -> 1f; else -> 0.25f } } },
                    materialIndex = s % 3, thickness = 3f + s, cyclic = s == 1, weights = if (s == 2) mapOf(0 to floatArrayOf(0f, 0.5f)) else emptyMap()))
            }
        }
        doc.createLayer("Second"); doc.createFrame(2)
        doc.addStroke(StrokeRecord(listOf(FloatArray(10), FloatArray(10) { if (it == 0) 9f else 0f })))
        doc.restoreVertexGroups(listOf("A"), 0)
        return ProjectDocumentCodec.encode(doc, 800, 600, 24, 1, 12)
    }

    private val edgeTokens = listOf("0", "-1", "1e9", "-1e9", "1e309", "2147483648", "-2147483649", "0.0000001", "\"x\"", "null",
        "true", "[]", "{}", "[1,2]", "{\"a\":1}", "\"\"", "[[[[[[]]]]]]", "1.5", "-0", "99999999999999999999")

    private fun mutate(src: String, rnd: Random): String {
        val sb = StringBuilder(src)
        repeat(rnd.nextInt(1, 6)) {
            if (sb.isEmpty()) return sb.toString()
            val at = rnd.nextInt(sb.length)
            when (rnd.nextInt(6)) {
                0 -> sb.deleteRange(at, minOf(sb.length, at + rnd.nextInt(1, 40)))
                1 -> sb.insert(at, edgeTokens.random(rnd))
                2 -> { // replace the number starting near `at`
                    val m = Regex("-?\\d+(\\.\\d+)?(e-?\\d+)?").find(sb, at)
                    if (m != null) sb.replace(m.range.first, m.range.last + 1, edgeTokens.random(rnd))
                }
                3 -> sb.setLength(at) // truncated file
                4 -> sb.setCharAt(at, "{}[],:\"0a-.e ".random(rnd))
                else -> { // duplicate a chunk (repeated keys / elements)
                    val end = minOf(sb.length, at + rnd.nextInt(1, 80)); sb.insert(at, sb.substring(at, end))
                }
            }
        }
        return sb.toString()
    }

    private fun loadAndResave(raw: String, where: String) {
        val parsed = try { ProjectDocumentCodec.parse(raw) } catch (t: Throwable) {
            fail("$where: parse threw ${t::class.simpleName}: ${t.message}\n${raw.take(400)}"); return
        } ?: return
        val doc = FakeDocument()
        val ok = try { ProjectDocumentCodec.restore(parsed, doc, 1f) } catch (t: Throwable) {
            fail("$where: restore threw ${t::class.simpleName}: ${t.message}\n${raw.take(400)}"); return
        }
        if (!ok) return
        // a loaded document saves and loads again to the same document
        val once = ProjectDocumentCodec.encode(doc, 800, 600, 24, 1, parsed.frameEnd)
        val again = FakeDocument()
        assertTrue("$where: re-saved document loads", ProjectDocumentCodec.restore(ProjectDocumentCodec.parse(once)!!, again, 1f))
        val twice = ProjectDocumentCodec.encode(again, 800, 600, 24, 1, parsed.frameEnd)
        assertEquals("$where: save/load round trip is stable", once, twice)
    }

    @Test fun projectJsonMutations() {
        val base = sampleDocument()
        loadAndResave(base, "base document")
        for (seed in seeds) {
            val rnd = Random(seed)
            repeat(1500) { i -> loadAndResave(mutate(base, rnd), "seed $seed mutation $i") }
        }
    }

    @Test fun projectJsonEdgeCases() {
        val cases = listOf("", "{}", "[]", "null", "{\"version\":999}", "{\"layers\":null}", "{\"layers\":{}}",
            "{\"layers\":[null,1,\"x\",{}]}", "{\"layers\":[{\"frames\":[{\"number\":-5,\"strokes\":[{\"points\":[[1e309]]}]}]}]}",
            "{\"materials\":[{\"stroke\":[1,2]},{\"stroke\":\"red\"}],\"layers\":[]}",
            "{\"layers\":[{\"frames\":[{\"number\":1,\"strokes\":[{\"points\":[[0,0,0,1,1,0]],\"material\":2147483647}]}]}]}",
            "{\"layers\":[{\"frames\":[{\"number\":2147483647,\"strokes\":[]},{\"number\":2147483647,\"strokes\":[]}]}]}",
            "{\"frameEnd\":-7,\"frame\":-3,\"fps\":0,\"width\":-1,\"height\":0,\"layers\":[]}",
            "{\"vertexGroups\":[null,\"\",1],\"activeVertexGroup\":99,\"layers\":[]}",
            "{\"layers\":[{\"name\":null,\"opacity\":\"x\",\"modifiers\":[{\"type\":-1},{\"type\":99999,\"params\":[1e400]}],\"effects\":[{\"type\":\"blur\"}],\"masks\":[{\"name\":\"nope\"}],\"frames\":[]}]}")
        cases.forEachIndexed { i, c -> loadAndResave(c, "edge case $i") }
        // deeply nested / large input stays bounded
        loadAndResave("{\"layers\":" + "[".repeat(3000) + "]".repeat(3000) + "}", "deep nesting")
        val bigStroke = (0 until 20000).joinToString(",", "[", "]") { "[${it % 500},${it / 500},0,1,1,0]" }
        loadAndResave("{\"version\":6,\"layers\":[{\"frames\":[{\"number\":1,\"strokes\":[{\"points\":$bigStroke}]}]}]}", "20000-point stroke")
    }

    // ---- SVG --------------------------------------------------------------------------------------
    private fun randomPath(rnd: Random): String {
        val cmds = "MmLlHhVvCcSsQqTtAaZz"
        val sb = StringBuilder()
        repeat(rnd.nextInt(1, 30)) {
            sb.append(cmds[rnd.nextInt(cmds.length)])
            repeat(rnd.nextInt(0, 9)) {
                sb.append(when (rnd.nextInt(8)) {
                    0 -> "1e999"; 1 -> "-"; 2 -> "."; 3 -> "1e-999"; 4 -> "${rnd.nextInt(-5000, 5000)}"
                    5 -> ".5.5"; 6 -> "${rnd.nextDouble(-100.0, 100.0)}"; else -> "${rnd.nextInt(0, 2)}"
                }).append(if (rnd.nextBoolean()) " " else ",")
            }
        }
        return sb.toString()
    }

    private fun randomSvg(rnd: Random): String {
        val sb = StringBuilder("<svg viewBox=\"0 0 ${rnd.nextInt(-10, 2000)} ${rnd.nextInt(-10, 2000)}\">")
        repeat(rnd.nextInt(0, 12)) {
            sb.append(when (rnd.nextInt(8)) {
                0 -> "<path d=\"${randomPath(rnd)}\" stroke=\"#${rnd.nextInt(0, 0xFFFFFF).toString(16)}\" fill=\"${listOf("none", "red", "#zz", "url(#g)", "#abc").random(rnd)}\" stroke-width=\"${listOf("1", "-3", "1e99", "x", "").random(rnd)}\"/>"
                1 -> "<polyline points=\"${(0 until rnd.nextInt(0, 30)).joinToString(" ") { "${rnd.nextInt(-99, 999)},${rnd.nextInt(-99, 999)}" }}\"/>"
                2 -> "<polygon points=\"1e999,1 2 , ,3\"/>"
                3 -> "<rect x=\"${rnd.nextInt(-9, 99)}\" y=\"1\" width=\"${listOf("0", "-5", "1e40", "10").random(rnd)}\" height=\"5\" rx=\"${listOf("0", "-1", "1e30", "3").random(rnd)}\"/>"
                4 -> "<circle cx=\"5\" cy=\"5\" r=\"${listOf("0", "-1", "1e300", "7", "NaN").random(rnd)}\"/>"
                5 -> "<ellipse cx=\"5\" cy=\"5\" rx=\"${listOf("0", "-1", "3").random(rnd)}\" ry=\"1e308\"/>"
                6 -> "<line x1=\"0\" y1=\"0\" x2=\"${listOf("1", "1e999", "").random(rnd)}\" y2=\"3\" transform=\"rotate(${rnd.nextInt(-720, 720)}) scale(${listOf("0", "-1", "1e30").random(rnd)})\"/>"
                else -> "<g transform=\"matrix(${(0 until 6).joinToString(",") { "${rnd.nextInt(-3, 3)}" }})\"><path d=\"${randomPath(rnd)}\"/></g>"
            })
        }
        if (rnd.nextInt(4) != 0) sb.append("</svg>")
        return sb.toString()
    }

    @Test fun svgImportFuzz() {
        for (seed in seeds) {
            val rnd = Random(seed)
            repeat(1500) { i ->
                val svg = randomSvg(rnd)
                val strokes = try { SvgImport.parse(svg) } catch (t: Throwable) {
                    fail("seed $seed svg $i: SvgImport threw ${t::class.simpleName}: ${t.message}\n${svg.take(400)}"); return
                }
                for (s in strokes) {
                    assertTrue("seed $seed svg $i: stroke width ${s.width}", s.width.isFinite())
                    for (p in s.points) assertTrue("seed $seed svg $i: point ${p.toList()} not finite\n${svg.take(400)}", p.all { it.isFinite() })
                }
            }
        }
        // pathological sizes
        SvgImport.parse("<path d=\"M0 0" + " l1 1".repeat(20000) + "\"/>")
        SvgImport.parse("<".repeat(50000))
    }

    /** Regression (fuzz): a command letter where a number belongs threw NumberFormatException and
     *  closed the app on import; the path is now drawn up to the last complete segment. */
    @Test fun svgPathErrorStopsAtTheLastCompleteSegment() {
        val strokes = SvgImport.parse("<path d=\"M0 0 L10 0 L20 A 1 C1 2 3\"/>")
        assertEquals(1, strokes.size)
        assertEquals(listOf(0f, 10f), strokes[0].points.map { it[0] })
    }

    /** Regression (fuzz): 1e999 and float overflow became Infinity points. */
    @Test fun svgOutOfRangeNumbersAreErrors() {
        val path = SvgImport.parse("<path d=\"M0 0 L5 5 L1e999 3 L7 7\"/>")
        assertEquals(listOf(0f, 5f), path.single().points.map { it[0] })
        val poly = SvgImport.parse("<polyline points=\"0,0 4,4 1e40,1 9,9\"/>")
        assertTrue(poly.all { s -> s.points.all { p -> p.all { it.isFinite() } } })
        assertEquals(listOf(0f, 4f), poly.single().points.map { it[0] })
    }

    // ---- image trace ------------------------------------------------------------------------------
    @Test fun imageTraceFuzz() {
        for (seed in seeds) {
            val rnd = Random(seed)
            repeat(400) { i ->
                val w = rnd.nextInt(0, 48); val h = rnd.nextInt(0, 48)
                val style = rnd.nextInt(4)
                val argb = IntArray(w * h) { k ->
                    when (style) {
                        0 -> rnd.nextInt()
                        1 -> if (rnd.nextInt(3) == 0) 0xFF000000.toInt() else 0xFFFFFFFF.toInt()
                        2 -> if (((k % maxOf(w, 1)) / 3 + (k / maxOf(w, 1)) / 3) % 2 == 0) 0xFF000000.toInt() else 0 // checker, transparent
                        else -> 0xFF000000.toInt() // all ink
                    }
                }
                val threshold = listOf(0f, 0.5f, 1f, -1f, 2f, Float.NaN).random(rnd)
                val tolerance = listOf(0f, 0.5f, 3f, -1f, 1e9f, Float.NaN).random(rnd)
                val outlines = try {
                    ImageTrace.trace(ImageTrace.mask(argb, w, h, threshold, rnd.nextBoolean()), w, h, tolerance, rnd.nextInt(0, 6))
                } catch (t: Throwable) {
                    fail("seed $seed image $i (${w}x$h style $style threshold $threshold tolerance $tolerance): ImageTrace threw ${t::class.simpleName}: ${t.message}"); return
                }
                for (o in outlines) for (p in o) {
                    assertTrue("seed $seed image $i: point outside the image", p[0] in 0f..w.toFloat() && p[1] in 0f..h.toFloat())
                }
            }
        }
    }
}
