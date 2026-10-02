#!/usr/bin/env python3
"""Wire six Legacy GP stroke operators into Kotlin and the Advanced sheet (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (B / "editor/BlenderSelectRules.kt", "CMD_ARRANGE", [(
        "        return Command(CMD_MOD_COLOR, floatArrayOf(mode.toFloat(), hue, saturation, value))\n    }\n}\n",
        "        return Command(CMD_MOD_COLOR, floatArrayOf(mode.toFloat(), hue, saturation, value))\n    }\n\n"
        "    // Legacy GP stroke operators (stroke_arrange, stroke_change_color, stroke_reset_vertex_color,\n"
        "    // stroke_flip, stroke_cyclical_set, snap_to_grid)\n"
        "    const val CMD_ARRANGE = 45\n    const val CMD_SET_MATERIAL = 46\n    const val CMD_RESET_VCOLOR = 47\n"
        "    const val CMD_FLIP = 48\n    const val CMD_CYCLIC = 49\n    const val CMD_SNAP_GRID = 50\n"
        "    const val ARRANGE_TOP = 0\n    const val ARRANGE_UP = 1\n    const val ARRANGE_DOWN = 2\n    const val ARRANGE_BOTTOM = 3\n"
        "    const val CYCLIC_CLOSE = 1\n    const val CYCLIC_OPEN = 2\n    const val CYCLIC_TOGGLE = 3\n\n"
        "    fun arrange(direction: Int): Command? =\n"
        "        if (direction in ARRANGE_TOP..ARRANGE_BOTTOM) Command(CMD_ARRANGE, floatArrayOf(direction.toFloat())) else null\n"
        "    fun setMaterial(index: Int): Command? =\n"
        "        if (index >= 0) Command(CMD_SET_MATERIAL, floatArrayOf(index.toFloat())) else null\n"
        "    fun resetVertexColor(mode: Int): Command? =\n"
        "        if (mode in PAINT_STROKE..PAINT_BOTH) Command(CMD_RESET_VCOLOR, floatArrayOf(mode.toFloat())) else null\n"
        "    fun flip() = Command(CMD_FLIP, FloatArray(0))\n"
        "    fun cyclic(type: Int): Command? =\n"
        "        if (type in CYCLIC_CLOSE..CYCLIC_TOGGLE) Command(CMD_CYCLIC, floatArrayOf(type.toFloat())) else null\n"
        "    fun snapToGrid(grid: Float): Command? =\n"
        "        if (finite(grid) && grid > 0f) Command(CMD_SNAP_GRID, floatArrayOf(grid)) else null\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun arrangeSelection", [(
        "        runSelectCommand(ProjectGreaseSelect.colorModifier(mode, hue, saturation, value))\n",
        "        runSelectCommand(ProjectGreaseSelect.colorModifier(mode, hue, saturation, value))\n"
        "    fun arrangeSelection(direction:Int) = runSelectCommand(ProjectGreaseSelect.arrange(direction))\n"
        "    fun assignActiveMaterialToSelection() = runSelectCommand(ProjectGreaseSelect.setMaterial(materials.activeMaterial))\n"
        "    fun resetSelectionVertexColor(mode:Int=ProjectGreaseSelect.PAINT_BOTH) = runSelectCommand(ProjectGreaseSelect.resetVertexColor(mode))\n"
        "    fun flipSelection() = runSelectCommand(ProjectGreaseSelect.flip())\n"
        "    fun setSelectionCyclic(type:Int) = runSelectCommand(ProjectGreaseSelect.cyclic(type))\n"
        "    fun snapSelectionToGrid() = runSelectCommand(ProjectGreaseSelect.snapToGrid(view.gridSize))\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "arrangeSelection", [(
        '            "Darken 20%" to { controller.applyColorModifier(value = 0.8f) }\n',
        '            "Darken 20%" to { controller.applyColorModifier(value = 0.8f) },\n'
        '            "Bring to front" to { controller.arrangeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ARRANGE_TOP) },\n'
        '            "Bring forward" to { controller.arrangeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ARRANGE_UP) },\n'
        '            "Send backward" to { controller.arrangeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ARRANGE_DOWN) },\n'
        '            "Send to back" to { controller.arrangeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ARRANGE_BOTTOM) },\n'
        '            "Assign active material" to { controller.assignActiveMaterialToSelection() },\n'
        '            "Reset vertex color" to { controller.resetSelectionVertexColor() },\n'
        '            "Flip direction" to { controller.flipSelection() },\n'
        '            "Toggle closed" to { controller.setSelectionCyclic(com.smitnk.projectgrease.editor.ProjectGreaseSelect.CYCLIC_TOGGLE) },\n'
        '            "Snap to grid" to { controller.snapSelectionToGrid() }\n',
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
