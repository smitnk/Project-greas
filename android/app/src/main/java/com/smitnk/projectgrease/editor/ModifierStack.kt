package com.smitnk.projectgrease.editor

import org.json.JSONArray
import org.json.JSONObject

/**
 * Live (non-destructive) per-layer modifier stack. The stack lives in the native document
 * (project_grease_modifier_stack.h): strokes are only changed by Apply. Parameters are a flat float
 * array per type; the index layout below mirrors the PG_P_* enums of that header (a unit test
 * compares the two so they cannot drift apart).
 */
object ModifierType {
    const val THICKNESS = 1
    const val OPACITY = 2
    const val TINT = 3
    const val COLOR = 4
    const val LENGTH = 5
    const val SMOOTH = 6
    const val SIMPLIFY = 7
    const val SUBDIV = 8
    const val OFFSET = 9
    const val NOISE = 10
    const val BUILD = 11
    const val TIME = 12
    const val HOOK = 13
    const val LATTICE = 14
    const val ENVELOPE = 15
    const val WEIGHT_PROXIMITY = 16
    const val WEIGHT_ANGLE = 17
    const val DASH = 18
    const val OUTLINE = 19
    const val MIRROR = 20
    const val ARRAY = 21
    const val MULTIPLY = 22
    const val LAST = 22
    const val MAX_PARAMS = 120
    const val MAX_STACK = 32
    /** PG_P_CURVE_BASE / PG_P_FILTER_BASE: the custom curve and influence filter blocks of every entry. */
    const val CURVE_BASE = 96
    const val FILTER_BASE = 112
    const val LATTICE_MAX = 6

    fun isValid(type: Int) = type in THICKNESS..LAST
    /** Entries whose handles can be dragged on the canvas (centre / target / grid / point / pivot). */
    fun hasCanvasHandles(type: Int) = type == HOOK || type == LATTICE || type == WEIGHT_PROXIMITY || type == MIRROR

    fun name(type: Int) = when (type) {
        THICKNESS -> "Thickness"
        OPACITY -> "Opacity"
        TINT -> "Tint"
        COLOR -> "Hue/Saturation"
        LENGTH -> "Length"
        SMOOTH -> "Smooth"
        SIMPLIFY -> "Simplify"
        SUBDIV -> "Subdivide"
        OFFSET -> "Offset"
        NOISE -> "Noise"
        BUILD -> "Build"
        TIME -> "Time Offset"
        HOOK -> "Hook"
        LATTICE -> "Lattice"
        ENVELOPE -> "Envelope"
        WEIGHT_PROXIMITY -> "Vertex Weight Proximity"
        WEIGHT_ANGLE -> "Vertex Weight Angle"
        DASH -> "Dot Dash"
        OUTLINE -> "Outline"
        MIRROR -> "Mirror"
        ARRAY -> "Array"
        MULTIPLY -> "Multiple Strokes"
        else -> "Modifier"
    }

    val all: List<Int> = (THICKNESS..LAST).toList()
}

enum class ParamKind { FLOAT, INT, BOOL, ENUM }

/**
 * One editable parameter. [index] is the position in the flat array; [displayFactor] converts the
 * stored value to what the UI shows (radians shown as degrees).
 */
class ParamSpec(
    val index: Int,
    val label: String,
    val kind: ParamKind,
    val min: Float = 0f,
    val max: Float = 1f,
    val options: List<String> = emptyList(),
    val displayFactor: Float = 1f
)

object ModifierSpecs {
    private const val DEG = 57.29578f

    private fun f(i: Int, label: String, min: Float, max: Float, display: Float = 1f) =
        ParamSpec(i, label, ParamKind.FLOAT, min, max, displayFactor = display)
    private fun n(i: Int, label: String, min: Float, max: Float) = ParamSpec(i, label, ParamKind.INT, min, max)
    private fun b(i: Int, label: String) = ParamSpec(i, label, ParamKind.BOOL, 0f, 1f)
    private fun e(i: Int, label: String, vararg options: String) =
        ParamSpec(i, label, ParamKind.ENUM, 0f, (options.size - 1).toFloat(), options.toList())

