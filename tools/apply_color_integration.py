#!/usr/bin/env python3
"""Wire the Legacy GP Hue/Saturation (Color) modifier into Kotlin and the Advanced sheet (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (B / "editor/BlenderSelectRules.kt", "CMD_MOD_COLOR", [(
        "        return Command(CMD_MOD_TINT, floatArrayOf(mode.toFloat(), factor, r, g, b))\n    }\n}\n",
        "        return Command(CMD_MOD_TINT, floatArrayOf(mode.toFloat(), factor, r, g, b))\n    }\n\n"
        "    // MOD_gpencil_legacy_color.c (Hue/Saturation). Defaults: hue 0.5 (no shift), saturation 1, value 1.\n"
        "    const val CMD_MOD_COLOR = 44\n\n"
        "    fun colorModifier(mode: Int, hue: Float = 0.5f, saturation: Float = 1f, value: Float = 1f): Command? {\n"
        "        if (mode !in MODIFY_BOTH..MODIFY_FILL || !finite(hue, saturation) || !finite(value)) return null\n"
        "        return Command(CMD_MOD_COLOR, floatArrayOf(mode.toFloat(), hue, saturation, value))\n"
        "    }\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun applyColorModifier", [(
        "        return runSelectCommand(ProjectGreaseSelect.tintModifier(mode, factor, r, g, b))\n    }\n",
        "        return runSelectCommand(ProjectGreaseSelect.tintModifier(mode, factor, r, g, b))\n    }\n"
        "    fun applyColorModifier(hue:Float=0.5f, saturation:Float=1f, value:Float=1f, mode:Int=ProjectGreaseSelect.MODIFY_BOTH) =\n"
        "        runSelectCommand(ProjectGreaseSelect.colorModifier(mode, hue, saturation, value))\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "applyColorModifier", [(
        '            "Tint 50% (current color)" to { controller.applyTintModifier(0.5f) }\n',
        '            "Tint 50% (current color)" to { controller.applyTintModifier(0.5f) },\n'
        '            "Hue shift +30\u00b0" to { controller.applyColorModifier(hue = 0.5f + 1f / 12f) },\n'
        '            "Desaturate 50%" to { controller.applyColorModifier(saturation = 0.5f) },\n'
        '            "Darken 20%" to { controller.applyColorModifier(value = 0.8f) }\n',
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
