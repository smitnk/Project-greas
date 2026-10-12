package com.smitnk.projectgrease.editor

import kotlin.math.abs

/** The projective 3x3 matrix (row major) taking 4 source points to 4 target points (x0, y0, .. x3, y3). */
object Homography {
    fun fromQuads(src: FloatArray, dst: FloatArray): FloatArray? {
        if (src.size < 8 || dst.size < 8) return null
        // h00 h01 h02 h10 h11 h12 h20 h21 (h22 = 1): two equations per correspondence
        val a = Array(8) { DoubleArray(9) }
        for (i in 0 until 4) {
            val x = src[2 * i].toDouble(); val y = src[2 * i + 1].toDouble()
            val u = dst[2 * i].toDouble(); val v = dst[2 * i + 1].toDouble()
            a[2 * i] = doubleArrayOf(x, y, 1.0, 0.0, 0.0, 0.0, -u * x, -u * y, u)
            a[2 * i + 1] = doubleArrayOf(0.0, 0.0, 0.0, x, y, 1.0, -v * x, -v * y, v)
        }
        for (c in 0 until 8) { // Gauss-Jordan with partial pivoting
            val p = (c until 8).maxByOrNull { abs(a[it][c]) } ?: return null
            if (abs(a[p][c]) < 1e-12) return null
            val t = a[p]; a[p] = a[c]; a[c] = t
            for (r in 0 until 8) if (r != c) {
                val f = a[r][c] / a[c][c]
                for (k in c..8) a[r][k] -= f * a[c][k]
            }
        }
        return FloatArray(9) { if (it == 8) 1f else (a[it][8] / a[it][it]).toFloat() }
    }

    fun apply(m: FloatArray, x: Float, y: Float): Pair<Float, Float> {
        val w = m[6] * x + m[7] * y + m[8]
        return (m[0] * x + m[1] * y + m[2]) / w to (m[3] * x + m[4] * y + m[5]) / w
    }
}