    /** Stored parameters per entry: MAX_PARAMS for every valid type (own + curve + filter blocks). */
    fun paramCount(type: Int) = if (ModifierType.isValid(type)) ModifierType.MAX_PARAMS else 0

    /** The type's own parameters: PG_P_*_COUNT in project_grease_modifier_stack.h. */
    fun ownParamCount(type: Int) = when (type) {
        ModifierType.BUILD -> 9
        ModifierType.TIME -> 7
        ModifierType.HOOK -> 9
        ModifierType.LATTICE -> 7 + ModifierType.LATTICE_MAX * ModifierType.LATTICE_MAX * 2
        ModifierType.ENVELOPE -> 6
        ModifierType.WEIGHT_PROXIMITY -> 8
        ModifierType.WEIGHT_ANGLE -> 5
        ModifierType.DASH -> 3
        ModifierType.OUTLINE -> 2
        ModifierType.MIRROR -> 4
        ModifierType.ARRAY -> 3
        ModifierType.MULTIPLY -> 2
        ModifierType.THICKNESS -> 6
        ModifierType.OPACITY -> 4
        ModifierType.TINT -> 5
        ModifierType.COLOR -> 4
        ModifierType.LENGTH -> 9
        ModifierType.SMOOTH -> 7
        ModifierType.SIMPLIFY -> 6
        ModifierType.SUBDIV -> 2
        ModifierType.OFFSET -> 23
        ModifierType.NOISE -> 10
        else -> 0
    }

    private val applyTo = arrayOf("Stroke and fill", "Stroke", "Fill")

