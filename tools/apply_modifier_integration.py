#!/usr/bin/env python3
"""Wire the Thickness/Opacity Legacy GP modifiers into Kotlin and the Advanced sheet (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (B / "editor/BlenderSelectRules.kt", "CMD_MOD_THICKNESS", [(
        "    fun deletePoints() = Command(CMD_DELETE_POINTS, FloatArray(0))\n}\n",
        "    fun deletePoints() = Command(CMD_DELETE_POINTS, FloatArray(0))\n\n"
        "    // Legacy GP modifiers baked into the selected strokes (MOD_gpencil_legacy_thick/opacity.c)\n"
        "    const val CMD_MOD_THICKNESS = 40\n"
        "    const val CMD_MOD_OPACITY = 41\n"
        "    const val MODIFY_BOTH = 0\n    const val MODIFY_STROKE = 1\n    const val MODIFY_FILL = 2\n    const val MODIFY_HARDNESS = 3\n\n"
        "    fun thicknessModifier(factor: Float, normalize: Boolean = false, thickness: Int = 0): Command? =\n"
        "        if (finite(factor) && thickness >= 0) Command(CMD_MOD_THICKNESS, floatArrayOf(flag(normalize), thickness.toFloat(), factor)) else null\n\n"
        "    fun opacityModifier(mode: Int, factor: Float, normalize: Boolean = false, hardness: Float = 1f): Command? =\n"
        "        if (mode in MODIFY_BOTH..MODIFY_HARDNESS && finite(factor, hardness))\n"
        "            Command(CMD_MOD_OPACITY, floatArrayOf(mode.toFloat(), factor, flag(normalize), hardness)) else null\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun applyThicknessModifier", [(
        "    fun deleteSelectedPoints() = runSelectCommand(ProjectGreaseSelect.deletePoints()).also { if (it) selection.clear() }\n",
        "    fun deleteSelectedPoints() = runSelectCommand(ProjectGreaseSelect.deletePoints()).also { if (it) selection.clear() }\n"
        "    fun applyThicknessModifier(factor:Float, normalize:Boolean=false, thickness:Int=0) =\n"
        "        runSelectCommand(ProjectGreaseSelect.thicknessModifier(factor, normalize, thickness))\n"
        "    fun applyOpacityModifier(mode:Int, factor:Float, normalize:Boolean=false, hardness:Float=1f) =\n"
        "        runSelectCommand(ProjectGreaseSelect.opacityModifier(mode, factor, normalize, hardness))\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "applyThicknessModifier", [(
        '        listOf("SMOOTH","SIMPLIFY","SUBDIVIDE").forEach { modifier ->\n',
        '        listOf(\n'
        '            "Thickness x0.5" to { controller.applyThicknessModifier(0.5f) },\n'
        '            "Thickness x2" to { controller.applyThicknessModifier(2f) },\n'
        '            "Opacity 50%" to { controller.applyOpacityModifier(com.smitnk.projectgrease.editor.ProjectGreaseSelect.MODIFY_BOTH, 0.5f, normalize = true) },\n'
        '            "Opacity 100%" to { controller.applyOpacityModifier(com.smitnk.projectgrease.editor.ProjectGreaseSelect.MODIFY_BOTH, 1f, normalize = true) }\n'
        '        ).forEach { (label, action) ->\n'
        '            Button(onClick={ if (action()) redraw() }, modifier=Modifier.fillMaxWidth().padding(horizontal=20.dp,vertical=2.dp)){Text(label)}\n'
        '        }\n'
        '        listOf("SMOOTH","SIMPLIFY","SUBDIVIDE").forEach { modifier ->\n',
        "ui")]),
    (B / "editor/FeatureRegistry.kt", "Thickness modifier baked", [(
        'FeatureId.THICKNESS_MODIFIER to missing("Modifier source not traced yet."),',
        'FeatureId.THICKNESS_MODIFIER to wired("Thickness modifier baked into selected strokes (MOD_gpencil_legacy_thick.c, no vertex groups/curve); Opacity modifier likewise. No live stack. $DEVICE"),',
        "registry")]),
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
