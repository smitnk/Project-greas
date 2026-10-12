package com.smitnk.projectgrease.editor

import kotlin.math.pow

/** Blender's blenlib math_color.c sRGB transfer functions (DNA colours are scene linear). */
object ColorSpace {
    fun srgbToLinear(c: Float): Float =
        if (c < 0.04045f) (if (c < 0f) 0f else c / 12.92f) else ((c + 0.055f) / 1.055f).pow(2.4f)
    fun linearToSrgb(c: Float): Float =
        if (c < 0.0031308f) (if (c < 0f) 0f else c * 12.92f) else 1.055f * c.pow(1f / 2.4f) - 0.055f
}