    /** The parameters the 2D canvas UI offers (Offset/Noise z and x/y rotation stay at 0). */
    fun specs(type: Int): List<ParamSpec> = when (type) {
        ModifierType.THICKNESS -> listOf(
            b(0, "Normalize thickness"), n(1, "Thickness", 0f, 500f), f(2, "Factor", 0f, 5f),
            b(3, "Use vertex group"), n(4, "Vertex group", 0f, 255f), b(5, "Invert vertex group")
        )
        ModifierType.OPACITY -> listOf(
            e(0, "Apply to", "Stroke and fill", "Stroke", "Fill", "Hardness"),
            f(1, "Factor", 0f, 2f), b(2, "Normalize opacity"), f(3, "Hardness", 0f, 1f)
        )
        ModifierType.TINT -> listOf(
            e(0, "Apply to", "Stroke", "Fill", "Stroke and fill"), f(1, "Factor", 0f, 2f),
            f(2, "Red", 0f, 1f), f(3, "Green", 0f, 1f), f(4, "Blue", 0f, 1f)
        )
        ModifierType.COLOR -> listOf(
            e(0, "Apply to", *applyTo), f(1, "Hue", 0f, 1f), f(2, "Saturation", 0f, 2f), f(3, "Value", 0f, 2f)
        )
        ModifierType.LENGTH -> listOf(
            e(0, "Mode", "Relative", "Absolute"), f(1, "Start", -1f, 1f), f(2, "End", -1f, 1f),
            f(3, "Random overshoot", 0f, 1f), b(4, "Use curvature"), f(5, "Point density", 0.1f, 100f),
            f(6, "Segment influence", -2f, 3f), f(7, "Max angle", 0f, 3.1415927f, DEG), b(8, "Invert curvature")
        )
        ModifierType.SMOOTH -> listOf(
            f(0, "Factor", 0f, 2f), n(1, "Repeat", 1f, 30f), b(2, "Position"), b(3, "Strength"),
            b(4, "Thickness"), b(5, "UV"), b(6, "Keep shape")
        )
        ModifierType.SIMPLIFY -> listOf(
            e(0, "Mode", "Fixed", "Adaptive", "Sample", "Merge"), n(1, "Steps", 1f, 30f),
            f(2, "Factor", 0f, 100f), f(3, "Length", 0f, 100f),
            f(4, "Sharp threshold", 0f, 3.1415927f, DEG), f(5, "Distance", 0f, 100f)
        )
        ModifierType.SUBDIV -> listOf(n(0, "Level", 0f, 6f), e(1, "Type", "Catmull-Clark", "Simple"))
        ModifierType.OFFSET -> listOf(
            e(0, "Mode", "Random", "Layer", "Material", "Stroke"),
            f(1, "Location X", -500f, 500f), f(2, "Location Y", -500f, 500f),
            f(6, "Rotation", -3.1415927f, 3.1415927f, DEG),
            f(7, "Scale X", -1f, 1f), f(8, "Scale Y", -1f, 1f),
            f(10, "Random X", -500f, 500f), f(11, "Random Y", -500f, 500f),
            f(15, "Random rotation", -3.1415927f, 3.1415927f, DEG),
            f(16, "Random scale X", -1f, 1f), f(17, "Random scale Y", -1f, 1f),
            n(19, "Seed", 0f, 1000f), n(20, "Step", 1f, 50f), n(21, "Start offset", -50f, 50f),
            b(22, "Uniform random scale")
        )
        ModifierType.NOISE -> listOf(
            f(0, "Position", 0f, 1f), f(1, "Strength", 0f, 1f), f(2, "Thickness", 0f, 1f), f(3, "UV", 0f, 1f),
            f(4, "Noise scale", 0f, 1f), f(5, "Noise offset", 0f, 20f), n(6, "Seed", 0f, 1000f),
            n(7, "Step", 1f, 30f), b(8, "Randomize"), e(9, "Mode", "Steps", "Keyframes")
        )
        ModifierType.BUILD -> listOf(
            e(0, "Mode", "Sequential", "Concurrent", "Additive"), e(1, "Transition", "Grow", "Shrink", "Vanish"),
            f(2, "Delay (frames)", 0f, 250f), f(3, "Length (frames)", 1f, 500f),
            e(4, "Time alignment", "Start", "End"), b(5, "Fade"), f(6, "Fade factor", 0f, 1f),
            f(7, "Fade thickness strength", 0f, 1f), f(8, "Fade opacity strength", 0f, 1f)
        )
        ModifierType.TIME -> listOf(
            e(0, "Mode", "Normal", "Reverse", "Fixed frame", "Ping pong"), n(1, "Frame offset", -250f, 250f),
            f(2, "Frame scale", 0.1f, 10f), b(3, "Custom range"), n(4, "Start frame", 0f, 1000f),
            n(5, "End frame", 0f, 1000f), b(6, "Loop")
        )
        ModifierType.HOOK -> listOf(
            f(0, "Center X", -4000f, 4000f), f(1, "Center Y", -4000f, 4000f),
            f(2, "Offset X", -2000f, 2000f), f(3, "Offset Y", -2000f, 2000f),
            f(4, "Rotation", -3.1415927f, 3.1415927f, DEG), f(5, "Scale", 0f, 4f),
            f(6, "Radius", 0f, 4000f), e(7, "Falloff", "Constant", "Smooth", "Linear"), f(8, "Strength", 0f, 1f)
        )
        ModifierType.LATTICE -> listOf(
            f(0, "Left", -4000f, 4000f), f(1, "Top", -4000f, 4000f), f(2, "Right", -4000f, 4000f),
            f(3, "Bottom", -4000f, 4000f), n(4, "Columns", 2f, 6f), n(5, "Rows", 2f, 6f), f(6, "Strength", 0f, 1f)
        )
        ModifierType.ENVELOPE -> listOf(
            e(0, "Mode", "Deform", "Segments", "Fills"), n(1, "Spread", 1f, 100f), n(2, "Skip", 0f, 20f),
            f(3, "Thickness", 0f, 10f), f(4, "Strength", 0f, 1f), n(5, "Material (-1 same)", -1f, 64f)
        )
        ModifierType.WEIGHT_PROXIMITY -> listOf(
            n(0, "Target vertex group", 0f, 255f), f(1, "Point X", -4000f, 4000f), f(2, "Point Y", -4000f, 4000f),
            f(3, "Lowest distance", 0f, 4000f), f(4, "Highest distance", 0f, 4000f), f(5, "Minimum weight", 0f, 1f),
            b(6, "Invert output"), b(7, "Multiply weights")
        )
        ModifierType.WEIGHT_ANGLE -> listOf(
            n(0, "Target vertex group", 0f, 255f), f(1, "Angle", -3.1415927f, 3.1415927f, DEG),
            f(2, "Minimum weight", 0f, 1f), b(3, "Invert output"), b(4, "Multiply weights")
        )
        ModifierType.DASH -> listOf(n(0, "Dash", 1f, 50f), n(1, "Gap", 1f, 50f), n(2, "Offset", -50f, 50f))
        ModifierType.OUTLINE -> listOf(n(0, "Thickness", 1f, 100f), n(1, "Cap segments", 1f, 32f))
        ModifierType.MIRROR -> listOf(b(0, "Axis X"), b(1, "Axis Y"), f(2, "Pivot X", -4000f, 4000f), f(3, "Pivot Y", -4000f, 4000f))
        ModifierType.ARRAY -> listOf(n(0, "Count", 2f, 50f), f(1, "Offset X", -2000f, 2000f), f(2, "Offset Y", -2000f, 2000f))
        ModifierType.MULTIPLY -> listOf(n(0, "Duplicates", 1f, 20f), f(1, "Distance", -200f, 200f))
        else -> emptyList()
    }

