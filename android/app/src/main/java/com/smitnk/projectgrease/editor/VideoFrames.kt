package com.smitnk.projectgrease.editor

/**
 * Pure parts of the MP4 export (ui/VideoExport.kt drives MediaCodec / MediaMuxer): the even frame
 * size H.264 needs, the presentation time of frame k at a frame rate, and ARGB to YUV 4:2:0
 * (BT.601 limited range, the colour space of Android's AVC encoders) written into the encoder's
 * planes with their pixel / row strides.
 */
object VideoFrames {
    /** H.264 4:2:0 needs even dimensions: one pixel row / column is cropped when odd. */
    fun evenSize(width: Int, height: Int): Pair<Int, Int> = (width and 1.inv()).coerceAtLeast(2) to (height and 1.inv()).coerceAtLeast(2)

    fun presentationTimeUs(index: Int, fps: Int): Long = index.toLong() * 1_000_000L / fps.coerceAtLeast(1)

    /** Bit rate for [w] x [h] at [fps]: about 0.2 bits per pixel per frame, at least 1 Mbit/s. */
    fun bitRate(w: Int, h: Int, fps: Int): Int = (w.toLong() * h * fps.coerceAtLeast(1) / 5).coerceIn(1_000_000L, 40_000_000L).toInt()

    fun y(r: Int, g: Int, b: Int): Int = ((66 * r + 129 * g + 25 * b + 128) shr 8) + 16
    fun u(r: Int, g: Int, b: Int): Int = ((-38 * r - 74 * g + 112 * b + 128) shr 8) + 128
    fun v(r: Int, g: Int, b: Int): Int = ((112 * r - 94 * g - 18 * b + 128) shr 8) + 128

    /** Writes the top-left [w] x [h] of [argb] (row length [srcWidth]) into Y / U / V planes. */
    fun argbToYuv420(
        argb: IntArray, srcWidth: Int, w: Int, h: Int,
        yPlane: ByteArray, yRowStride: Int, yPixelStride: Int,
        uPlane: ByteArray, vPlane: ByteArray, uvRowStride: Int, uvPixelStride: Int
    ) {
        for (row in 0 until h) {
            for (col in 0 until w) {
                val c = argb[row * srcWidth + col]
                val r = (c shr 16) and 255; val g = (c shr 8) and 255; val b = c and 255
                yPlane[row * yRowStride + col * yPixelStride] = y(r, g, b).coerceIn(0, 255).toByte()
                if ((row and 1) == 0 && (col and 1) == 0) {
                    // average of the 2x2 block
                    var rs = 0; var gs = 0; var bs = 0
                    for (dy in 0..1) for (dx in 0..1) {
                        val cc = argb[(row + dy) * srcWidth + col + dx]
                        rs += (cc shr 16) and 255; gs += (cc shr 8) and 255; bs += cc and 255
                    }
                    val o = (row / 2) * uvRowStride + (col / 2) * uvPixelStride
                    uPlane[o] = u(rs / 4, gs / 4, bs / 4).coerceIn(0, 255).toByte()
                    vPlane[o] = v(rs / 4, gs / 4, bs / 4).coerceIn(0, 255).toByte()
                }
            }
        }
    }
}
