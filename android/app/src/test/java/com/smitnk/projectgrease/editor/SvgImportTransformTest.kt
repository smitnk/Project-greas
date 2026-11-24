package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SvgImportTransformTest {
    private fun near(e: Float, a: Float) = assertEquals(e, a, 1e-2f)

    @Test
    fun lineCoordinatesAreRead() {
        // regression: attribute names with digits (x1, y1, x2, y2) were not parsed
        val s = SvgImport.parse("""<line x1="1" y1="2" x2="30" y2="40"/>""")
        near(1f, s[0].points[0][0]); near(40f, s[0].points[1][1])
    }

    @Test
    fun nestedGroupTransformsAndRotate() {
        val s = SvgImport.parse("""<svg><g transform="translate(100,0)"><g transform="scale(2)"><line x1="0" y1="0" x2="5" y2="0"/></g></g>
            <line x1="0" y1="0" x2="1" y2="0" transform="rotate(90)"/></svg>""")
        near(110f, s[0].points[1][0]); near(0f, s[1].points[1][0]); near(1f, s[1].points[1][1])
    }

    @Test
    fun arcIsExact() {
        val a = SvgImport.parse("""<path d="M0 0 A10 10 0 0 1 20 0"/>""")[0]
        val mid = a.points[a.points.size / 2]
        assertTrue(a.points.size > 4); near(10f, mid[0]); near(10f, kotlin.math.abs(mid[1]))
    }

    @Test
    fun groupClosesAndSiblingsAreUntouched() {
        val s = SvgImport.parse("""<svg><g transform="translate(10,0)"><g transform="translate(0,5)"><line x1="0" y1="0" x2="1" y2="0"/></g>
            <line x1="0" y1="0" x2="1" y2="0"/></g><line x1="0" y1="0" x2="1" y2="0"/></svg>""")
        near(10f, s[0].points[0][0]); near(5f, s[0].points[0][1])
        near(10f, s[1].points[0][0]); near(0f, s[1].points[0][1])
        near(0f, s[2].points[0][0]); near(0f, s[2].points[0][1])
    }

    @Test
    fun manyPathsParseQuickly() {
        val sb = StringBuilder("<svg><g transform=\"translate(1,1)\">")
        for (i in 0 until 5000) sb.append("<path d=\"M0 0 L$i 1\"/>")
        sb.append("</g></svg>")
        val t0 = System.nanoTime()
        val s = SvgImport.parse(sb.toString())
        assertEquals(5000, s.size)
        near(1f, s[0].points[0][0])
        assertTrue("took ${(System.nanoTime() - t0) / 1_000_000} ms", System.nanoTime() - t0 < 5_000_000_000L)
    }
}
