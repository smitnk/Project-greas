package com.smitnk.projectgrease

import android.graphics.Bitmap
import android.media.MediaExtractor
import android.media.MediaFormat
import android.media.MediaMetadataRetriever
import android.os.Build
import java.io.File

/**
 * MP4 checks that work on every supported API level: METADATA_KEY_VIDEO_FRAME_COUNT and
 * getFrameAtIndex only exist from API 28 (they return null / throw on API 26-27).
 */
fun mp4VideoSampleCount(file: File): Int {
    val ex = MediaExtractor()
    try {
        ex.setDataSource(file.absolutePath)
        val track = (0 until ex.trackCount).firstOrNull {
            ex.getTrackFormat(it).getString(MediaFormat.KEY_MIME)?.startsWith("video/") == true
        } ?: return 0
        ex.selectTrack(track)
        var n = 0
        while (ex.sampleTime >= 0) { n++; ex.advance() }
        return n
    } finally { ex.release() }
}

fun mp4FirstFrame(r: MediaMetadataRetriever): Bitmap? =
    if (Build.VERSION.SDK_INT >= 28) r.getFrameAtIndex(0)
    else r.getFrameAtTime(0, MediaMetadataRetriever.OPTION_CLOSEST_SYNC)
