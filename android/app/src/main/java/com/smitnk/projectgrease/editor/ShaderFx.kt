package com.smitnk.projectgrease.editor

import org.json.JSONArray
import org.json.JSONObject

/**
 * Per-layer shader effects: a 2D post-pass over the rendered layer (project_grease_shader_fx.h).
 * Types are Blender's ShaderFxType values; parameters are a flat float array per type whose layout
 * mirrors the PG_FXP_* enums of that header (a unit test compares the two). Sizes are canvas pixels.
 */
object FxType {
    const val BLUR = 1
    const val FLIP = 2
    const val PIXEL = 4
    const val SWIRL = 5
    const val WAVE = 6
    const val RIM = 7
    const val COLORIZE = 8
    const val SHADOW = 9
    const val GLOW = 10
    const val MAX_PARAMS = 24
    const val MAX_STACK = 16

    /** Add-menu order. */
    val all: List<Int> = listOf(COLORIZE, PIXEL, FLIP, WAVE, SWIRL, SHADOW, RIM, BLUR, GLOW)

    fun isValid(type: Int) = type in all

    fun name(type: Int) = when (type) {
        BLUR -> "Blur"
        FLIP -> "Flip"
        PIXEL -> "Pixelate"
        SWIRL -> "Swirl"
        WAVE -> "Wave Distortion"
        RIM -> "Rim"
        COLORIZE -> "Colorize"
        SHADOW -> "Shadow"
        GLOW -> "Glow"
        else -> "Effect"
    }
}

object FxSpecs {
    private const val DEG = 57.29578f
    private const val PI = 3.1415927f

    private fun f(i: Int, label: String, min: Float, max: Float, display: Float = 1f) =
        ParamSpec(i, label, ParamKind.FLOAT, min, max, displayFactor = display)
    private fun n(i: Int, label: String, min: Float, max: Float) = ParamSpec(i, label, ParamKind.INT, min, max)
    private fun b(i: Int, label: String) = ParamSpec(i, label, ParamKind.BOOL, 0f, 1f)
    private fun e(i: Int, label: String, vararg options: String) =
        ParamSpec(i, label, ParamKind.ENUM, 0f, (options.size - 1).toFloat(), options.toList())

    /** Number of parameters per type: PG_FXP_*_COUNT in project_grease_shader_fx.h. */
    fun paramCount(type: Int) = when (type) {
        FxType.BLUR -> 4
        FxType.COLORIZE -> 8
        FxType.FLIP -> 2
        FxType.GLOW -> 15
        FxType.PIXEL -> 3
        FxType.RIM -> 12
        FxType.SHADOW -> 17
        FxType.SWIRL -> 4
        FxType.WAVE -> 4
        else -> 0
    }

    /** Blender's initData() defaults, as native pg_fx_defaults() returns them. */
    fun defaults(type: Int): FloatArray {
        val p = FloatArray(paramCount(type))
        when (type) {
            FxType.BLUR -> { p[0] = 50f; p[1] = 50f; p[2] = 8f }
            FxType.COLORIZE -> { p[4] = 1f; p[5] = 1f; p[6] = 1f; p[7] = 0.5f }
            FxType.FLIP -> p[0] = 1f
            FxType.GLOW -> { p[0] = 0.75f; p[1] = 1f; p[2] = 1f; p[3] = 1f; p[7] = 0.1f; p[9] = 50f; p[10] = 50f; p[11] = 8f }
            FxType.PIXEL -> { p[0] = 5f; p[1] = 5f }
            FxType.RIM -> { p[0] = 50f; p[1] = -100f; p[2] = 1f; p[3] = 1f; p[4] = 0.5f; p[8] = 1f; p[11] = 2f }
            FxType.SHADOW -> {
                p[0] = 15f; p[1] = 20f; p[5] = 0.8f; p[7] = 10f; p[8] = 20f; p[10] = 1f
                p[11] = 1f; p[12] = 1f; p[14] = 5f; p[15] = 5f; p[16] = 2f
            }
            FxType.SWIRL -> { p[0] = 0.5f; p[1] = 0.5f; p[2] = 100f; p[3] = PI / 2f }
            FxType.WAVE -> { p[0] = 10f; p[1] = 20f; p[3] = 1f }
        }
        return p
    }

    private val blendModes = arrayOf("Regular", "Overlay", "Add", "Subtract", "Multiply", "Divide")

