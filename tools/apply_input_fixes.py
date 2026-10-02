#!/usr/bin/env python3
"""Patch the Android touch handler for Blender-faithful drawing input (idempotent).

Edits android/app/src/main/java/com/smitnk/projectgrease/nativebridge/ProjectGreaseEglSurface.kt with
exact-anchor replacements; fails loudly (changing nothing) if an anchor is missing or ambiguous.

  * pressure: only a pen reports pressure (Blender's mouse is always 1.0)
  * every batched touch sample (MotionEvent history) becomes an input event
  * event time is relative to the gesture start (a float of uptime loses precision)
  * no extra point is added on pointer-up (Blender adds nothing on release)
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PATH = ROOT / "android/app/src/main/java/com/smitnk/projectgrease/nativebridge/ProjectGreaseEglSurface.kt"

EDITS = [
    (
        "move: history samples",
        """                        strokeOpen -> {
                            addPoint(event, pointerIndex)
                            controller.render()
                        }
""",
        """                        strokeOpen -> {
                            addPointsWithHistory(event, pointerIndex)
                            controller.render()
                        }
""",
    ),
    (
        "pointer up: no extra point",
        """                        } else {
                            addPoint(event, event.actionIndex)
                            controller.endStroke()
                        }
""",
        """                        } else {
                            controller.endStroke()
                        }
""",
    ),
    (
        "up: no extra point",
        """                    if (pointerIndex >= 0) {
                        addPoint(event, pointerIndex)
                        controller.endStroke()
                    } else {
""",
        """                    if (pointerIndex >= 0) {
                        controller.endStroke()
                    } else {
""",
    ),
    (
        "addPoint",
        """    private fun addPoint(event: MotionEvent, pointerIndex: Int) {
        val pressure = event.getPressure(pointerIndex).coerceIn(0f, 1f)
        val p = canvasPoint(event.getX(pointerIndex), event.getY(pointerIndex))
        controller.addStrokePoint(
            p.first,
            p.second,
            pressure,
            event.eventTime.toFloat() / 1000f
        )
    }
""",
        """    private fun addPoint(event: MotionEvent, pointerIndex: Int) {
        addSample(
            event.getX(pointerIndex),
            event.getY(pointerIndex),
            event.getPressure(pointerIndex),
            event.getToolType(pointerIndex),
            event.eventTime,
            event.downTime
        )
    }

    /** Android batches touch samples between frames; each one is a real input event. */
    private fun addPointsWithHistory(event: MotionEvent, pointerIndex: Int) {
        for (h in 0 until event.historySize) {
            addSample(
                event.getHistoricalX(pointerIndex, h),
                event.getHistoricalY(pointerIndex, h),
                event.getHistoricalPressure(pointerIndex, h),
                event.getToolType(pointerIndex),
                event.getHistoricalEventTime(h),
                event.downTime
            )
        }
        addPoint(event, pointerIndex)
    }

    private fun addSample(
        rawX: Float,
        rawY: Float,
        rawPressure: Float,
        toolType: Int,
        eventTimeMs: Long,
        gestureStartMs: Long
    ) {
        val isPen = toolType == MotionEvent.TOOL_TYPE_STYLUS ||
            toolType == MotionEvent.TOOL_TYPE_ERASER
        val p = canvasPoint(rawX, rawY)
        controller.addStrokePoint(
            p.first,
            p.second,
            com.smitnk.projectgrease.editor.TouchInputRules.pressureFor(isPen, rawPressure),
            com.smitnk.projectgrease.editor.TouchInputRules.elapsedSeconds(eventTimeMs, gestureStartMs)
        )
    }
""",
    ),
]


def main():
    text = PATH.read_text(encoding="utf-8")
    if "addPointsWithHistory" in text:
        print("already applied")
        return 0
    for label, old, new in EDITS:
        count = text.count(old)
        if count != 1:
            print(f"anchor '{label}' found {count} times (expected 1); nothing changed", file=sys.stderr)
            return 1
        text = text.replace(old, new)
    PATH.write_text(text, encoding="utf-8")
    print(f"patched: {PATH.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
