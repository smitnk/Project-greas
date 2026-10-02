#!/usr/bin/env python3
"""Wire selection-aware editing into the native backend, CI and the Kotlin controller.

Idempotent; every edit uses an exact anchor and the whole run changes nothing if one is missing.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
N = ROOT / "native/blender_gp"
K = ROOT / "android/app/src/main/java/com/smitnk/projectgrease/editor"

KOTLIN_CONTROLLER_NEW = '''    /** Median of every selected point (native stroke_center(-1)); null when nothing is selected. */
    fun selectionPivot():FloatArray? = native.strokeCenter(-1)?.takeIf { it.size >= 2 }
    fun selectedStrokeCenter():FloatArray? {
        selectionPivot()?.let { return it }
        val i=selection.selectedStroke
        if(i<0) return null
        return native.strokeCenter(i)
    }
    /** Tapping selects whole strokes (true) or the nearest point only (false, Blender's point mode). */
    var pickEntireStrokes = true
        private set
    fun setPickEntireStrokes(value:Boolean) { pickEntireStrokes = value }

    /** Blender gpencil_select_exec(): nearest point under the tap; a tap on empty space deselects. */
    fun pickSelect(x:Float, y:Float):Boolean {
        val flags = ProjectGreaseSelect.PICK_DESELECT_ALL or
            (if (pickEntireStrokes) ProjectGreaseSelect.PICK_ENTIRE else 0)
        val radiusSquared = ProjectGreaseSelect.pickRadiusSquared(view.zoom)
        val changed = runSelectCommand(ProjectGreaseSelect.pick(x, y, radiusSquared, flags, selectMode))
        val index = native.hitTestStroke(x, y, 24f)
        if (index >= 0) selection.note(index) else if (changed) selection.clear()
        return changed || index >= 0
    }
    fun deleteSelectedStrokes() = runSelectCommand(ProjectGreaseSelect.deleteStrokes()).also { if (it) selection.clear() }
    fun deleteSelectedPoints() = runSelectCommand(ProjectGreaseSelect.deletePoints()).also { if (it) selection.clear() }
'''

KOTLIN_EDITS = [
    (
        K / "EditorControllers.kt",
        "fun selectionPivot()",
        [
            (
                "    fun clear(){hasSelection=false; selectedStroke=-1}\n",
                "    fun clear(){hasSelection=false; selectedStroke=-1}\n"
                "    /** Remember the stroke under the last pick without touching the native selection. */\n"
                "    fun note(index:Int){hasSelection=true; selectedStroke=index}\n",
                "SelectionController.note",
            ),
            (
                "    fun hitTestAndSelectStroke(x:Float, y:Float, radius:Float = 24f):Boolean {\n"
                "        val index = native.hitTestStroke(x, y, radius)\n"
                "        return index >= 0 && selection.selectStroke(index)\n"
                "    }\n",
                "    fun hitTestAndSelectStroke(x:Float, y:Float, radius:Float = 24f):Boolean {\n"
                "        // The select tool is a plain Blender click-select.\n"
                "        if (tools.activeTool == GreaseTool.SELECT) return pickSelect(x, y)\n"
                "        val index = native.hitTestStroke(x, y, radius)\n"
                "        if (index < 0) return false\n"
                "        // Transform tools: pass-through keeps the current selection when the tapped stroke is\n"
                "        // part of it, so move/rotate/scale/mirror act on everything that is selected.\n"
                "        runSelectCommand(ProjectGreaseSelect.pick(\n"
                "            x, y, ProjectGreaseSelect.pickRadiusSquared(view.zoom),\n"
                "            ProjectGreaseSelect.PICK_ENTIRE or ProjectGreaseSelect.PICK_PASSTHROUGH,\n"
                "            ProjectGreaseSelect.MODE_STROKE))\n"
                "        if (selectionPivot() == null) return selection.selectStroke(index)\n"
                "        selection.note(index)\n"
                "        return true\n"
                "    }\n",
                "hitTestAndSelectStroke",
            ),
            (
                "    fun selectedStrokeCenter():FloatArray?{val i=selection.selectedStroke;if(i<0)return null;return native.strokeCenter(i)}\n",
                KOTLIN_CONTROLLER_NEW,
                "selectedStrokeCenter",
            ),
            (
                "    fun translateSelectedStroke(dx:Float,dy:Float):Boolean{val i=selection.selectedStroke;",
                "    fun translateSelectedStroke(dx:Float,dy:Float):Boolean{"
                "if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.translate(dx,dy));"
                "val i=selection.selectedStroke;",
                "translate",
            ),
            (
                "    fun rotateSelectedStrokeAround(radians:Float,centerX:Float,centerY:Float):Boolean{val i=selection.selectedStroke;",
                "    fun rotateSelectedStrokeAround(radians:Float,centerX:Float,centerY:Float):Boolean{"
                "if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.rotate(radians,floatArrayOf(centerX,centerY)));"
                "val i=selection.selectedStroke;",
                "rotate",
            ),
            (
                "    fun scaleSelectedStrokeAround(scaleX:Float,scaleY:Float,centerX:Float,centerY:Float):Boolean{val i=selection.selectedStroke;",
                "    fun scaleSelectedStrokeAround(scaleX:Float,scaleY:Float,centerX:Float,centerY:Float):Boolean{"
                "if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.scale(scaleX,scaleY,floatArrayOf(centerX,centerY)));"
                "val i=selection.selectedStroke;",
                "scale",
            ),
            (
                "    fun mirrorSelectedStrokeAround(mirrorX:Boolean,mirrorY:Boolean,centerX:Float,centerY:Float):Boolean{val i=selection.selectedStroke;",
                "    fun mirrorSelectedStrokeAround(mirrorX:Boolean,mirrorY:Boolean,centerX:Float,centerY:Float):Boolean{"
                "if(selectionPivot()!=null)return runSelectCommand(ProjectGreaseSelect.mirror(mirrorX,mirrorY,floatArrayOf(centerX,centerY)));"
                "val i=selection.selectedStroke;",
                "mirror",
            ),
        ],
    ),
    (
        K / "BlenderSelectRules.kt",
        "const val CMD_PICK",
        [
            (
                "        return Command(CMD_CIRCLE, floatArrayOf(op.toFloat(), mode.toFloat(), x, y, radius, flag(isFirst)))\n    }\n}\n",
                "        return Command(CMD_CIRCLE, floatArrayOf(op.toFloat(), mode.toFloat(), x, y, radius, flag(isFirst)))\n    }\n\n"
                "    // ---- selection-aware editing (native/blender_gp/project_grease_blender_edit.h) ----\n"
                "    const val PICK_EXTEND = 1\n"
                "    const val PICK_DESELECT = 2\n"
                "    const val PICK_TOGGLE = 4\n"
                "    const val PICK_ENTIRE = 8\n"
                "    const val PICK_DESELECT_ALL = 16\n"
                "    const val PICK_PASSTHROUGH = 32\n\n"
                "    const val CMD_PICK = 31\n"
                "    const val CMD_TRANSLATE = 32\n"
                "    const val CMD_ROTATE = 33\n"
                "    const val CMD_SCALE = 34\n"
                "    const val CMD_MIRROR = 35\n"
                "    const val CMD_DELETE_STROKES = 36\n"
                "    const val CMD_DELETE_POINTS = 37\n\n"
                "    /** gpencil_select_exec(): radius = 0.4 * widget_unit (20) = 8, scaled to canvas units by the zoom. */\n"
                "    const val PICK_RADIUS = 8f\n\n"
                "    /** `(int)(radius * radius)`: Blender compares the Manhattan distance with this squared radius. */\n"
                "    fun pickRadiusSquared(zoom: Float): Int {\n"
                "        val radius = PICK_RADIUS / zoom.coerceAtLeast(0.1f)\n"
                "        return (radius * radius).toInt()\n"
                "    }\n\n"
                "    fun pick(x: Float, y: Float, radiusSquared: Int, flags: Int, mode: Int): Command? {\n"
                "        if (!isValidMode(mode) || radiusSquared < 0 || !finite(x, y)) return null\n"
                "        return Command(CMD_PICK, floatArrayOf(x, y, radiusSquared.toFloat(), flags.toFloat(), mode.toFloat()))\n"
                "    }\n\n"
                "    private fun withPivot(values: FloatArray, pivot: FloatArray?): FloatArray? {\n"
                "        if (pivot == null) return values\n"
                "        if (pivot.size < 2 || !finite(pivot[0], pivot[1])) return null\n"
                "        return values + floatArrayOf(pivot[0], pivot[1])\n"
                "    }\n\n"
                "    fun translate(dx: Float, dy: Float): Command? =\n"
                "        if (finite(dx, dy)) Command(CMD_TRANSLATE, floatArrayOf(dx, dy)) else null\n\n"
                "    fun rotate(radians: Float, pivot: FloatArray? = null): Command? {\n"
                "        if (!finite(radians)) return null\n"
                "        return withPivot(floatArrayOf(radians), pivot)?.let { Command(CMD_ROTATE, it) }\n"
                "    }\n\n"
                "    fun scale(sx: Float, sy: Float, pivot: FloatArray? = null): Command? {\n"
                "        if (!finite(sx, sy) || sx == 0f || sy == 0f) return null\n"
                "        return withPivot(floatArrayOf(sx, sy), pivot)?.let { Command(CMD_SCALE, it) }\n"
                "    }\n\n"
                "    fun mirror(mirrorX: Boolean, mirrorY: Boolean, pivot: FloatArray? = null): Command? {\n"
                "        if (!mirrorX && !mirrorY) return null\n"
                "        return withPivot(floatArrayOf(flag(mirrorX), flag(mirrorY)), pivot)?.let { Command(CMD_MIRROR, it) }\n"
                "    }\n\n"
                "    fun deleteStrokes() = Command(CMD_DELETE_STROKES, FloatArray(0))\n"
                "    fun deletePoints() = Command(CMD_DELETE_POINTS, FloatArray(0))\n}\n",
                "ProjectGreaseSelect editing commands",
            ),
        ],
    ),
    (
        N / "project_grease_gp_bridge.cpp",
        "pg_gp_edit_dispatch",
        [
            (
                '#include "project_grease_blender_select.h"\n',
                '#include "project_grease_blender_select.h"\n#include "project_grease_blender_edit.h"\n',
                "bridge include",
            ),
            (
                "    default:\n"
                "      // Blender 3.6.23 Legacy GP selection operators (ids 20..30).\n"
                "      return pg_gp_select_dispatch(handle->backend.document_data(),\n",
                "    default:\n"
                "      // Selection-aware editing (ids 31..37), then the selection operators (20..30).\n"
                "      if (command >= PG_EDIT_CMD_FIRST && command <= PG_EDIT_CMD_LAST) {\n"
                "        return pg_gp_edit_dispatch(handle->backend.document_data(),\n"
                "                                   handle->backend.active_layer_data(),\n"
                "                                   command,\n"
                "                                   args,\n"
                "                                   arg_count);\n"
                "      }\n"
                "      return pg_gp_select_dispatch(handle->backend.document_data(),\n",
                "bridge default branch",
            ),
        ],
    ),
    (
        N / "project_grease_gp_backend.cpp",
        "pg_gp_edit_selection_pivot",
        [
            (
                '#include "project_grease_legacy_eraser.h"\n',
                '#include "project_grease_legacy_eraser.h"\n#include "project_grease_blender_edit.h"\n',
                "backend include",
            ),
            (
                "bool Backend::stroke_center(int index, float *x, float *y) const\n{\n"
                "  if (!impl_->frame || index < 0 || !x || !y) {\n",
                "bool Backend::stroke_center(int index, float *x, float *y) const\n{\n"
                "  // Reserved index -1: median of every selected point (pivot of selection-wide edits).\n"
                "  if (index == -1 && x && y && impl_->gpd) {\n"
                "    return pg_gp_edit_selection_pivot(impl_->gpd, impl_->layer, x, y) != 0;\n"
                "  }\n"
                "  if (!impl_->frame || index < 0 || !x || !y) {\n",
                "stroke_center pivot",
            ),
        ],
    ),
    (
        ROOT / "tools/android_gp_source_manifest.txt",
        "project_grease_blender_edit.c",
        [
            (
                "C|native/blender_gp/project_grease_blender_eraser.c\n",
                "C|native/blender_gp/project_grease_blender_eraser.c\nC|native/blender_gp/project_grease_blender_edit.c\n",
                "manifest",
            ),
        ],
    ),
    (
        ROOT / ".github/workflows/android-shell.yml",
        "run_native_edit_tests.sh",
        [
            (
                "      - name: Run native eraser tests\n        run: bash tools/run_native_eraser_tests.sh\n",
                "      - name: Run native eraser tests\n        run: bash tools/run_native_eraser_tests.sh\n\n"
                "      - name: Run native edit tests\n        run: bash tools/run_native_edit_tests.sh\n",
                "workflow",
            ),
        ],
    ),
]


def main():
    pending = []
    for path, marker, edits in KOTLIN_EDITS:
        text = path.read_text(encoding="utf-8")
        if marker in text:
            print(f"already applied: {path.relative_to(ROOT)}")
            continue
        for old, new, label in edits:
            if text.count(old) != 1:
                print(f"anchor '{label}' in {path.relative_to(ROOT)} found {text.count(old)} times "
                      "(expected 1); nothing changed", file=sys.stderr)
                return 1
            text = text.replace(old, new)
        pending.append((path, text))
    for path, text in pending:
        path.write_text(text, encoding="utf-8")
        print(f"patched: {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
