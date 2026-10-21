package com.smitnk.projectgrease.ui

import android.content.Context
import android.media.AudioFormat
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaExtractor
import android.media.MediaFormat
import android.net.Uri
import com.smitnk.projectgrease.editor.AudioStrip
import java.nio.ByteOrder

/**
 * The sound strips mixed for the MP4 export (Blender mixes the scene's sound strips into the
 * render): each file decoded by the phone's decoder, resampled to 44.1 kHz stereo, placed at its
 * start frame relative to the export's first frame, scaled by its volume, summed and clipped, then
 * encoded to AAC.
 */
object AudioMix {
    const val RATE = 44100
    private const val CH = 2

    class Encoded(val format: MediaFormat, val samples: List<Pair<ByteArray, MediaCodec.BufferInfo>>)

    /** Mixed AAC for [frames] scene frames from [frameStart]; null when no strip sounds there. */
    fun encodeForExport(context: Context, strips: List<AudioStrip>, fps: Int, frameStart: Int, frames: Int): Encoded? {
        val pcm = mix(context, strips, fps, frameStart, frames) ?: return null
        return runCatching { encodeAac(pcm) }.getOrNull()
    }

    /** Interleaved stereo 16-bit mix; null when nothing is audible. */
    fun mix(context: Context, strips: List<AudioStrip>, fps: Int, frameStart: Int, frames: Int): ShortArray? {
        val f = fps.coerceIn(1, 120)
        val total = (frames.toLong() * RATE / f).toInt().coerceAtLeast(0)
        if (total == 0) return null
        val acc = FloatArray(total * CH)
        var any = false
        for (s in strips) {
            if (s.muted || s.volume <= 0f) continue
            // sample of the mix where the strip starts (negative: it started before the export range)
            val at = ((s.start - frameStart).toLong() * RATE / f).toInt()
            val skipSec = if (at < 0) -at.toDouble() / RATE else 0.0
            val needSec = (total - at.coerceAtLeast(0)).toDouble() / RATE
            if (needSec <= 0) continue
            val src = decode(context, Uri.parse(s.uri), skipSec, needSec) ?: continue
            val out = resampleStereo(src.first, src.second, src.third)
            val base = at.coerceAtLeast(0)
            val n = minOf(out.size / CH, total - base)
            for (i in 0 until n * CH) acc[base * CH + i] += out[i] * s.volume
            any = any || n > 0
        }
        if (!any) return null
        return ShortArray(acc.size) { acc[it].coerceIn(-32768f, 32767f).toInt().toShort() }
    }

    /** Linear resampling of interleaved [channels]-channel PCM at [rate] to 44.1 kHz stereo floats. */
    fun resampleStereo(pcm: ShortArray, rate: Int, channels: Int): FloatArray {
        val ch = channels.coerceAtLeast(1)
        val inFrames = pcm.size / ch
        if (inFrames == 0 || rate <= 0) return FloatArray(0)
        val outFrames = (inFrames.toLong() * RATE / rate).toInt()
        val out = FloatArray(outFrames * CH)
        for (i in 0 until outFrames) {
            val pos = i.toDouble() * rate / RATE
            val i0 = pos.toInt().coerceAtMost(inFrames - 1); val i1 = (i0 + 1).coerceAtMost(inFrames - 1)
            val t = (pos - i0).toFloat()
            for (c in 0 until CH) {
                val sc = if (ch == 1) 0 else c.coerceAtMost(ch - 1)
                val a = pcm[i0 * ch + sc].toFloat(); val b = pcm[i1 * ch + sc].toFloat()
                out[i * CH + c] = a + (b - a) * t
            }
        }
        return out
    }

