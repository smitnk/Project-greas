#!/usr/bin/env python3
"""Wire the Legacy GP Length modifier into Kotlin and the Advanced sheet (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (B / "editor/BlenderSelectRules.kt", "CMD_MOD_LENGTH", [(
        "            Command(CMD_MOD_OPACITY, floatArrayOf(mode.toFloat(), factor, flag(normalize), hardness)) else null\n}\n",
        "            Command(CMD_MOD_OPACITY, floatArrayOf(mode.toFloat(), factor, flag(normalize), hardness)) else null\n\n"
        "    // MOD_gpencil_legacy_length.c (no random offsets). Defaults follow DNA_gpencil_modifier_defaults.h.\n"
        "    const val CMD_MOD_LENGTH = 42\n"
        "    const val LENGTH_RELATIVE = 0\n    const val LENGTH_ABSOLUTE = 1\n\n"
        "    fun lengthModifier(start: Float, end: Float, mode: Int = LENGTH_RELATIVE, overshoot: Float = 0.1f,\n"
        "                       useCurvature: Boolean = false, pointDensity: Float = 30f, segmentInfluence: Float = 0f,\n"
        "                       maxAngle: Float = 2.9670597f, invertCurvature: Boolean = false): Command? {\n"
        "        if (mode != LENGTH_RELATIVE && mode != LENGTH_ABSOLUTE) return null\n"
        "        if (!finite(start, end) || !finite(overshoot, pointDensity) || !finite(segmentInfluence, maxAngle)) return null\n"
        "        return Command(CMD_MOD_LENGTH, floatArrayOf(mode.toFloat(), start, end, overshoot, flag(useCurvature),\n"
        "            pointDensity, segmentInfluence, maxAngle, flag(invertCurvature)))\n"
        "    }\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun applyLengthModifier", [(
        "        runSelectCommand(ProjectGreaseSelect.opacityModifier(mode, factor, normalize, hardness))\n",
        "        runSelectCommand(ProjectGreaseSelect.opacityModifier(mode, factor, normalize, hardness))\n"
        "    fun applyLengthModifier(start:Float, end:Float, mode:Int=ProjectGreaseSelect.LENGTH_RELATIVE) =\n"
        "        runSelectCommand(ProjectGreaseSelect.lengthModifier(start, end, mode))\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "applyLengthModifier", [(
        '            "Opacity 100%" to { controller.applyOpacityModifier(com.smitnk.projectgrease.editor.ProjectGreaseSelect.MODIFY_BOTH, 1f, normalize = true) }\n',
        '            "Opacity 100%" to { controller.applyOpacityModifier(com.smitnk.projectgrease.editor.ProjectGreaseSelect.MODIFY_BOTH, 1f, normalize = true) },\n'
        '            "Lengthen ends 10%" to { controller.applyLengthModifier(0.1f, 0.1f) },\n'
        '            "Shorten ends 10%" to { controller.applyLengthModifier(-0.1f, -0.1f) }\n',
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
