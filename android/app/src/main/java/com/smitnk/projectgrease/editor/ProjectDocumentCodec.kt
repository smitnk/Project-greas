package com.smitnk.projectgrease.editor

import org.json.JSONArray
import org.json.JSONObject

/**
 * One stroke as saved. Each point is [x, y, z, pressure, strength, time, r, g, b, a]: the last four
 * are bGPDspoint.vert_color, and alpha 0 means "no vertex color" (the material color shows).
 * [fillColor] is bGPDstroke.vert_color_fill with the same convention.
 */
class StrokeRecord(
    val points: List<FloatArray>,
    val materialIndex: Int = 0,
    val thickness: Float = 1f,
    val cyclic: Boolean = false,
    val fillOpacity: Float = 1f,
    val fillColor: FloatArray = floatArrayOf(0f, 0f, 0f, 0f),
    /** false for strokes read from a version-1 file, which stored no style. */
    val hasStyle: Boolean = true
) {
    companion object {
        /** Floats per point: position/pressure/strength/time plus RGBA vertex color. */
        const val POINT_SIZE = 10
    }
}

class LayerRecord(val name: String, val visible: Boolean, val locked: Boolean, val opacity: Float)

/** One entry of a layer's mask list: [name] is the mask layer (masks refer to layers by name). */
class MaskRecord(val name: String, val hidden: Boolean = false, val inverted: Boolean = false)

/** One palette entry; colors are straight RGBA in 0..1. */
class MaterialRecord(
    val stroke: FloatArray,
    val fill: FloatArray,
    val visible: Boolean,
    val fillEnabled: Boolean
)

/** What the save/load code needs from the native document; the real one is [NativeDocumentAdapter]. */
interface DocumentNative {
    fun layerCount(): Int
    fun layerRecord(index: Int): LayerRecord?
    fun selectLayer(index: Int): Boolean
    fun frameNumbers(): IntArray
    fun selectFrame(frame: Int): Boolean
    fun strokeCount(): Int
    fun strokeRecord(index: Int): StrokeRecord?
    fun materialCount(): Int
    fun materialRecord(index: Int): MaterialRecord?

    fun createLayer(name: String): Boolean
    /** Applies name (when not blank), visibility, lock and opacity to an existing layer. */
    fun applyLayerRecord(index: Int, record: LayerRecord): Boolean
    fun createFrame(frame: Int): Boolean
    /** Adds a stroke to the selected frame exactly as recorded. */
    fun addStroke(record: StrokeRecord): Boolean
    fun createMaterial(): Boolean
    fun applyMaterialRecord(index: Int, record: MaterialRecord): Boolean

    /** The layer's live modifier stack (version 4 files); documents without one report an empty stack. */
    fun modifierCount(layer: Int): Int = 0
    fun modifierRecord(layer: Int, index: Int): ModifierRecord? = null
    /** Appends a modifier to the layer's stack exactly as recorded (type, enabled flag, parameters). */
    fun addModifier(layer: Int, record: ModifierRecord): Boolean = true

    /** Layer masks (version 4 files): use-mask flag and the mask list, which refers to layers by name. */
    fun layerUseMask(layer: Int): Boolean = false
    fun layerMasks(layer: Int): List<MaskRecord> = emptyList()
    /** Called after every layer exists and is named, so the names in [masks] can be resolved. */
    fun restoreLayerMasks(layer: Int, useMask: Boolean, masks: List<MaskRecord>): Boolean = true
}

class ParsedFrame(val number: Int, val strokes: List<StrokeRecord>)
/** [record] is null for version-1 files, which stored no layer state. */
class ParsedLayer(
    val record: LayerRecord?,
    val frames: List<ParsedFrame>,
    /** The layer's modifier stack; empty for files before version 4. */
    val modifiers: List<ModifierRecord> = emptyList(),
    val useMask: Boolean = false,
    val masks: List<MaskRecord> = emptyList()
)
class ParsedDocument(
    val version: Int,
    val width: Int?,
    val height: Int?,
    val fps: Int?,
    val frame: Int,
    val materials: List<MaterialRecord>,
    /** null when the file has no "layers" key at all. */
    val layers: List<ParsedLayer>?
)

/**
 * Project file format. Version 1 stored only layers, frames, strokes and points; version 2 adds
 * per-stroke style (material, thickness, cyclic, fill), layer state (name, visibility, lock,
 * opacity) and the material palette; version 3 adds per-point vertex color (points may carry four
 * more values); version 4 adds the per-layer live modifier stack ("modifiers"). Older files still
 * load: missing fields read as zero, which is "no vertex color", and a missing stack is empty.
 */
object ProjectDocumentCodec {
    const val VERSION = 4

