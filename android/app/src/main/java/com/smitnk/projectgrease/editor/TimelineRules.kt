package com.smitnk.projectgrease.editor

import org.json.JSONArray
import org.json.JSONObject

/** One scene time marker (Blender's TimeMarker: frame, name, SELECT flag). */
data class TimeMarker(val frame: Int, val name: String, val selected: Boolean = false)

/** Scene preview range (RenderData psfra / pefra with SCER_PRV_RANGE); [enabled] false means off. */
data class PreviewRange(val enabled: Boolean = false, val start: Int = 1, val end: Int = 1)

/** Timeline state kept on the Kotlin side; snapshotted with every undo step and saved in the project. */
data class TimelineState(val markers: List<TimeMarker> = emptyList(), val preview: PreviewRange = PreviewRange())

/**
 * Pure timeline rules: scene markers (anim_markers.c MARKER_OT_add / rename / move / delete), frame
 * scrubbing (anim_ops.c ANIM_OT_change_frame: round to the nearest whole frame) and the preview
 * range (ANIM_OT_previewrange_set / clear, screen_ops.c playback wrap with PRVRANGEON).
 */
object TimelineRules {
    /** MAXFRAME of DNA_scene_types.h. */
    const val MAX_FRAME = 1048574
    /** MAX_NAME (TimeMarker.name is char[64], 63 characters plus the terminator). */
    const val MAX_MARKER_NAME = 63

    /** ed_marker_add_exec: SNPRINTF(marker->name, "F_%02d", frame). */
    fun defaultMarkerName(frame: Int): String = String.format(java.util.Locale.ROOT, "F_%02d", frame)

    /**
     * MARKER_OT_add at [frame]: refused (null) when a marker already sits there; otherwise every
     * marker is deselected and the new one, named "F_<frame>", is the only selected one.
     */
    fun addMarker(markers: List<TimeMarker>, frame: Int): List<TimeMarker>? {
        if (markers.any { it.frame == frame }) return null
        return (markers.map { it.copy(selected = false) } + TimeMarker(frame, defaultMarkerName(frame), true))
            .sortedBy { it.frame }
    }

    /** MARKER_OT_rename: renames the last selected marker (ED_markers_get_first_selected); null when none is selected. */
    fun renameMarker(markers: List<TimeMarker>, name: String): List<TimeMarker>? {
        val index = markers.indexOfFirst { it.selected }
        if (index < 0) return null
        val clean = name.replace('\n', ' ').take(MAX_MARKER_NAME)
        return markers.mapIndexed { i, m -> if (i == index) m.copy(name = clean) else m }
    }

    /** MARKER_OT_move: offsets every selected marker by [offset] whole frames; null when nothing moves. */
    fun moveMarkers(markers: List<TimeMarker>, offset: Int): List<TimeMarker>? {
        if (offset == 0 || markers.none { it.selected }) return null
        return markers.map { if (it.selected) it.copy(frame = (it.frame + offset).coerceIn(-MAX_FRAME, MAX_FRAME)) else it }
            .sortedBy { it.frame }
    }

    /** MARKER_OT_delete: removes the selected markers; null when none is selected. */
    fun deleteMarkers(markers: List<TimeMarker>): List<TimeMarker>? {
        if (markers.none { it.selected }) return null
        return markers.filterNot { it.selected }
    }

    /** Marker selection (MARKER_OT_select): [extend] toggles, otherwise the marker at [frame] is the only selected one. */
    fun selectMarker(markers: List<TimeMarker>, frame: Int, extend: Boolean = false): List<TimeMarker> =
        markers.map {
            when {
                it.frame == frame -> it.copy(selected = if (extend) !it.selected else true)
                extend -> it
                else -> it.copy(selected = false)
            }
        }

    /**
     * ANIM_OT_change_frame: the frame under the pointer is rounded to the nearest whole frame
     * (round_fl_to_int) and clamped to [minFrame]..[maxFrame]. With [snapToKeys] on, the nearest of
     * [keyframes] wins (ties go to the earlier key); no keys means plain frame snapping.
     */
    fun scrubFrame(position: Float, minFrame: Int, maxFrame: Int, keyframes: IntArray = IntArray(0), snapToKeys: Boolean = false): Int {
        val hi = maxOf(minFrame, maxFrame)
        if (!position.isFinite()) return minFrame
        var frame = kotlin.math.floor(position + 0.5f).toInt().coerceIn(minFrame, hi)
        if (snapToKeys && keyframes.isNotEmpty()) {
            var best = keyframes[0]
            for (k in keyframes) if (kotlin.math.abs(k - position) < kotlin.math.abs(best - position)) best = k
            frame = best.coerceIn(minFrame, hi)
        }
        return frame
    }

    /** Frame under a horizontal offset [x] in a strip whose cells are [cellWidth] wide, first cell = frame 1. */
    fun framePosition(x: Float, cellWidth: Float, scrollOffsetFrames: Float = 0f): Float =
        if (cellWidth <= 0f) 1f else scrollOffsetFrames + x / cellWidth + 0.5f

    /** ANIM_OT_previewrange_set: both ends clamped to >= 1 (FRAMENUMBER_MIN_CLAMP), end not before start. */
    fun setPreviewRange(start: Int, end: Int): PreviewRange {
        val s = start.coerceIn(1, MAX_FRAME)
        val e = end.coerceIn(1, MAX_FRAME).coerceAtLeast(s)
        return PreviewRange(true, s, e)
    }

    /** The frames playback runs over: the preview range when on, else the scene 1..[sceneEnd]. */
    fun playbackRange(preview: PreviewRange, sceneEnd: Int): IntRange =
        if (preview.enabled) preview.start..preview.end.coerceAtLeast(preview.start) else 1..sceneEnd.coerceAtLeast(1)

    /**
     * screen_animation_step: the next frame after [current]; past the range end it wraps to the range
     * start when [loop] is on (null = stop). A current frame before the range jumps to its start.
     */
    fun nextPlaybackFrame(current: Int, preview: PreviewRange, sceneEnd: Int, loop: Boolean): Int? {
        val range = playbackRange(preview, sceneEnd)
        val next = current + 1
        if (next < range.first) return range.first
        if (next > range.last) return if (loop) range.first else null
        return next
    }

    fun toJson(state: TimelineState): Pair<JSONArray?, JSONObject?> {
        val markers = if (state.markers.isEmpty()) null else JSONArray().apply {
            for (m in state.markers) put(JSONObject().put("frame", m.frame).put("name", m.name).apply { if (m.selected) put("select", true) })
        }
        val preview = if (!state.preview.enabled) null else
            JSONObject().put("start", state.preview.start).put("end", state.preview.end)
        return markers to preview
    }

    /** Reads "markers" / "previewRange"; missing keys (older files) give no markers and no preview range. */
    fun fromJson(markers: JSONArray?, preview: JSONObject?): TimelineState {
        val list = markers?.let { a ->
            (0 until a.length()).mapNotNull { i ->
                a.optJSONObject(i)?.takeIf { it.has("frame") }?.let {
                    val frame = it.optInt("frame").coerceIn(-MAX_FRAME, MAX_FRAME)
                    TimeMarker(frame, it.optString("name", defaultMarkerName(frame)).take(MAX_MARKER_NAME), it.optBoolean("select", false))
                }
            }.sortedBy { it.frame }
        } ?: emptyList()
        val range = preview?.takeIf { it.has("start") && it.has("end") }?.let { setPreviewRange(it.optInt("start"), it.optInt("end")) } ?: PreviewRange()
        return TimelineState(list, range)
    }
}
