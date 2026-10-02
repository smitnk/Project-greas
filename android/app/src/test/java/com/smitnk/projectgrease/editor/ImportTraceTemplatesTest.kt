package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ImportTraceTemplatesTest {
    private fun near(e: Float, a: Float) = assertEquals(e, a, 1e-3f)

    @Test
    fun svgShapesBecomeStrokes() {
        val s = SvgImport.parse("""<svg><rect x="10" y="20" width="30" height="40" stroke="#f00" fill="#00ff00" stroke-width="3"/>
            <line x1="0" y1="0" x2="5" y2="5"/><circle cx="50" cy="50" r="10" style="stroke:rgb(0,0,255); fill:none"/></svg>""")
        assertEquals(3, s.size)
        assertTrue(s[0].closed); assertEquals(4, s[0].points.size); near(40f, s[0].points[2][0])
        assertEquals(0xFFFF0000.toInt(), s[0].strokeArgb); assertEquals(0xFF00FF00.toInt(), s[0].fillArgb); near(3f, s[0].width)
        assertNull(s[1].fillArgb)
        near(5f, s[1].points[1][0]); near(5f, s[1].points[1][1]) // x2/y2: attribute names with digits
        assertEquals(SvgImport.CIRCLE_SEGMENTS, s[2].points.size); assertNull(s[2].fillArgb)
    }

    @Test
    fun svgPathCommands() {
        val p = SvgImport.parse("""<path d="M10 10 L20 10 l0 10 H5 v-5 Z M100,100 C100,200 200,200 200,100"/>""")
        assertEquals(2, p.size); assertTrue(p[0].closed); assertEquals(5, p[0].points.size)
        near(5f, p[0].points[4][0]); near(15f, p[0].points[4][1])
        val mid = p[1].points[SvgImport.CURVE_SEGMENTS / 2]
        near(150f, mid[0]); near(175f, mid[1])
        val q = SvgImport.parse("""<path d="M0 0 Q10 10 20 0 T40 0 z L5 5"/>""")
        assertEquals(2, q.size)
        near(-5f, q[0].points[SvgImport.CURVE_SEGMENTS + SvgImport.CURVE_SEGMENTS / 2][1])
        near(0f, q[1].points[0][0])
    }

    private fun image(w: Int, h: Int, ink: (Int, Int) -> Boolean) =
        IntArray(w * h) { i -> if (ink(i % w, i / w)) 0xFF000000.toInt() else 0xFFFFFFFF.toInt() }

    @Test
    fun traceOutlines() {
        val sq = image(12, 12) { x, y -> x in 3..8 && y in 3..8 }
        val t = ImageTrace.trace(ImageTrace.mask(sq, 12, 12, 0.5f, false), 12, 12, 0.6f, 3)
        assertEquals(1, t.size); assertEquals(4, t[0].size)
        val two = image(20, 10) { x, y -> y in 2..5 && (x in 2..5 || x in 12..16) }
        assertEquals(2, ImageTrace.trace(ImageTrace.mask(two, 20, 10, 0.5f, false), 20, 10, 0.6f, 3).size)
        val ring = image(14, 14) { x, y -> x in 2..11 && y in 2..11 && (x < 5 || x > 8 || y < 5 || y > 8) }
        assertEquals(2, ImageTrace.trace(ImageTrace.mask(ring, 14, 14, 0.5f, false), 14, 14, 0.6f, 3).size)
    }

    @Test
    fun templates() {
        assertEquals(listOf("Background", "Fills", "Lines"), GreaseTemplates.byId("2d_animation")!!.layers)
        assertNull(GreaseTemplates.byId("missing"))
    }
}
