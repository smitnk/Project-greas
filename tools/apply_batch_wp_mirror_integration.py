#!/usr/bin/env python3
"""Mirror copies, Weight Paint dab API and easing constants in Kotlin (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (B / "editor/BlenderSelectRules.kt", "CMD_MIRROR_COPY", [(
        "        return Command(CMD_VERTEX_PAINT, floatArrayOf(brush.toFloat(), x, y, radius, strength, r, g, b, target.toFloat(), dx, dy))\n    }\n}\n",
        "        return Command(CMD_VERTEX_PAINT, floatArrayOf(brush.toFloat(), x, y, radius, strength, r, g, b, target.toFloat(), dx, dy))\n    }\n\n"
        "    // Mirror modifier baked as copies, Weight Paint draw, interpolation easing ids\n"
        "    const val CMD_MIRROR_COPY = 56\n    const val CMD_WEIGHT_PAINT = 57\n"
        "    const val EASE_LINEAR = 0\n    const val EASE_QUAD = 1\n    const val EASE_CUBIC = 2\n    const val EASE_QUART = 3\n"
        "    const val EASE_QUINT = 4\n    const val EASE_SINE = 5\n    const val EASE_EXPO = 6\n    const val EASE_CIRC = 7\n"
        "    const val EASE_BACK = 8\n    const val EASE_BOUNCE = 9\n"
        "    const val EASE_IN = 0\n    const val EASE_OUT = 1\n    const val EASE_IN_OUT = 2\n\n"
        "    fun mirrorCopy(axisX: Boolean, axisY: Boolean, pivotX: Float, pivotY: Float): Command? {\n"
        "        if ((!axisX && !axisY) || !finite(pivotX, pivotY)) return null\n"
        "        return Command(CMD_MIRROR_COPY, floatArrayOf(flag(axisX), flag(axisY), pivotX, pivotY))\n"
        "    }\n"
        "    fun weightPaint(group: Int, x: Float, y: Float, radius: Float, strength: Float, weight: Float): Command? {\n"
        "        if (group < 0 || !finite(x, y) || !finite(radius, strength) || !finite(weight) || radius <= 0f) return null\n"
        "        return Command(CMD_WEIGHT_PAINT, floatArrayOf(group.toFloat(), x, y, radius, strength, weight))\n"
        "    }\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun mirrorSelectionCopy", [(
        "    fun endVertexPaint() { if (vertexPaintChanged) history.markEdit(); vertexPaintChanged = false }\n",
        "    fun endVertexPaint() { if (vertexPaintChanged) history.markEdit(); vertexPaintChanged = false }\n"
        "    /** Mirror modifier as copies, about the selection median (Blender uses the object origin). */\n"
        "    fun mirrorSelectionCopy(axisX:Boolean, axisY:Boolean):Boolean {\n"
        "        val pivot = selectionPivot() ?: return false\n"
        "        return runSelectCommand(ProjectGreaseSelect.mirrorCopy(axisX, axisY, pivot[0], pivot[1]))\n"
        "    }\n"
        "    // ---- Weight Paint: one undo step per drag ----\n"
        "    var weightPaintGroup = 0\n        private set\n"
        "    var weightPaintValue = 1f\n        private set\n"
        "    private var weightPaintChanged = false\n"
        "    fun setWeightPaintGroup(group:Int) { if (group >= 0) weightPaintGroup = group }\n"
        "    fun setWeightPaintValue(value:Float) { weightPaintValue = value.coerceIn(0f, 1f) }\n"
        "    fun weightPaintDab(x:Float, y:Float, pressure:Float=1f):Boolean {\n"
        "        if (native.handle == 0L) return false\n"
        "        val cmd = ProjectGreaseSelect.weightPaint(weightPaintGroup, x, y, brushes.size.coerceAtLeast(1f),\n"
        "            (brushes.strength * pressure).coerceIn(0f, 1f), weightPaintValue) ?: return false\n"
        "        val changed = native.applyEditCommand(cmd.id, cmd.args)\n"
        "        if (changed) { weightPaintChanged = true; document.markDirty(); render() }\n"
        "        return changed\n"
        "    }\n"
        "    fun endWeightPaint() { if (weightPaintChanged) history.markEdit(); weightPaintChanged = false }\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "mirrorSelectionCopy", [(
        '            "Join selected strokes" to { controller.joinSelection() }\n',
        '            "Join selected strokes" to { controller.joinSelection() },\n'
        '            "Mirror copy (X)" to { controller.mirrorSelectionCopy(true, false) },\n'
        '            "Mirror copy (Y)" to { controller.mirrorSelectionCopy(false, true) },\n'
        '            "Mirror copy (X+Y)" to { controller.mirrorSelectionCopy(true, true) }\n',
        "ui")]),
]
def main():
    pending=[]
    for path, marker, edits in EDITS:
        text=path.read_text(encoding="utf-8")
        if marker in text: print("already applied:", path.relative_to(ROOT)); continue
        for old,new,label in edits:
            if text.count(old)!=1:
                print(f"anchor '{label}' found {text.count(old)} times; nothing changed", file=sys.stderr); return 1
            text=text.replace(old,new)
        pending.append((path,text))
    for path,text in pending: path.write_text(text,encoding="utf-8"); print("patched:", path.relative_to(ROOT))
    return 0
if __name__=="__main__": sys.exit(main())