    /**
     * The parameters the UI offers. Not offered: Blur's depth-of-field mode and Shadow's object
     * pivot (no camera or objects in a 2D canvas).
     */
    fun specs(type: Int): List<ParamSpec> = when (type) {
        FxType.BLUR -> listOf(
            f(0, "Radius X", 0f, 300f), f(1, "Radius Y", 0f, 300f), n(2, "Samples", 0f, 32f),
            f(3, "Rotation", -PI, PI, DEG)
        )
        FxType.COLORIZE -> listOf(
            e(0, "Mode", "Grayscale", "Sepia", "Duotone", "Custom", "Transparent"),
            f(1, "Low red", 0f, 1f), f(2, "Low green", 0f, 1f), f(3, "Low blue", 0f, 1f),
            f(4, "High red", 0f, 1f), f(5, "High green", 0f, 1f), f(6, "High blue", 0f, 1f),
            f(7, "Factor", 0f, 1f)
        )
        FxType.FLIP -> listOf(b(0, "Horizontal"), b(1, "Vertical"))
        FxType.GLOW -> listOf(
            e(8, "Mode", "Luminance", "Color"), f(7, "Threshold", 0f, 1f),
            f(4, "Select red", 0f, 1f), f(5, "Select green", 0f, 1f), f(6, "Select blue", 0f, 1f),
            f(0, "Glow red", 0f, 1f), f(1, "Glow green", 0f, 1f), f(2, "Glow blue", 0f, 1f),
            f(3, "Opacity", 0f, 1f), b(14, "Use alpha"),
            f(9, "Blur X", 0f, 300f), f(10, "Blur Y", 0f, 300f), n(11, "Samples", 0f, 32f),
            f(12, "Rotation", -PI, PI, DEG), e(13, "Blend", *blendModes)
        )
        FxType.PIXEL -> listOf(n(0, "Size X", 1f, 200f), n(1, "Size Y", 1f, 200f), b(2, "Nearest filter"))
        FxType.RIM -> listOf(
            n(0, "Offset X", -500f, 500f), n(1, "Offset Y", -500f, 500f),
            f(2, "Rim red", 0f, 1f), f(3, "Rim green", 0f, 1f), f(4, "Rim blue", 0f, 1f),
            f(5, "Mask red", 0f, 1f), f(6, "Mask green", 0f, 1f), f(7, "Mask blue", 0f, 1f),
            e(8, "Mode", *blendModes), n(9, "Blur X", 0f, 300f), n(10, "Blur Y", 0f, 300f),
            n(11, "Samples", 0f, 32f)
        )
        FxType.SHADOW -> listOf(
            n(0, "Offset X", -500f, 500f), n(1, "Offset Y", -500f, 500f),
            f(2, "Shadow red", 0f, 1f), f(3, "Shadow green", 0f, 1f), f(4, "Shadow blue", 0f, 1f),
            f(5, "Opacity", 0f, 1f), f(13, "Rotation", -PI, PI, DEG),
            f(11, "Scale X", 0.01f, 5f), f(12, "Scale Y", 0.01f, 5f),
            n(14, "Blur X", 0f, 300f), n(15, "Blur Y", 0f, 300f), n(16, "Samples", 0f, 32f),
            b(6, "Wave"), f(7, "Amplitude", -200f, 200f), f(8, "Period", 0f, 300f),
            f(9, "Phase", -PI * 2, PI * 2, DEG), e(10, "Orientation", "Horizontal", "Vertical")
        )
        FxType.SWIRL -> listOf(
            f(0, "Center X", 0f, 1f), f(1, "Center Y", 0f, 1f), n(2, "Radius", 0f, 2000f),
            f(3, "Angle", -PI * 4, PI * 4, DEG)
        )
        FxType.WAVE -> listOf(
            f(0, "Amplitude", -200f, 200f), f(1, "Period", 0f, 300f), f(2, "Phase", -PI * 2, PI * 2, DEG),
            e(3, "Orientation", "Horizontal", "Vertical")
        )
        else -> emptyList()
    }

    /** What the effect does and the units it uses, shown under the list. */
    fun note(type: Int) = when (type) {
        FxType.BLUR, FxType.GLOW -> "Sizes are canvas pixels."
        FxType.SWIRL -> "Center is a fraction of the canvas."
        else -> ""
    }
}

/** One effect of a layer's list as saved and as read back from the native document. */
class FxRecord(val type: Int, val enabled: Boolean, val params: FloatArray)