    /** Influence filters every entry has (Blender's Influence panel): +1 encoded, 0 = off. */
    fun filterSpecs(): List<ParamSpec> {
        val k = ModifierType.FILTER_BASE
        return listOf(
            n(k + 0, "Material slot filter (0 off, slot+1)", 0f, 64f), b(k + 1, "Invert material"),
            n(k + 2, "Material pass (0 off)", 0f, 100f), b(k + 3, "Invert material pass"),
            n(k + 4, "Layer pass (0 off)", 0f, 100f), b(k + 5, "Invert layer pass"),
            n(k + 6, "Vertex group (0 off, group+1)", 0f, 64f), b(k + 7, "Invert vertex group")
        )
    }
    /** Custom curve switch (curve_intensity): the points are edited with the curve editor. */
    fun curveUseSpec() = b(ModifierType.CURVE_BASE, "Use custom curve")

    /** The custom curve points stored in [params] (empty when the curve is off or unset). */
    fun curvePoints(params: FloatArray): List<Pair<Float, Float>> {
        val c = ModifierType.CURVE_BASE
        val n = params.getOrElse(c + 1) { 0f }.toInt().coerceIn(0, 7)
        return (0 until n).map { params.getOrElse(c + 2 + 2 * it) { 0f } to params.getOrElse(c + 3 + 2 * it) { 0f } }
    }
    /** Writes the curve points (2..7) into a copy of [params]. */
    fun withCurve(params: FloatArray, use: Boolean, points: List<Pair<Float, Float>>): FloatArray {
        val out = params.copyOf(ModifierType.MAX_PARAMS)
        val c = ModifierType.CURVE_BASE
        for (i in c until c + 16) out[i] = 0f
        out[c] = if (use) 1f else 0f
        val pts = points.take(7)
        out[c + 1] = pts.size.toFloat()
        pts.forEachIndexed { i, (x, y) -> out[c + 2 + 2 * i] = x.coerceIn(0f, 1f); out[c + 3 + 2 * i] = y.coerceIn(0f, 1f) }
        return out
    }

    /** Lattice grid offsets: node (u, v) of an nu x nv grid is at index OFFSETS + (v * nu + u) * 2. */
    fun latticeOffsetIndex(nu: Int, u: Int, v: Int) = 7 + (v * nu + u) * 2