    /** Writes the whole document. Moves the native layer/frame selection; the caller restores it. */
    fun encode(native: DocumentNative, width: Int, height: Int, fps: Int, frame: Int): String {
        val root = JSONObject()
        root.put("version", VERSION)
        root.put("width", width)
        root.put("height", height)
        root.put("fps", fps)
        root.put("frame", frame)

        val materials = JSONArray()
        for (i in 0 until native.materialCount()) {
            native.materialRecord(i)?.let { materials.put(materialJson(it)) }
        }
        root.put("materials", materials)

        val layers = JSONArray()
        for (layerIndex in 0 until native.layerCount()) {
            if (!native.selectLayer(layerIndex)) continue
            val layerJson = JSONObject().put("index", layerIndex)
            native.layerRecord(layerIndex)?.let {
                layerJson.put("name", it.name)
                layerJson.put("visible", it.visible)
                layerJson.put("locked", it.locked)
                layerJson.put("opacity", num(it.opacity))
            }
            val frames = JSONArray()
            for (frameNumber in native.frameNumbers()) {
                if (!native.selectFrame(frameNumber)) continue
                val strokes = JSONArray()
                for (strokeIndex in 0 until native.strokeCount()) {
                    val stroke = native.strokeRecord(strokeIndex) ?: continue
                    if (stroke.points.isNotEmpty()) strokes.put(strokeJson(stroke))
                }
                frames.put(JSONObject().put("number", frameNumber).put("strokes", strokes))
            }
            layerJson.put("frames", frames)
            val modifiers = (0 until native.modifierCount(layerIndex)).mapNotNull { native.modifierRecord(layerIndex, it) }
            if (modifiers.isNotEmpty()) layerJson.put("modifiers", ModifierStackJson.toJson(modifiers))
            val masks = native.layerMasks(layerIndex)
            if (masks.isNotEmpty() || native.layerUseMask(layerIndex)) {
                layerJson.put("useMask", native.layerUseMask(layerIndex))
                layerJson.put("masks", JSONArray().apply {
                    for (mask in masks) put(JSONObject().put("name", mask.name).put("hidden", mask.hidden).put("invert", mask.inverted))
                })
            }
            layers.put(layerJson)
        }
        root.put("layers", layers)
        return root.toString()
    }

    fun parse(raw: String): ParsedDocument? {
        val root = runCatching { JSONObject(raw) }.getOrNull() ?: return null
        val layersJson = root.optJSONArray("layers")
        val layers = layersJson?.let { array ->
            (0 until array.length()).mapNotNull { array.optJSONObject(it)?.let(::parseLayer) }
        }
        val materialsJson = root.optJSONArray("materials")
        val materials = materialsJson?.let { array ->
            (0 until array.length()).mapNotNull { array.optJSONObject(it)?.let(::parseMaterial) }
        } ?: emptyList()
        return ParsedDocument(
            version = root.optInt("version", 1),
            width = if (root.has("width")) root.optInt("width") else null,
            height = if (root.has("height")) root.optInt("height") else null,
            fps = if (root.has("fps")) root.optInt("fps") else null,
            frame = root.optInt("frame", 1),
            materials = materials,
            layers = layers
        )
    }

    /**
     * Rebuilds the palette, layers, frames and strokes in an empty document (layer 0 and
     * material 0 already exist). [legacyThickness] is used for strokes saved without a style.
     */
    fun restore(parsed: ParsedDocument, native: DocumentNative, legacyThickness: Float): Boolean {
        parsed.materials.forEachIndexed { index, material ->
            while (native.materialCount() <= index) {
                if (!native.createMaterial()) return false
            }
            if (!native.applyMaterialRecord(index, material)) return false
        }
        val layers = parsed.layers ?: return true
        for ((layerIndex, layer) in layers.withIndex()) {
            if (layerIndex > 0 && !native.createLayer("Layer " + (layerIndex + 1))) return false
            if (!native.selectLayer(layerIndex)) return false
            for ((frameIndex, frame) in layer.frames.withIndex()) {
                val number = frame.number.coerceAtLeast(1)
                if (frameIndex == 0) {
                    if (!native.createFrame(number) && !native.selectFrame(number)) return false
                } else if (!native.createFrame(number)) {
                    return false
                }
                if (!native.selectFrame(number)) return false
                for (stroke in frame.strokes) {
                    if (stroke.points.isEmpty()) continue
                    val styled = if (stroke.hasStyle) stroke else StrokeRecord(stroke.points, 0, legacyThickness)
                    if (!native.addStroke(styled)) return false
                }
            }
            layer.record?.let { if (!native.applyLayerRecord(layerIndex, it)) return false }
            for (modifier in layer.modifiers) {
                if (!native.addModifier(layerIndex, modifier)) return false
            }
        }
        // Masks name other layers, so they are restored once every layer exists and has its name.
        for ((layerIndex, layer) in layers.withIndex()) {
            if (layer.useMask || layer.masks.isNotEmpty()) {
                if (!native.restoreLayerMasks(layerIndex, layer.useMask, layer.masks)) return false
            }
        }
        return true
    }