/** What the effect commands need from the native document (implemented by NativeEditorBridge). */
interface FxNative {
    fun fxCount(layer: Int): Int
    /** Index of the new effect or -1. */
    fun fxAdd(layer: Int, type: Int): Int
    fun fxRemove(layer: Int, index: Int): Boolean
    fun fxMove(layer: Int, from: Int, to: Int): Boolean
    fun fxSetEnabled(layer: Int, index: Int, enabled: Boolean): Boolean
    fun fxSetParams(layer: Int, index: Int, params: FloatArray): Boolean
    /** [type, enabled, param0, param1, ...] or null. */
    fun fxGet(layer: Int, index: Int): FloatArray?
}

/** Packing between [FxRecord] and the float arrays that cross JNI, like [ModifierStackPacking]. */
object FxPacking {
    fun pack(record: FxRecord): FloatArray {
        val count = FxSpecs.paramCount(record.type)
        val out = FloatArray(2 + count)
        out[0] = record.type.toFloat()
        out[1] = if (record.enabled) 1f else 0f
        for (i in 0 until count) out[2 + i] = record.params.getOrElse(i) { 0f }
        return out
    }

    fun unpack(packed: FloatArray?): FxRecord? {
        if (packed == null || packed.size < 2) return null
        val type = packed[0].toInt()
        if (!FxType.isValid(type)) return null
        val count = FxSpecs.paramCount(type)
        return FxRecord(type, packed[1] != 0f, FloatArray(count) { packed.getOrElse(2 + it) { 0f } })
    }

    /** The params array sent to nativeFxSetParams: exactly the type's parameter count. */
    fun paramsFor(type: Int, params: FloatArray): FloatArray =
        FloatArray(FxSpecs.paramCount(type)) { params.getOrElse(it) { 0f } }
}

/** Effect commands; each returns true when the native call succeeded. */
object FxCommands {
    fun add(native: FxNative, layer: Int, type: Int): Int =
        if (FxType.isValid(type)) native.fxAdd(layer, type) else -1

    fun remove(native: FxNative, layer: Int, index: Int) = native.fxRemove(layer, index)

    /** Moves by [delta] places (-1 = up the list); false at the ends. */
    fun moveBy(native: FxNative, layer: Int, index: Int, delta: Int): Boolean {
        val to = index + delta
        if (to < 0 || to >= native.fxCount(layer)) return false
        return native.fxMove(layer, index, to)
    }

    fun setEnabled(native: FxNative, layer: Int, index: Int, enabled: Boolean) =
        native.fxSetEnabled(layer, index, enabled)

    /** Writes one parameter (clamped to its spec range) keeping the others. */
    fun setParam(native: FxNative, layer: Int, index: Int, paramIndex: Int, value: Float): Boolean {
        val current = FxPacking.unpack(native.fxGet(layer, index)) ?: return false
        if (paramIndex !in current.params.indices || !value.isFinite()) return false
        val spec = FxSpecs.specs(current.type).firstOrNull { it.index == paramIndex }
        val params = current.params.copyOf()
        params[paramIndex] = if (spec != null) value.coerceIn(spec.min, spec.max) else value
        return native.fxSetParams(layer, index, FxPacking.paramsFor(current.type, params))
    }

    fun list(native: FxNative, layer: Int): List<FxRecord> =
        (0 until native.fxCount(layer)).mapNotNull { FxPacking.unpack(native.fxGet(layer, it)) }
}

/** The list as stored in a project file (per layer, key "effects"). */
object FxJson {
    fun toJson(records: List<FxRecord>): JSONArray {
        val array = JSONArray()
        for (record in records) {
            array.put(
                JSONObject().put("type", record.type).put("enabled", record.enabled)
                    .put("params", JSONArray().apply { for (v in record.params) put(if (v.isFinite()) v.toDouble() else 0.0) })
            )
        }
        return array
    }

    /** Unknown types and malformed entries are dropped; a missing key (old files) is an empty list. */
    fun fromJson(array: JSONArray?): List<FxRecord> {
        if (array == null) return emptyList()
        val out = ArrayList<FxRecord>()
        for (i in 0 until array.length()) {
            val json = array.optJSONObject(i) ?: continue
            val type = json.optInt("type", 0)
            if (!FxType.isValid(type) || out.size >= FxType.MAX_STACK) continue
            val values = json.optJSONArray("params")
            val defaults = FxSpecs.defaults(type)
            out.add(
                FxRecord(
                    type,
                    json.optBoolean("enabled", true),
                    // a missing value keeps Blender's default rather than zero (zero is a valid, different value)
                    FloatArray(defaults.size) { values?.optDouble(it, defaults[it].toDouble())?.toFloat() ?: defaults[it] }
                )
            )
        }
        return out
    }
}
