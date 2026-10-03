package com.smitnk.projectgrease.editor

import com.smitnk.projectgrease.editor.LegacyGpBrushStrokeEngine.InputEvent
import com.smitnk.projectgrease.editor.LegacyGpBrushStrokeEngine.Settings
import java.io.File
import kotlin.math.cos
import kotlin.math.sin

/**
 * Golden data for the native Draw input engine (native/blender_gp/project_grease_draw_input.c):
 * runs the former Kotlin LegacyGpBrushStrokeEngine (kept here as the reference) over every
 * scenario of its old unit tests plus wider ones, and writes the points released per call.
 * usage: tools/draw_input_golden/generate.sh  (writes native/blender_gp/tests/draw_input_golden.txt)
 */
private fun line(count: Int, stepX: Float, y: Float = 100f, pressure: Float = 1f) =
    (0 until count).map { InputEvent(stepX * it, y, pressure, 0.016f * it) }

private fun fmt(f: Float) = java.lang.Float.floatToRawIntBits(f).toString()
private fun b(v: Boolean) = if (v) "1" else "0"

fun main(args: Array<String>) {
    val wave = (0 until 40).map {
        val t = it * 0.25f
        InputEvent(30f * t + 7f * sin(3f * t), 200f + 60f * sin(t), 0.2f + 0.8f * (0.5f + 0.5f * cos(2f * t)), 0.012f * it)
    }
    val fastZigzag = (0 until 18).map { InputEvent((it * 55f), if (it % 2 == 0) 0f else 70f, 0.5f + 0.03f * it, 0.02f * it) }
    val curve = (0 until 30).map { InputEvent(300f + 120f * cos(it * 0.3f), 300f + 120f * sin(it * 0.3f), 1f, 0.01f * it) }
    val scenarios = listOf(
        "first_sample" to (Settings() to listOf(InputEvent(10f, 20f, 1f, 5f))),
        "filter" to (Settings(manhattanThreshold = 1, euclideanThreshold = 1f) to listOf(InputEvent(0f, 0f, 1f, 0f), InputEvent(1f, 0f, 1f, 0.01f), InputEvent(2f, 0f, 1f, 0.02f))),
        "lazy" to (Settings(lazyEnabled = true, smoothStrokeRadius = 5f, smoothStrokeFactor = 0.5f) to listOf(InputEvent(0f, 0f, 1f, 0f), InputEvent(10f, 0f, 1f, 0.01f), InputEvent(8f, 0f, 1f, 0.02f), InputEvent(30f, 4f, 0.7f, 0.03f))),
        "arcs4" to (Settings(inputSamples = 4) to (0 until 4).map { InputEvent(40f * it, 0f, 1f, 0.01f * it) }),
        "slow" to (Settings(inputSamples = 4) to (0 until 10).map { InputEvent(10f * it, 5f * it, 1f, 0.016f * it) }),
        "fast_line" to (Settings(inputSamples = 4) to line(12, 40f)),
        "smooth_samples" to (Settings(inputSamples = 4, activeSmooth = 0.5f) to line(20, 33f)),
        "time" to (Settings() to listOf(InputEvent(0f, 0f, 1f, 100f), InputEvent(10f, 0f, 1f, 100.5f))),
        "pressure_one" to (Settings() to line(6, 5f)),
        "pressure_only" to (Settings(drawStrength = 0.8f) to line(6, 5f, pressure = 0.3f)),
        "strength_pressure" to (Settings(drawStrength = 0.5f, useStrengthPressure = true) to line(4, 5f, pressure = 0.5f)),
        "draw_angle" to (Settings(drawAngleFactor = 0.5f, drawAngle = 0f) to listOf(InputEvent(0f, 0f, 1f, 0f), InputEvent(20f, 0f, 0.5f, 0.01f))),
        "active_smooth_line" to (Settings(activeSmooth = 0.65f) to (0 until 20).map { InputEvent(8f * it, 50f, 0.4f + 0.02f * it, 0.016f * it) }),
        "pressure_spike" to (Settings(activeSmooth = 0.5f) to listOf(1f, 1f, 1f, 0.2f, 1f, 1f).mapIndexed { i, p -> InputEvent(10f * i, 0f, p, 0.01f * i) }),
        "no_smooth" to (Settings(activeSmooth = 0f) to line(6, 7f, pressure = 0.5f)),
        "trailing_truncation" to (Settings(inputSamples = 1) to listOf(1f, 1f, 1f, 1f, 1f, 0f).mapIndexed { i, p -> InputEvent(5f * i, 0f, p, 0.01f * i) }),
        "wave_all" to (Settings(drawStrength = 0.7f, useStrengthPressure = true, pressureCurve = 1.6f, strengthCurve = 0.7f, activeSmooth = 0.35f, inputSamples = 7) to wave),
        "zigzag_angle" to (Settings(inputSamples = 10, drawAngleFactor = 0.4f, drawAngle = 0.6f) to fastZigzag),
        "jitter" to (Settings(jitter = 0.3f) to curve),
        "lazy_curve" to (Settings(lazyEnabled = true, smoothStrokeRadius = 12f, smoothStrokeFactor = 0.7f) to curve),
        "lazy_disabled_stabilizer" to (Settings(lazyEnabled = true, disableStabilizer = true, inputSamples = 3, smoothStrokeRadius = 12f, smoothStrokeFactor = 0.7f) to fastZigzag),
        "no_fake_points" to (Settings(inputSamples = 8, synthesizeFastPoints = false) to fastZigzag),
    )
    val out = StringBuilder()
    out.append("# Golden output of the Kotlin LegacyGpBrushStrokeEngine (tools/draw_input_golden). Floats are raw IEEE-754 bits.\n")
    for ((name, sc) in scenarios) {
        val (s, events) = sc
        out.append("scenario ").append(name).append('\n')
        out.append("settings ").append(listOf(fmt(s.drawStrength), b(s.usePressure), b(s.useStrengthPressure), fmt(s.pressureCurve), fmt(s.strengthCurve),
            fmt(s.activeSmooth), s.inputSamples.toString(), b(s.lazyEnabled), fmt(s.smoothStrokeRadius), fmt(s.smoothStrokeFactor), b(s.disableStabilizer),
            s.manhattanThreshold.toString(), fmt(s.euclideanThreshold), fmt(s.jitter), fmt(s.drawAngleFactor), fmt(s.drawAngle), b(s.synthesizeFastPoints)).joinToString(" ")).append('\n')
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(s)
        events.forEachIndexed { i, e ->
            out.append("event ").append(listOf(fmt(e.x), fmt(e.y), fmt(e.pressure), fmt(e.timeSeconds)).joinToString(" ")).append('\n')
            engine.add(e).forEach { p -> out.append("out $i ").append(listOf(fmt(p.x), fmt(p.y), fmt(p.pressure), fmt(p.strength), fmt(p.time)).joinToString(" ")).append('\n') }
        }
        engine.end().forEach { p -> out.append("out -1 ").append(listOf(fmt(p.x), fmt(p.y), fmt(p.pressure), fmt(p.strength), fmt(p.time)).joinToString(" ")).append('\n') }
        out.append("end\n")
    }
    File(args[0]).writeText(out.toString())
}