    /**
     * Canvas handles of an entry: Hook centre and target (centre + offset), Lattice grid nodes (rect +
     * offsets), Weight Proximity point, Mirror pivot.
     */
    fun canvasHandles(type: Int, p: FloatArray): List<Pair<Float, Float>> {
        fun v(i: Int) = p.getOrElse(i) { 0f }
        return when (type) {
            ModifierType.HOOK -> listOf(v(0) to v(1), (v(0) + v(2)) to (v(1) + v(3)))
            ModifierType.LATTICE -> {
                val nu = v(4).toInt().coerceIn(2, ModifierType.LATTICE_MAX)
                val nv = v(5).toInt().coerceIn(2, ModifierType.LATTICE_MAX)
                val out = ArrayList<Pair<Float, Float>>()
                for (j in 0 until nv) for (i in 0 until nu) {
                    val x = v(0) + (v(2) - v(0)) * i / (nu - 1)
                    val y = v(1) + (v(3) - v(1)) * j / (nv - 1)
                    val k = latticeOffsetIndex(nu, i, j)
                    out += (x + v(k)) to (y + v(k + 1))
                }
                out
            }
            ModifierType.WEIGHT_PROXIMITY -> listOf(v(1) to v(2))
            ModifierType.MIRROR -> listOf(v(2) to v(3))
            else -> emptyList()
        }
    }

    /** New parameters after handle [handle] of an entry was dragged to (x, y). */
    fun moveHandle(type: Int, p: FloatArray, handle: Int, x: Float, y: Float): FloatArray {
        val out = p.copyOf(ModifierType.MAX_PARAMS)
        when (type) {
            ModifierType.HOOK -> if (handle == 0) {
                out[0] = x; out[1] = y          // the centre moves, the target keeps its place
                out[2] = p[0] + p[2] - x; out[3] = p[1] + p[3] - y
            } else { out[2] = x - p[0]; out[3] = y - p[1] }
            ModifierType.LATTICE -> {
                val nu = p[4].toInt().coerceIn(2, ModifierType.LATTICE_MAX)
                val nv = p[5].toInt().coerceIn(2, ModifierType.LATTICE_MAX)
                if (handle in 0 until nu * nv) {
                    val i = handle % nu; val j = handle / nu
                    val gx = p[0] + (p[2] - p[0]) * i / (nu - 1)
                    val gy = p[1] + (p[3] - p[1]) * j / (nv - 1)
                    val k = latticeOffsetIndex(nu, i, j)
                    out[k] = x - gx; out[k + 1] = y - gy
                }
            }
            ModifierType.WEIGHT_PROXIMITY -> { out[1] = x; out[2] = y }
            ModifierType.MIRROR -> { out[2] = x; out[3] = y }
        }
        return out
    }
}

/** One modifier of a layer's stack as saved and as read back from the native document. */
class ModifierRecord(val type: Int, val enabled: Boolean, val params: FloatArray)

/** What the stack commands need from the native document (implemented by NativeEditorBridge). */
interface ModifierNative {
    fun modifierCount(layer: Int): Int
    /** Index of the new modifier or -1. */
    fun modifierAdd(layer: Int, type: Int): Int
    fun modifierRemove(layer: Int, index: Int): Boolean
    fun modifierMove(layer: Int, from: Int, to: Int): Boolean
    fun modifierSetEnabled(layer: Int, index: Int, enabled: Boolean): Boolean
    fun modifierSetParams(layer: Int, index: Int, params: FloatArray): Boolean
    /** [type, enabled, param0, param1, ...] or null. */
    fun modifierGet(layer: Int, index: Int): FloatArray?
    fun modifierApply(layer: Int, index: Int): Boolean
}

/**
 * Packing between [ModifierRecord] and the float arrays that cross JNI:
 * `[type, enabled, params...]` (nativeModifierGet) and the bare params array (nativeModifierSetParams).
 */
object ModifierStackPacking {
    fun pack(record: ModifierRecord): FloatArray {
        val count = ModifierSpecs.paramCount(record.type)
        val out = FloatArray(2 + count)
        out[0] = record.type.toFloat()
        out[1] = if (record.enabled) 1f else 0f
        for (i in 0 until count) out[2 + i] = record.params.getOrElse(i) { 0f }
        return out
    }

    fun unpack(packed: FloatArray?): ModifierRecord? {
        if (packed == null || packed.size < 2) return null
        val type = packed[0].toInt()
        if (!ModifierType.isValid(type)) return null
        val count = ModifierSpecs.paramCount(type)
        return ModifierRecord(type, packed[1] != 0f, FloatArray(count) { packed.getOrElse(2 + it) { 0f } })
    }

