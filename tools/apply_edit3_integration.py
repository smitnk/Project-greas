#!/usr/bin/env python3
"""Register project_grease_blender_edit3.c and wire its ten operators into Kotlin and the UI (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
P = "com.smitnk.projectgrease.editor.ProjectGreaseSelect"
EDITS = [
    (ROOT / "tools/android_gp_source_manifest.txt", "project_grease_blender_edit3.c", [(
        "C|native/blender_gp/project_grease_blender_edit2.c\n",
        "C|native/blender_gp/project_grease_blender_edit2.c\nC|native/blender_gp/project_grease_blender_edit3.c\n", "manifest")]),
    (ROOT / "tools/run_native_edit_tests.sh", "project_grease_blender_edit3.c", [(
        '"$ROOT/native/blender_gp/project_grease_blender_edit2.c")',
        '"$ROOT/native/blender_gp/project_grease_blender_edit2.c" "$ROOT/native/blender_gp/project_grease_blender_edit3.c")', "runner")]),
    (B / "editor/BlenderSelectRules.kt", "CMD_SELECT_RANDOM", [(
        "    fun extrude() = Command(CMD_EXTRUDE, FloatArray(0))\n}\n",
        "    fun extrude() = Command(CMD_EXTRUDE, FloatArray(0))\n\n"
        "    // project_grease_blender_edit3.h\n"
        "    const val CMD_SELECT_RANDOM = 66\n    const val CMD_BLANK_FRAME = 67\n    const val CMD_FILL_COLOR = 68\n"
        "    const val CMD_CLEAN_LOOSE = 69\n    const val CMD_CLEAN_DUP_FRAMES = 70\n    const val CMD_VCOLOR_SET = 71\n"
        "    const val CMD_VCOLOR_INVERT = 72\n    const val CMD_VCOLOR_BC = 73\n    const val CMD_VCOLOR_HSV = 74\n    const val CMD_VCOLOR_LEVELS = 75\n\n"
        "    fun selectRandom(ratio: Float, seed: Int, select: Boolean = true): Command? =\n"
        "        if (finite(ratio) && seed >= 0) Command(CMD_SELECT_RANDOM, floatArrayOf(ratio.coerceIn(0f, 1f), seed.toFloat(), flag(select))) else null\n"
        "    fun blankFrame(frame: Int): Command? = if (frame >= 0) Command(CMD_BLANK_FRAME, floatArrayOf(frame.toFloat())) else null\n"
        "    fun fillColor(material: Int, r: Float, g: Float, b: Float, a: Float): Command? =\n"
        "        if (material >= 0 && finite(r, g) && finite(b, a)) Command(CMD_FILL_COLOR, floatArrayOf(material.toFloat(), r, g, b, a)) else null\n"
        "    fun cleanLoose(limit: Int = 1): Command? = if (limit >= 1) Command(CMD_CLEAN_LOOSE, floatArrayOf(limit.toFloat())) else null\n"
        "    fun cleanDuplicateFrames() = Command(CMD_CLEAN_DUP_FRAMES, FloatArray(0))\n"
        "    private fun paintMode(mode: Int) = mode in PAINT_STROKE..PAINT_BOTH\n"
        "    fun vcolorSet(mode: Int, r: Float, g: Float, b: Float, factor: Float = 1f): Command? =\n"
        "        if (paintMode(mode) && finite(r, g) && finite(b, factor)) Command(CMD_VCOLOR_SET, floatArrayOf(mode.toFloat(), r, g, b, factor)) else null\n"
        "    fun vcolorInvert(mode: Int): Command? = if (paintMode(mode)) Command(CMD_VCOLOR_INVERT, floatArrayOf(mode.toFloat())) else null\n"
        "    fun vcolorBrightnessContrast(mode: Int, brightness: Float, contrast: Float): Command? =\n"
        "        if (paintMode(mode) && finite(brightness, contrast)) Command(CMD_VCOLOR_BC, floatArrayOf(mode.toFloat(), brightness, contrast)) else null\n"
        "    fun vcolorHsv(mode: Int, h: Float = 0.5f, s: Float = 1f, v: Float = 1f): Command? =\n"
        "        if (paintMode(mode) && finite(h, s) && finite(v)) Command(CMD_VCOLOR_HSV, floatArrayOf(mode.toFloat(), h, s, v)) else null\n"
        "    fun vcolorLevels(mode: Int, offset: Float, gain: Float): Command? =\n"
        "        if (paintMode(mode) && finite(offset, gain)) Command(CMD_VCOLOR_LEVELS, floatArrayOf(mode.toFloat(), offset, gain)) else null\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun insertBlankFrame", [(
        "    fun extrudeSelection() = runSelectCommand(ProjectGreaseSelect.extrude())\n",
        "    fun extrudeSelection() = runSelectCommand(ProjectGreaseSelect.extrude())\n"
        "    private var randomSeed = 0\n"
        "    fun selectRandom(ratio:Float=0.5f) = runSelectCommand(ProjectGreaseSelect.selectRandom(ratio, randomSeed++))\n"
        "    /** Insert a blank keyframe at the current frame, shifting later frames (GPENCIL_OT_blank_frame_add). */\n"
        "    fun insertBlankFrame() = runSelectCommand(ProjectGreaseSelect.blankFrame(animation.currentFrame))\n"
        "    fun setFillColor(argb:Int) = runSelectCommand(ProjectGreaseSelect.fillColor(materials.activeMaterial,\n"
        "        ((argb shr 16) and 0xFF)/255f, ((argb shr 8) and 0xFF)/255f, (argb and 0xFF)/255f, ((argb ushr 24) and 0xFF)/255f))\n"
        "    fun cleanLoosePoints(limit:Int=1) = runSelectCommand(ProjectGreaseSelect.cleanLoose(limit))\n"
        "    fun cleanDuplicateFrames() = runSelectCommand(ProjectGreaseSelect.cleanDuplicateFrames())\n"
        "    fun setSelectionVertexColor(mode:Int=ProjectGreaseSelect.PAINT_STROKE):Boolean {\n"
        "        val argb = materials.colorArgb\n"
        "        return runSelectCommand(ProjectGreaseSelect.vcolorSet(mode, ((argb shr 16) and 0xFF)/255f,\n"
        "            ((argb shr 8) and 0xFF)/255f, (argb and 0xFF)/255f))\n"
        "    }\n"
        "    fun invertSelectionVertexColor(mode:Int=ProjectGreaseSelect.PAINT_BOTH) = runSelectCommand(ProjectGreaseSelect.vcolorInvert(mode))\n"
        "    fun selectionVertexColorBrightnessContrast(b:Float, c:Float, mode:Int=ProjectGreaseSelect.PAINT_BOTH) =\n"
        "        runSelectCommand(ProjectGreaseSelect.vcolorBrightnessContrast(mode, b, c))\n"
        "    fun selectionVertexColorHsv(h:Float=0.5f, s:Float=1f, v:Float=1f, mode:Int=ProjectGreaseSelect.PAINT_BOTH) =\n"
        "        runSelectCommand(ProjectGreaseSelect.vcolorHsv(mode, h, s, v))\n"
        "    fun selectionVertexColorLevels(offset:Float, gain:Float, mode:Int=ProjectGreaseSelect.PAINT_BOTH) =\n"
        "        runSelectCommand(ProjectGreaseSelect.vcolorLevels(mode, offset, gain))\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "insertBlankFrame", [(
        '            "Extrude ends" to { controller.extrudeSelection() }\n',
        '            "Extrude ends" to { controller.extrudeSelection() },\n'
        '            "Select random 50%" to { controller.selectRandom() },\n'
        '            "Insert blank keyframe" to { controller.insertBlankFrame() },\n'
        '            "Use color as fill color" to { controller.setFillColor(controller.materials.colorArgb) },\n'
        '            "Clean loose points" to { controller.cleanLoosePoints() },\n'
        '            "Clean duplicate frames" to { controller.cleanDuplicateFrames() },\n'
        '            "Set vertex color" to { controller.setSelectionVertexColor() },\n'
        '            "Invert vertex color" to { controller.invertSelectionVertexColor() },\n'
        '            "Vertex color brighter" to { controller.selectionVertexColorBrightnessContrast(0.1f, 0f) },\n'
        '            "Vertex color more contrast" to { controller.selectionVertexColorBrightnessContrast(0f, 0.2f) },\n'
        '            "Vertex color hue +30\u00b0" to { controller.selectionVertexColorHsv(h = 0.5f + 1f / 12f) },\n'
        '            "Vertex color levels x0.8" to { controller.selectionVertexColorLevels(0f, 0.8f) }\n',
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
