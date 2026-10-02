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
    const val LAST = 10
    const val MAX_PARAMS = 24
    const val MAX_STACK = 32

    fun isValid(type: Int) = type in THICKNESS..LAST

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

    /** Number of parameters per type: PG_P_*_COUNT in project_grease_modifier_stack.h. */
    fun paramCount(type: Int) = when (type) {
        ModifierType.THICKNESS -> 3
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
            b(0, "Normalize thickness"), n(1, "Thickness", 0f, 500f), f(2, "Factor", 0f, 5f)
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
        else -> emptyList()
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
        val spec = ModifierSpecs.specs(current.type).firstOrNull { it.index == paramIndex }
        val params = current.params.copyOf()
        params[paramIndex] = if (spec != null) value.coerceIn(spec.min, spec.max) else value
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
                    .put("params", JSONArray().apply { for (v in record.params) put(if (v.isFinite()) v.toDouble() else 0.0) })
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
