package com.smitnk.projectgrease.ui

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.MediaMuxer
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.VideoFrames
import java.io.File
import java.io.FileDescriptor

/**
 * MP4 (H.264) export of the project's frame range: every frame rendered offscreen by
 * [EditorController.renderExportFrames] (project settings start..end, held frames repeat their
 * keyframe), converted to YUV 4:2:0 into the encoder's input images, encoded by MediaCodec and
 * written by MediaMuxer at the project fps. Returns the number of frames written (0 on failure).
 */
object VideoExport {
    fun exportToFile(controller: EditorController, file: File): Int =
        runCatching { MediaMuxer(file.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4) }.getOrNull()
            ?.let { encode(controller, it) } ?: 0

    fun exportToDescriptor(controller: EditorController, fd: FileDescriptor): Int =
        runCatching { MediaMuxer(fd, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4) }.getOrNull()
            ?.let { encode(controller, it) } ?: 0

    private fun encode(controller: EditorController, muxer: MediaMuxer): Int {
        val srcW = controller.document.canvasWidth
        val (w, h) = VideoFrames.evenSize(srcW, controller.document.canvasHeight)
        val fps = controller.projectSettings.fps.coerceIn(1, 120)
        val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, w, h).apply {
            setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible)
            setInteger(MediaFormat.KEY_BIT_RATE, VideoFrames.bitRate(w, h, fps))
            setInteger(MediaFormat.KEY_FRAME_RATE, fps)
            setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
        }
        val codec = runCatching { MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC) }.getOrNull() ?: run { muxer.release(); return 0 }
        var frames = 0
        var track = -1
        var muxing = false
        val info = MediaCodec.BufferInfo()
        fun drain(endOfStream: Boolean) {
            while (true) {
                val index = codec.dequeueOutputBuffer(info, if (endOfStream) 10_000L else 0L)
                when {
                    index == MediaCodec.INFO_TRY_AGAIN_LATER -> if (!endOfStream) return
                    index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                        track = muxer.addTrack(codec.outputFormat); muxer.start(); muxing = true
                    }
                    index >= 0 -> {
                        val buf = codec.getOutputBuffer(index)
                        if (buf != null && info.size > 0 && muxing && (info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0) {
                            buf.position(info.offset); buf.limit(info.offset + info.size)
                            muxer.writeSampleData(track, buf, info)
                        }
                        codec.releaseOutputBuffer(index, false)
                        if ((info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) return
                    }
                }
            }
        }
        return try {
            codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            codec.start()
            val ok = controller.renderExportFrames(false) { _, argb ->
                var index = -1
                while (index < 0) { index = codec.dequeueInputBuffer(10_000L); if (index < 0) drain(false) }
                val image = codec.getInputImage(index) ?: return@renderExportFrames
                val planes = image.planes
                val y = ByteArray(planes[0].buffer.remaining()); val u = ByteArray(planes[1].buffer.remaining()); val v = ByteArray(planes[2].buffer.remaining())
                planes[0].buffer.duplicate().get(y); planes[1].buffer.duplicate().get(u); planes[2].buffer.duplicate().get(v)
                VideoFrames.argbToYuv420(argb, srcW, w, h, y, planes[0].rowStride, planes[0].pixelStride,
                    u, v, planes[1].rowStride, planes[1].pixelStride)
                planes[0].buffer.duplicate().put(y); planes[1].buffer.duplicate().put(u); planes[2].buffer.duplicate().put(v)
                codec.queueInputBuffer(index, 0, w * h * 3 / 2, VideoFrames.presentationTimeUs(frames, fps), 0)
                frames++
                drain(false)
            }
            var eos = -1
            while (eos < 0) { eos = codec.dequeueInputBuffer(10_000L); if (eos < 0) drain(false) }
            codec.queueInputBuffer(eos, 0, 0, VideoFrames.presentationTimeUs(frames, fps), MediaCodec.BUFFER_FLAG_END_OF_STREAM)
            drain(true)
            if (ok && muxing) frames else 0
        } catch (e: Exception) {
            0
        } finally {
            runCatching { codec.stop() }; codec.release()
            runCatching { if (muxing) muxer.stop() }; muxer.release()
        }
    }
}
