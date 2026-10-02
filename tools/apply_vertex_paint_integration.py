#!/usr/bin/env python3
"""Vertex Paint command builder and controller API (idempotent). Touch/UI wiring: see VERTEX_PAINT_WIRING_SPEC.md."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (B / "editor/BlenderSelectRules.kt", "CMD_VERTEX_PAINT", [(
        "    fun join(leaveGaps: Boolean = false) = Command(CMD_JOIN, floatArrayOf(flag(leaveGaps)))\n}\n",
        "    fun join(leaveGaps: Boolean = false) = Command(CMD_JOIN, floatArrayOf(flag(leaveGaps)))\n\n"
        "    // Vertex Paint mode brushes (gpencil_vertex_paint.c)\n"
        "    const val CMD_VERTEX_PAINT = 55\n"
        "    const val VPAINT_DRAW = 0\n    const val VPAINT_BLUR = 1\n    const val VPAINT_AVERAGE = 2\n"
        "    const val VPAINT_SMEAR = 3\n    const val VPAINT_REPLACE = 4\n\n"
        "    fun vertexPaint(brush: Int, x: Float, y: Float, radius: Float, strength: Float,\n"
        "                    r: Float, g: Float, b: Float, target: Int, dx: Float = 0f, dy: Float = 0f): Command? {\n"
        "        if (brush !in VPAINT_DRAW..VPAINT_REPLACE || target !in PAINT_STROKE..PAINT_BOTH) return null\n"
        "        if (!finite(x, y) || !finite(radius, strength) || radius <= 0f || !finite(r, g) || !finite(b) || !finite(dx, dy)) return null\n"
        "        return Command(CMD_VERTEX_PAINT, floatArrayOf(brush.toFloat(), x, y, radius, strength, r, g, b, target.toFloat(), dx, dy))\n"
        "    }\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun vertexPaintDab", [(
        "    fun joinSelection(leaveGaps:Boolean=false) = runSelectCommand(ProjectGreaseSelect.join(leaveGaps))\n",
        "    fun joinSelection(leaveGaps:Boolean=false) = runSelectCommand(ProjectGreaseSelect.join(leaveGaps))\n"
        "    // ---- Vertex Paint mode: one undo step per drag (dabs do not snapshot history) ----\n"
        "    var vertexPaintBrush = ProjectGreaseSelect.VPAINT_DRAW\n        private set\n"
        "    var vertexPaintTarget = ProjectGreaseSelect.PAINT_STROKE\n        private set\n"
        "    private var vertexPaintChanged = false\n"
        "    fun setVertexPaintBrush(brush:Int) { if (brush in ProjectGreaseSelect.VPAINT_DRAW..ProjectGreaseSelect.VPAINT_REPLACE) vertexPaintBrush = brush }\n"
        "    fun setVertexPaintTarget(target:Int) { if (target in ProjectGreaseSelect.PAINT_STROKE..ProjectGreaseSelect.PAINT_BOTH) vertexPaintTarget = target }\n"
        "    fun vertexPaintDab(x:Float, y:Float, dx:Float=0f, dy:Float=0f, pressure:Float=1f):Boolean {\n"
        "        if (native.handle == 0L) return false\n"
        "        val argb = materials.colorArgb\n"
        "        val cmd = ProjectGreaseSelect.vertexPaint(vertexPaintBrush, x, y, brushes.size.coerceAtLeast(1f),\n"
        "            (brushes.strength * pressure).coerceIn(0f, 1f),\n"
        "            ((argb shr 16) and 0xFF) / 255f, ((argb shr 8) and 0xFF) / 255f, (argb and 0xFF) / 255f,\n"
        "            vertexPaintTarget, dx, dy) ?: return false\n"
        "        val changed = native.applyEditCommand(cmd.id, cmd.args)\n"
        "        if (changed) { vertexPaintChanged = true; document.markDirty(); render() }\n"
        "        return changed\n"
        "    }\n"
        "    fun endVertexPaint() { if (vertexPaintChanged) history.markEdit(); vertexPaintChanged = false }\n",
        "controller")]),
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
