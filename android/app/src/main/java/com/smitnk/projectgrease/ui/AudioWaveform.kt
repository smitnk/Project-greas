package com.smitnk.projectgrease.ui

import android.content.Context
import android.media.MediaCodec
import android.media.MediaExtractor
import android.media.MediaFormat
import android.net.Uri
import java.nio.ByteOrder

/**
 * Waveform of an audio file for the audio timeline: the loudest sample (0..1) in each of [BUCKETS]
 * equal slices of the whole file. Decoded once per file (off the UI thread) and cached.
 */
object AudioWaveform {
    const val BUCKETS = 512
    private val cache = HashMap<String, FloatArray>()

    fun cached(uri: String): FloatArray? = synchronized(cache) { cache[uri] }

    /** Blocking: call from a background thread. Empty array when the file can't be decoded. */
    fun peaks(context: Context, uri: String): FloatArray {
        cached(uri)?.let { return it }
        val result = runCatching { decode(context, Uri.parse(uri)) }.getOrNull() ?: FloatArray(0)
        synchronized(cache) { cache[uri] = result }
        return result
    }

    private fun decode(context: Context, uri: Uri): FloatArray? {
        val ex = MediaExtractor()
        try {
            ex.setDataSource(context, uri, null)
            val track = (0 until ex.trackCount).firstOrNull { ex.getTrackFormat(it).getString(MediaFormat.KEY_MIME)?.startsWith("audio/") == true } ?: return null
            ex.selectTrack(track)
            val format = ex.getTrackFormat(track)
            val durationUs = if (format.containsKey(MediaFormat.KEY_DURATION)) format.getLong(MediaFormat.KEY_DURATION) else return null
            if (durationUs <= 0) return null
            val codec = MediaCodec.createDecoderByType(format.getString(MediaFormat.KEY_MIME)!!)
            val out = FloatArray(BUCKETS)
            try {
                codec.configure(format, null, null, 0); codec.start()
                val info = MediaCodec.BufferInfo()
                var inputDone = false
                while (true) {
                    if (!inputDone) {
                        val i = codec.dequeueInputBuffer(10_000)
                        if (i >= 0) {
                            val n = ex.readSampleData(codec.getInputBuffer(i)!!, 0)
                            if (n < 0) { codec.queueInputBuffer(i, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM); inputDone = true }
                            else { codec.queueInputBuffer(i, 0, n, ex.sampleTime, 0); ex.advance() }
                        }
                    }
                    val o = codec.dequeueOutputBuffer(info, 10_000)
                    if (o >= 0) {
                        val buf = codec.getOutputBuffer(o)
                        if (buf != null && info.size > 0) {
                            // 16-bit PCM (the decoder default); the whole buffer belongs to the bucket of its timestamp
                            val b = ((info.presentationTimeUs * BUCKETS) / durationUs).toInt().coerceIn(0, BUCKETS - 1)
                            val sb = buf.order(ByteOrder.nativeOrder()).asShortBuffer()
                            var peak = out[b]
                            var k = info.offset / 2; val end = (info.offset + info.size) / 2
                            while (k < end) { val v = kotlin.math.abs(sb.get(k).toInt()) / 32768f; if (v > peak) peak = v; k += 4 }
                            out[b] = peak
                        }
                        codec.releaseOutputBuffer(o, false)
                        if ((info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) break
                    }
                }
            } finally { runCatching { codec.stop() }; codec.release() }
            // fill buckets no buffer started in (short files) from the left neighbour
            for (k in 1 until BUCKETS) if (out[k] == 0f) out[k] = out[k - 1] * 0.98f
            return out
        } finally { ex.release() }
    }
}
