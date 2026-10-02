#!/usr/bin/env python3
"""Register project_grease_blender_edit2.c and wire its operators into Kotlin and the UI (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
B = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"
EDITS = [
    (ROOT / "tools/android_gp_source_manifest.txt", "project_grease_blender_edit2.c", [(
        "C|native/blender_gp/project_grease_blender_edit.c\n",
        "C|native/blender_gp/project_grease_blender_edit.c\nC|native/blender_gp/project_grease_blender_edit2.c\n",
        "manifest")]),
    (ROOT / "tools/run_native_edit_tests.sh", "project_grease_blender_edit2.c", [(
        '"$ROOT/native/blender_gp/project_grease_blender_edit.c")',
        '"$ROOT/native/blender_gp/project_grease_blender_edit.c" "$ROOT/native/blender_gp/project_grease_blender_edit2.c")',
        "test runner")]),
    (B / "editor/BlenderSelectRules.kt", "CMD_SELECT_VCOLOR", [(
        "        return Command(CMD_WEIGHT_PAINT, floatArrayOf(group.toFloat(), x, y, radius, strength, weight))\n    }\n}\n",
        "        return Command(CMD_WEIGHT_PAINT, floatArrayOf(group.toFloat(), x, y, radius, strength, weight))\n    }\n\n"
        "    // project_grease_blender_edit2.h\n"
        "    const val CMD_MOD_THICKNESS_VGROUP = 58\n    const val CMD_SELECT_VCOLOR = 59\n    const val CMD_NORMALIZE = 60\n"
        "    const val CMD_SIMPLIFY_FIXED = 61\n    const val CMD_SAMPLE = 62\n    const val CMD_EXTRUDE = 63\n"
        "    const val NORMALIZE_THICKNESS = 0\n    const val NORMALIZE_OPACITY = 1\n\n"
        "    fun thicknessModifierVGroup(group: Int, invert: Boolean, factor: Float, normalize: Boolean = false, thickness: Int = 0): Command? {\n"
        "        if (group < -1 || thickness < 0 || !finite(factor)) return null\n"
        "        return Command(CMD_MOD_THICKNESS_VGROUP, floatArrayOf(group.toFloat(), flag(invert), flag(normalize), thickness.toFloat(), factor))\n"
        "    }\n"
        "    fun selectVertexColor(r: Float, g: Float, b: Float, threshold: Float, extend: Boolean = false): Command? {\n"
        "        if (!finite(r, g) || !finite(b, threshold) || threshold < 0f) return null\n"
        "        return Command(CMD_SELECT_VCOLOR, floatArrayOf(r, g, b, threshold, flag(extend)))\n"
        "    }\n"
        "    fun normalize(mode: Int, value: Float): Command? =\n"
        "        if ((mode == NORMALIZE_THICKNESS || mode == NORMALIZE_OPACITY) && finite(value)) Command(CMD_NORMALIZE, floatArrayOf(mode.toFloat(), value)) else null\n"
        "    fun simplifyFixed(steps: Int): Command? = if (steps in 1..100) Command(CMD_SIMPLIFY_FIXED, floatArrayOf(steps.toFloat())) else null\n"
        "    fun sample(length: Float, sharpThreshold: Float = 0.1f): Command? =\n"
        "        if (finite(length, sharpThreshold) && length > 0f) Command(CMD_SAMPLE, floatArrayOf(length, sharpThreshold)) else null\n"
        "    fun extrude() = Command(CMD_EXTRUDE, FloatArray(0))\n}\n",
        "rules")]),
    (B / "editor/EditorControllers.kt", "fun selectByVertexColor", [(
        "    fun endWeightPaint() { if (weightPaintChanged) history.markEdit(); weightPaintChanged = false }\n",
        "    fun endWeightPaint() { if (weightPaintChanged) history.markEdit(); weightPaintChanged = false }\n"
        "    fun applyThicknessModifierWithWeights(factor:Float, invert:Boolean=false) =\n"
        "        runSelectCommand(ProjectGreaseSelect.thicknessModifierVGroup(weightPaintGroup, invert, factor))\n"
        "    fun selectByVertexColor(threshold:Float=0.05f, extend:Boolean=false):Boolean {\n"
        "        val argb = materials.colorArgb\n"
        "        return runSelectCommand(ProjectGreaseSelect.selectVertexColor(((argb shr 16) and 0xFF)/255f,\n"
        "            ((argb shr 8) and 0xFF)/255f, (argb and 0xFF)/255f, threshold, extend))\n"
        "    }\n"
        "    fun normalizeSelection(mode:Int, value:Float) = runSelectCommand(ProjectGreaseSelect.normalize(mode, value))\n"
        "    fun simplifySelectionFixed(steps:Int=1) = runSelectCommand(ProjectGreaseSelect.simplifyFixed(steps))\n"
        "    fun sampleSelection(length:Float) = runSelectCommand(ProjectGreaseSelect.sample(length))\n"
        "    fun extrudeSelection() = runSelectCommand(ProjectGreaseSelect.extrude())\n",
        "controller")]),
    (B / "ui/ProjectGreaseUI.kt", "selectByVertexColor", [(
        '            "Mirror copy (X+Y)" to { controller.mirrorSelectionCopy(true, true) }\n',
        '            "Mirror copy (X+Y)" to { controller.mirrorSelectionCopy(true, true) },\n'
        '            "Thickness x2 by weight" to { controller.applyThicknessModifierWithWeights(2f) },\n'
        '            "Select by vertex color" to { controller.selectByVertexColor() },\n'
        '            "Normalize thickness" to { controller.normalizeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.NORMALIZE_THICKNESS, 1f) },\n'
        '            "Normalize opacity" to { controller.normalizeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.NORMALIZE_OPACITY, 1f) },\n'
        '            "Simplify (fixed)" to { controller.simplifySelectionFixed() },\n'
        '            "Resample (8 px)" to { controller.sampleSelection(8f) },\n'
        '            "Extrude ends" to { controller.extrudeSelection() }\n',
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
