package com.smitnk.projectgrease.editor

/**
 * How Android touch input maps onto Blender's mouse/tablet model for drawing.
 *
 * Blender's mouse always reports pressure 1.0 and only a tablet pen reports real pressure
 * (gpencil_draw_apply_event(): `p->pressure = event->tablet.pressure`). Finger "pressure" on
 * Android is device-specific contact-area noise, so it must not drive stroke width.
 */
object TouchInputRules {
    /** Smallest pressure sent to the native stroke buffer (Blender's GPENCIL_ALPHA_OPACITY_THRESH). */
    const val MIN_NATIVE_PRESSURE = 0.001f

    fun pressureFor(isPen: Boolean, rawPressure: Float): Float =
        if (isPen) rawPressure.coerceIn(0f, 1f) else 1f

    /** Event time relative to the start of the gesture, as seconds (precise even after days of uptime). */
    fun elapsedSeconds(eventTimeMs: Long, gestureStartMs: Long): Float =
        (eventTimeMs - gestureStartMs).coerceAtLeast(0L) / 1000f
}