    private fun parseLayer(json: JSONObject): ParsedLayer {
        val frames = json.optJSONArray("frames")?.let { array ->
            (0 until array.length()).mapNotNull { array.optJSONObject(it)?.let(::parseFrame) }
        } ?: emptyList()
        val record = if (json.has("name") || json.has("visible") || json.has("locked") || json.has("opacity")) {
            LayerRecord(
                name = json.optString("name", ""),
                visible = json.optBoolean("visible", true),
                locked = json.optBoolean("locked", false),
                opacity = json.optDouble("opacity", 1.0).toFloat()
            )
        } else null
        val masks = json.optJSONArray("masks")?.let { array ->
            (0 until array.length()).mapNotNull { index ->
                array.optJSONObject(index)?.takeIf { it.optString("name", "").isNotEmpty() }?.let {
                    MaskRecord(it.getString("name"), it.optBoolean("hidden", false), it.optBoolean("invert", false))
                }
            }
        } ?: emptyList()
        return ParsedLayer(record, frames, ModifierStackJson.fromJson(json.optJSONArray("modifiers")),
            json.optBoolean("useMask", false), masks)
    }

    private fun parseFrame(json: JSONObject): ParsedFrame {
        val strokes = json.optJSONArray("strokes")?.let { array ->
            (0 until array.length()).mapNotNull { array.optJSONObject(it)?.let(::parseStroke) }
        } ?: emptyList()
        return ParsedFrame(json.optInt("number", 1), strokes)
    }

    private fun parseStroke(json: JSONObject): StrokeRecord? {
        val pointsJson = json.optJSONArray("points") ?: return null
        val points = (0 until pointsJson.length()).mapNotNull { index ->
            // Missing vertex-color values (files before version 3) read as 0 = no vertex color.
            pointsJson.optJSONArray(index)?.let { a ->
                FloatArray(StrokeRecord.POINT_SIZE) { a.optDouble(it, 0.0).toFloat() }
            }
        }
        if (points.isEmpty()) return null
        return StrokeRecord(
            points = points,
            materialIndex = json.optInt("material", 0).coerceAtLeast(0),
            thickness = json.optDouble("thickness", 1.0).toFloat(),
            cyclic = json.optBoolean("cyclic", false),
            fillOpacity = json.optDouble("fillOpacity", 1.0).toFloat(),
            fillColor = floats(json.optJSONArray("fillColor"), 4),
            hasStyle = json.has("thickness")
        )
    }

    private fun parseMaterial(json: JSONObject) = MaterialRecord(
        stroke = floats(json.optJSONArray("stroke"), 4, 1f),
        fill = floats(json.optJSONArray("fill"), 4, 1f),
        visible = json.optBoolean("visible", true),
        fillEnabled = json.optBoolean("fillEnabled", false)
    )

    private fun strokeJson(stroke: StrokeRecord): JSONObject {
        val points = JSONArray()
        for (p in stroke.points) {
            // Points without vertex color (the usual case) keep the compact 6-value form.
            val hasColor = (6 until StrokeRecord.POINT_SIZE).any { p.getOrElse(it) { 0f } != 0f }
            val size = if (hasColor) StrokeRecord.POINT_SIZE else 6
            points.put(JSONArray().apply { for (i in 0 until size) put(num(p.getOrElse(i) { 0f })) })
        }
        return JSONObject()
            .put("points", points)
            .put("material", stroke.materialIndex)
            .put("thickness", num(stroke.thickness))
            .put("cyclic", stroke.cyclic)
            .put("fillOpacity", num(stroke.fillOpacity))
            .put("fillColor", floatsJson(stroke.fillColor))
    }

    private fun materialJson(material: MaterialRecord) = JSONObject()
        .put("stroke", floatsJson(material.stroke))
        .put("fill", floatsJson(material.fill))
        .put("visible", material.visible)
        .put("fillEnabled", material.fillEnabled)

    private fun floatsJson(values: FloatArray) = JSONArray().apply { for (v in values) put(num(v)) }

    private fun floats(array: JSONArray?, size: Int, default: Float = 0f) =
        FloatArray(size) { array?.optDouble(it, default.toDouble())?.toFloat() ?: default }

    /** JSON cannot hold NaN/Infinity; a corrupt value is saved as 0 instead of failing the save. */
    private fun num(v: Float): Double = if (v.isNaN() || v.isInfinite()) 0.0 else v.toDouble()
}
