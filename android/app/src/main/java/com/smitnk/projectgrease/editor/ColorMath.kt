package com.smitnk.projectgrease.editor

import kotlin.math.abs
import kotlin.math.floor
import kotlin.math.roundToInt

/**
 * HSV <-> RGB using Blender's rgb_to_hsv()/hsv_to_rgb() (blenlib math_color.c), so the picker shows
 * the same hue/saturation/value numbers as Blender's color picker. All components are 0..1.
 */
object ColorMath {
    data class Hsva(val h: Float, val s: Float, val v: Float, val a: Float = 1f)

    fun rgbToHsv(r: Float, g0: Float, b0: Float): FloatArray {
        var rr = r; var g = g0; var b = b0
        var k = 0f
        if (g < b) { val t = g; g = b; b = t; k = -1f }
        var minGb = b
        if (rr < g) { val t = rr; rr = g; g = t; k = -2f / 6f - k; minGb = minOf(g, b) }
        val chroma = rr - minGb
        val h = abs(k + (g - b) / (6f * chroma + 1e-20f))
        val s = chroma / (rr + 1e-20f)
        return floatArrayOf(h, s, rr)
    }

    fun hsvToRgb(h: Float, s: Float, v: Float): FloatArray {
        val nr = (abs(h * 6f - 3f) - 1f).coerceIn(0f, 1f)
        val ng = (2f - abs(h * 6f - 2f)).coerceIn(0f, 1f)
        val nb = (2f - abs(h * 6f - 4f)).coerceIn(0f, 1f)
        return floatArrayOf(((nr - 1f) * s + 1f) * v, ((ng - 1f) * s + 1f) * v, ((nb - 1f) * s + 1f) * v)
    }

    private fun channel(x: Float) = (x.coerceIn(0f, 1f) * 255f).roundToInt()

    fun hsvaToArgb(c: Hsva): Int {
        val h = c.h - floor(c.h) // wrap hue like fractf()
        val rgb = hsvToRgb(h, c.s.coerceIn(0f, 1f), c.v.coerceIn(0f, 1f))
        return (channel(c.a) shl 24) or (channel(rgb[0]) shl 16) or (channel(rgb[1]) shl 8) or channel(rgb[2])
    }

    fun argbToHsva(argb: Int): Hsva {
        val a = ((argb ushr 24) and 0xFF) / 255f
        val r = ((argb shr 16) and 0xFF) / 255f
        val g = ((argb shr 8) and 0xFF) / 255f
        val b = (argb and 0xFF) / 255f
        val hsv = rgbToHsv(r, g, b)
        return Hsva(hsv[0], hsv[1], hsv[2], a)
    }

    /** "#RRGGBB" (alpha shown separately, like Blender's hex field). */
    fun toHex(argb: Int): String = "#%06X".format(argb and 0xFFFFFF)

    /** Accepts "RRGGBB", "#RRGGBB" or "#RGB"; keeps the given alpha. Null if invalid. */
    fun parseHex(text: String, alpha: Int = 0xFF): Int? {
        var t = text.trim().removePrefix("#")
        if (t.length == 3) t = t.map { "$it$it" }.joinToString("")
        if (t.length != 6 || t.any { it.digitToIntOrNull(16) == null }) return null
        return ((alpha and 0xFF) shl 24) or t.toInt(16)
    }
}