    /** The params array sent to nativeModifierSetParams: exactly the type's parameter count. */
    fun paramsFor(type: Int, params: FloatArray): FloatArray =
        FloatArray(ModifierSpecs.paramCount(type)) { params.getOrElse(it) { 0f } }
}

/** Stack commands; each returns true when the native call succeeded. */
object ModifierStackCommands {
    fun add(native: ModifierNative, layer: Int, type: Int): Int =
        if (ModifierType.isValid(type)) native.modifierAdd(layer, type) else -1

    fun remove(native: ModifierNative, layer: Int, index: Int) = native.modifierRemove(layer, index)

    /** Moves by [delta] places (-1 = up the stack); false at the ends. */
    fun moveBy(native: ModifierNative, layer: Int, index: Int, delta: Int): Boolean {
        val to = index + delta
        if (to < 0 || to >= native.modifierCount(layer)) return false
        return native.modifierMove(layer, index, to)
    }

    fun setEnabled(native: ModifierNative, layer: Int, index: Int, enabled: Boolean) =
        native.modifierSetEnabled(layer, index, enabled)

    /** Writes one parameter (clamped to its spec range) keeping the others. */
    fun setParam(native: ModifierNative, layer: Int, index: Int, paramIndex: Int, value: Float): Boolean {
        val current = ModifierStackPacking.unpack(native.modifierGet(layer, index)) ?: return false
        if (paramIndex !in current.params.indices || !value.isFinite()) return false
        val spec = (ModifierSpecs.specs(current.type) + ModifierSpecs.filterSpecs()).firstOrNull { it.index == paramIndex }
        val params = current.params.copyOf()
        params[paramIndex] = if (spec != null) value.coerceIn(spec.min, spec.max) else value
        return native.modifierSetParams(layer, index, ModifierStackPacking.paramsFor(current.type, params))
    }

    /** Replaces every parameter (custom curve, canvas handles); native sanitizes. */
    fun setAll(native: ModifierNative, layer: Int, index: Int, params: FloatArray): Boolean {
        val current = ModifierStackPacking.unpack(native.modifierGet(layer, index)) ?: return false
        if (params.any { !it.isFinite() }) return false
        return native.modifierSetParams(layer, index, ModifierStackPacking.paramsFor(current.type, params))
    }

    fun apply(native: ModifierNative, layer: Int, index: Int) = native.modifierApply(layer, index)

    fun list(native: ModifierNative, layer: Int): List<ModifierRecord> =
        (0 until native.modifierCount(layer)).mapNotNull { ModifierStackPacking.unpack(native.modifierGet(layer, it)) }
}

/** The stack as stored in a project file (per layer, key "modifiers"). */
object ModifierStackJson {
    fun toJson(records: List<ModifierRecord>): JSONArray {
        val array = JSONArray()
        for (record in records) {
            array.put(
                JSONObject().put("type", record.type).put("enabled", record.enabled)
                    .put("params", JSONArray().apply {
                        // trailing zeros (unused curve / filter blocks) are left out; loading pads them back
                        val last = record.params.indexOfLast { it != 0f && it.isFinite() }
                        for (i in 0..last) { val v = record.params[i]; put(if (v.isFinite()) v.toDouble() else 0.0) }
                    })
            )
        }
        return array
    }

    /** Unknown types and malformed entries are dropped; a missing key (old files) is an empty stack. */
    fun fromJson(array: JSONArray?): List<ModifierRecord> {
        if (array == null) return emptyList()
        val out = ArrayList<ModifierRecord>()
        for (i in 0 until array.length()) {
            val json = array.optJSONObject(i) ?: continue
            val type = json.optInt("type", 0)
            if (!ModifierType.isValid(type) || out.size >= ModifierType.MAX_STACK) continue
            val values = json.optJSONArray("params")
            out.add(
                ModifierRecord(
                    type,
                    json.optBoolean("enabled", true),
                    FloatArray(ModifierSpecs.paramCount(type)) { values?.optDouble(it, 0.0)?.toFloat() ?: 0f }
                )
            )
        }
        return out
    }
}
