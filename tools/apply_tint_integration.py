#!/usr/bin/env python3
"""Wire the Legacy GP Tint modifier (uniform) into Kotlin and the Advanced sheet (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (B / "editor/BlenderSelectRules.kt", "CMD_MOD_TINT", [(
        "            pointDensity, segmentInfluence, maxAngle, flag(invertCurvature)))\n    }\n}\n",
        "            pointDensity, segmentInfluence, maxAngle, flag(invertCurvature)))\n    }\n\n"
        "    // MOD_gpencil_legacy_tint.c, uniform type (gradient needs a scene object)\n"
        "    const val CMD_MOD_TINT = 43\n"
        "    const val PAINT_STROKE = 0\n    const val PAINT_FILL = 1\n    const val PAINT_BOTH = 2\n\n"
        "    fun tintModifier(mode: Int, factor: Float, r: Float, g: Float, b: Float): Command? {\n"
        "        if (mode !in PAINT_STROKE..PAINT_BOTH || !finite(factor) || !finite(r, g) || !finite(b)) return null\n"
        "        return Command(CMD_MOD_TINT, floatArrayOf(mode.toFloat(), factor, r, g, b))\n"
        "    }\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun applyTintModifier", [(
        "        runSelectCommand(ProjectGreaseSelect.lengthModifier(start, end, mode))\n",
        "        runSelectCommand(ProjectGreaseSelect.lengthModifier(start, end, mode))\n"
        "    /** Tint the selected strokes toward the current color (vertex colors, like Blender's Tint). */\n"
        "    fun applyTintModifier(factor:Float, mode:Int=ProjectGreaseSelect.PAINT_BOTH):Boolean {\n"
        "        val argb = materials.colorArgb\n"
        "        val r = ((argb shr 16) and 0xFF) / 255f\n"
        "        val g = ((argb shr 8) and 0xFF) / 255f\n"
        "        val b = (argb and 0xFF) / 255f\n"
        "        return runSelectCommand(ProjectGreaseSelect.tintModifier(mode, factor, r, g, b))\n"
        "    }\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "applyTintModifier", [(
        '            "Shorten ends 10%" to { controller.applyLengthModifier(-0.1f, -0.1f) }\n',
        '            "Shorten ends 10%" to { controller.applyLengthModifier(-0.1f, -0.1f) },\n'
        '            "Tint 50% (current color)" to { controller.applyTintModifier(0.5f) }\n',
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
