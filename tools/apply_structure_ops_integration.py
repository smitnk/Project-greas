#!/usr/bin/env python3
"""Wire duplicate / dissolve (3 types) / split / join into Kotlin and the Advanced sheet (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (B / "editor/BlenderSelectRules.kt", "CMD_DUPLICATE", [(
        "        if (finite(grid) && grid > 0f) Command(CMD_SNAP_GRID, floatArrayOf(grid)) else null\n}\n",
        "        if (finite(grid) && grid > 0f) Command(CMD_SNAP_GRID, floatArrayOf(grid)) else null\n\n"
        "    // GPENCIL_OT_duplicate / dissolve / stroke_split / stroke_join\n"
        "    const val CMD_DUPLICATE = 51\n    const val CMD_DISSOLVE = 52\n    const val CMD_SPLIT = 53\n    const val CMD_JOIN = 54\n"
        "    const val DISSOLVE_POINTS = 0\n    const val DISSOLVE_BETWEEN = 1\n    const val DISSOLVE_UNSELECT = 2\n\n"
        "    fun duplicate() = Command(CMD_DUPLICATE, FloatArray(0))\n"
        "    fun dissolve(type: Int): Command? =\n"
        "        if (type in DISSOLVE_POINTS..DISSOLVE_UNSELECT) Command(CMD_DISSOLVE, floatArrayOf(type.toFloat())) else null\n"
        "    fun split() = Command(CMD_SPLIT, FloatArray(0))\n"
        "    fun join(leaveGaps: Boolean = false) = Command(CMD_JOIN, floatArrayOf(flag(leaveGaps)))\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun duplicateSelection", [(
        "    fun snapSelectionToGrid() = runSelectCommand(ProjectGreaseSelect.snapToGrid(view.gridSize))\n",
        "    fun snapSelectionToGrid() = runSelectCommand(ProjectGreaseSelect.snapToGrid(view.gridSize))\n"
        "    fun duplicateSelection() = runSelectCommand(ProjectGreaseSelect.duplicate())\n"
        "    fun dissolveSelection(type:Int=ProjectGreaseSelect.DISSOLVE_POINTS) = runSelectCommand(ProjectGreaseSelect.dissolve(type))\n"
        "    fun splitSelection() = runSelectCommand(ProjectGreaseSelect.split())\n"
        "    fun joinSelection(leaveGaps:Boolean=false) = runSelectCommand(ProjectGreaseSelect.join(leaveGaps))\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "duplicateSelection", [(
        '            "Snap to grid" to { controller.snapSelectionToGrid() }\n',
        '            "Snap to grid" to { controller.snapSelectionToGrid() },\n'
        '            "Duplicate selection" to { controller.duplicateSelection() },\n'
        '            "Dissolve points" to { controller.dissolveSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.DISSOLVE_POINTS) },\n'
        '            "Dissolve between" to { controller.dissolveSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.DISSOLVE_BETWEEN) },\n'
        '            "Dissolve unselected" to { controller.dissolveSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.DISSOLVE_UNSELECT) },\n'
        '            "Split selection" to { controller.splitSelection() },\n'
        '            "Join selected strokes" to { controller.joinSelection() }\n',
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