    /** Decodes [needSec] seconds after [skipSec] of the file's first audio track: (pcm, rate, channels). */
    private fun decode(context: Context, uri: Uri, skipSec: Double, needSec: Double): Triple<ShortArray, Int, Int>? {
        val ex = MediaExtractor()
        var codec: MediaCodec? = null
        try {
            ex.setDataSource(context, uri, null)
            val track = (0 until ex.trackCount).firstOrNull { ex.getTrackFormat(it).getString(MediaFormat.KEY_MIME)?.startsWith("audio/") == true } ?: return null
            ex.selectTrack(track)
            val inFmt = ex.getTrackFormat(track)
            if (skipSec > 0) ex.seekTo((skipSec * 1e6).toLong(), MediaExtractor.SEEK_TO_PREVIOUS_SYNC)
            codec = MediaCodec.createDecoderByType(inFmt.getString(MediaFormat.KEY_MIME)!!)
            codec.configure(inFmt, null, null, 0); codec.start()
            var rate = inFmt.getInteger(MediaFormat.KEY_SAMPLE_RATE); var ch = inFmt.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
            var floatPcm = false
            val out = ShortArrayBuilder()
            val info = MediaCodec.BufferInfo()
            val startUs = (skipSec * 1e6).toLong(); val endUs = startUs + (needSec * 1e6).toLong()
            var inDone = false
            while (true) {
                if (!inDone) {
                    val i = codec.dequeueInputBuffer(10_000)
                    if (i >= 0) {
                        val n = ex.readSampleData(codec.getInputBuffer(i)!!, 0)
                        if (n < 0 || ex.sampleTime > endUs) { codec.queueInputBuffer(i, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM); inDone = true }
                        else { codec.queueInputBuffer(i, 0, n, ex.sampleTime, 0); ex.advance() }
                    }
                }
                val o = codec.dequeueOutputBuffer(info, 10_000)
                when {
                    o == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> codec.outputFormat.let {
                        rate = it.getInteger(MediaFormat.KEY_SAMPLE_RATE); ch = it.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
                        floatPcm = it.containsKey(MediaFormat.KEY_PCM_ENCODING) && it.getInteger(MediaFormat.KEY_PCM_ENCODING) == AudioFormat.ENCODING_PCM_FLOAT
                    }
                    o >= 0 -> {
                        val buf = codec.getOutputBuffer(o)!!.order(ByteOrder.nativeOrder())
                        buf.position(info.offset); buf.limit(info.offset + info.size)
                        // samples before the skip point (seek lands on an earlier sync frame) are dropped
                        val dropFrames = if (info.presentationTimeUs < startUs) ((startUs - info.presentationTimeUs) * rate / 1_000_000).toInt() else 0
                        if (floatPcm) { val fb = buf.asFloatBuffer(); val a = FloatArray(fb.remaining()); fb.get(a); out.add(a, dropFrames * ch) }
                        else { val sb = buf.asShortBuffer(); val a = ShortArray(sb.remaining()); sb.get(a); out.add(a, dropFrames * ch) }
                        codec.releaseOutputBuffer(o, false)
                        if ((info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) break
                    }
                }
            }
            return Triple(out.toArray(), rate, ch)
        } catch (e: Exception) {
            com.smitnk.projectgrease.CrashLog.event("AUDIO decode failed for export: ${e.message}")
            return null
        } finally {
            runCatching { codec?.stop() }; runCatching { codec?.release() }; ex.release()
        }
    }

    private class ShortArrayBuilder {
        private var a = ShortArray(1 shl 16); private var n = 0
        private fun ensure(k: Int) { if (n + k > a.size) a = a.copyOf(maxOf(a.size * 2, n + k)) }
        fun add(s: ShortArray, skip: Int) { val k = (s.size - skip).coerceAtLeast(0); ensure(k); System.arraycopy(s, s.size - k, a, n, k); n += k }
        fun add(f: FloatArray, skip: Int) { val k = (f.size - skip).coerceAtLeast(0); ensure(k); for (i in 0 until k) a[n + i] = (f[f.size - k + i] * 32767f).coerceIn(-32768f, 32767f).toInt().toShort(); n += k }
        fun toArray() = a.copyOf(n)
    }

    private fun encodeAac(pcm: ShortArray): Encoded {
        val fmt = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, RATE, CH).apply {
            setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC)
            setInteger(MediaFormat.KEY_BIT_RATE, 160_000)
            setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 16384)
        }
        val codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC)
        val samples = ArrayList<Pair<ByteArray, MediaCodec.BufferInfo>>()
        var outFormat: MediaFormat? = null
        try {
            codec.configure(fmt, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE); codec.start()
            val info = MediaCodec.BufferInfo()
            var pos = 0; var inDone = false
            while (true) {
                if (!inDone) {
                    val i = codec.dequeueInputBuffer(10_000)
                    if (i >= 0) {
                        val buf = codec.getInputBuffer(i)!!.order(ByteOrder.nativeOrder()); buf.clear()
                        val k = minOf(buf.capacity() / 2, pcm.size - pos, 2048 * CH)
                        val pts = pos.toLong() / CH * 1_000_000L / RATE
                        if (k <= 0) { codec.queueInputBuffer(i, 0, 0, pts, MediaCodec.BUFFER_FLAG_END_OF_STREAM); inDone = true }
                        else { buf.asShortBuffer().put(pcm, pos, k); pos += k; codec.queueInputBuffer(i, 0, k * 2, pts, 0) }
                    }
                }
                val o = codec.dequeueOutputBuffer(info, 10_000)
                when {
                    o == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> outFormat = codec.outputFormat
                    o >= 0 -> {
                        if (info.size > 0 && (info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0) {
                            val b = codec.getOutputBuffer(o)!!; b.position(info.offset); b.limit(info.offset + info.size)
                            val bytes = ByteArray(info.size); b.get(bytes)
                            samples += bytes to MediaCodec.BufferInfo().apply { set(0, info.size, info.presentationTimeUs, info.flags) }
                        }
                        codec.releaseOutputBuffer(o, false)
                        if ((info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) break
                    }
                }
            }
        } finally { runCatching { codec.stop() }; codec.release() }
        return Encoded(outFormat ?: error("AAC encoder gave no format"), samples)
    }
}
