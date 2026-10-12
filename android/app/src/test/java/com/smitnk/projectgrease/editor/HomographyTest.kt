package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class HomographyTest {
    @Test fun mapsTheFourCorners() {
        val src = floatArrayOf(0f, 0f, 100f, 0f, 100f, 50f, 0f, 50f)
        val dst = floatArrayOf(20f, 0f, 80f, 10f, 100f, 40f, 0f, 50f)
        val m = Homography.fromQuads(src, dst)
        assertNotNull(m)
        for (i in 0 until 4) {
            val (u, v) = Homography.apply(m!!, src[2 * i], src[2 * i + 1])
            assertEquals(dst[2 * i], u, 1e-3f); assertEquals(dst[2 * i + 1], v, 1e-3f)
        }
    }

    @Test fun identityForSameQuad() {
        val q = floatArrayOf(0f, 0f, 10f, 0f, 10f, 10f, 0f, 10f)
        val m = Homography.fromQuads(q, q)!!
        val (x, y) = Homography.apply(m, 3f, 7f)
        assertEquals(3f, x, 1e-4f); assertEquals(7f, y, 1e-4f)
    }

    @Test fun viewRotationRoundTrips() {
        val (vx, vy) = CanvasMapping.toView(300f, 200f, 1080f, 1920f, 1280, 720, 1.3f, 20f, -40f, 0.7f)
        val (cx, cy) = CanvasMapping.toCanvas(vx, vy, 1080f, 1920f, 1280, 720, 1.3f, 20f, -40f, clamp = false, rotation = 0.7f)
        assertEquals(300f, cx, 1e-2f); assertEquals(200f, cy, 1e-2f)
    }
}
