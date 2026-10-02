package com.smitnk.projectgrease.editor

import org.json.JSONArray
import org.json.JSONObject

/**
 * Kotlin side of the annotation data (native/blender_gp/project_grease_annotations.h): the flat
 * native dump [frame_count, { framenum, stroke_count, { point_count, x, y, ... } }], its project
 * file form (key "annotations") and its optional export layer. Annotations are not part of the
 * drawing: export leaves them out unless asked.
 */
object AnnotationData {
    class Frame(val frame: Int, val strokes: List<FloatArray>)

    /** Parses a native dump; null when it is malformed. */
    fun frames(dump: FloatArray?): List<Frame>? {
        if (dump == null || dump.isEmpty()) return null
        var k = 0
        fun next(): Float? = if (k < dump.size) dump[k++] else null
        val count = next()?.toInt() ?: return null
        if (count < 0) return null
        val frames = ArrayList<Frame>(count)
        repeat(count) {
            val number = next()?.toInt() ?: return null
            val strokeCount = next()?.toInt() ?: return null
            if (strokeCount < 0) return null
            val strokes = ArrayList<FloatArray>(strokeCount)
            repeat(strokeCount) {
                val points = next()?.toInt() ?: return null
                if (points < 0 || k + points * 2 > dump.size) return null
                strokes += dump.copyOfRange(k, k + points * 2)
                k += points * 2
            }
            frames += Frame(number, strokes)
        }
        return if (k == dump.size) frames else null
    }

    fun dump(frames: List<Frame>): FloatArray {
        val out = ArrayList<Float>()
        out += frames.size.toFloat()
        for (f in frames) {
            out += f.frame.toFloat(); out += f.strokes.size.toFloat()
            for (s in f.strokes) { out += (s.size / 2).toFloat(); s.forEach { out += it } }
        }
        return out.toFloatArray()
    }

    /** The annotation frame shown at [frame]: the last one at or before it (frames hold). */
    fun shownAt(frames: List<Frame>, frame: Int): Frame? = frames.filter { it.frame <= frame }.maxByOrNull { it.frame }

    /** Project file form; [style] is r, g, b, a, thickness. Null when there is nothing to save. */
    fun toJson(dump: FloatArray?, style: FloatArray?): JSONObject? {
        val frames = frames(dump) ?: emptyList()
        if (frames.isEmpty() && style == null) return null
        val root = JSONObject()
        if (style != null && style.size >= 5) {
            root.put("color", JSONArray().apply { for (i in 0 until 4) put(style[i].toDouble()) })
            root.put("thickness", style[4].toDouble())
        }
        root.put("frames", JSONArray().apply {
            frames.forEach { f ->
                put(JSONObject().apply {
                    put("frame", f.frame)
                    put("strokes", JSONArray().apply {
                        f.strokes.forEach { s -> put(JSONArray().apply { s.forEach { put(it.toDouble()) } }) }
                    })
                })
            }
        })
        return root
    }

    class Parsed(val style: FloatArray?, val dump: FloatArray)

    fun fromJson(json: JSONObject?): Parsed? {
        if (json == null) return null
        val color = json.optJSONArray("color")
        val style = if (color != null && color.length() == 4 && json.has("thickness")) {
            floatArrayOf(color.optDouble(0).toFloat(), color.optDouble(1).toFloat(), color.optDouble(2).toFloat(),
                color.optDouble(3).toFloat(), json.optDouble("thickness", 3.0).toFloat())
        } else null
        val frames = ArrayList<Frame>()
        val array = json.optJSONArray("frames") ?: JSONArray()
        for (i in 0 until array.length()) {
            val f = array.optJSONObject(i) ?: continue
            val strokes = ArrayList<FloatArray>()
            val sa = f.optJSONArray("strokes") ?: JSONArray()
            for (j in 0 until sa.length()) {
                val pts = sa.optJSONArray(j) ?: continue
                if (pts.length() < 2 || pts.length() % 2 != 0) continue
                strokes += FloatArray(pts.length()) { pts.optDouble(it).toFloat() }
            }
            frames += Frame(f.optInt("frame", 1).coerceAtLeast(0), strokes)
        }
        return Parsed(style, dump(frames.sortedBy { it.frame }.distinctBy { it.frame }))
    }

    /** The "Annotations" export layer for [frame], or null when no annotation is shown there. */
    fun exportLayer(dump: FloatArray?, style: FloatArray?, frame: Int): VectorLayer? {
        val shown = shownAt(frames(dump) ?: return null, frame) ?: return null
        if (shown.strokes.isEmpty()) return null
        val color = if (style != null && style.size >= 4) floatArrayOf(style[0], style[1], style[2], style[3]) else floatArrayOf(0f, 0.6f, 1f, 1f)
        val width = if (style != null && style.size >= 5) style[4] else 3f
        return VectorLayer("Annotations", shown.strokes.map { s ->
            VectorShape((0 until s.size / 2).map { floatArrayOf(s[it * 2], s[it * 2 + 1]) }, false, null, color, width)
        })
    }
}
