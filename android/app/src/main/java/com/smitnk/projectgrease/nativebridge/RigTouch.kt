package com.smitnk.projectgrease.nativebridge

import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.view.ViewConfiguration
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.RigController
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.hypot

/**
 * Canvas touches in Object mode and in armature Edit / Pose mode, in the Camera view (canvas px) and
 * in the 3D view ([handle3D]).
 * Object: a tap on a bone makes the armature active, elsewhere the GP object (Blender's click select).
 * Edit / Pose: tap selects (RigController.pick); a drag from the picked bone runs the active transform
 * tool, like the stroke transform tools: Move (G), Rotate (R) about the pivot, Scale (S) by the
 * distance from the pivot. One drag is one undo step. Extra fingers are ignored while a drag runs.
 * Extrude (armature.extrude_move: extrude, then translate the new tips): a long press on a tail
 * joint, or the armed Extrude tool (RigController.extrudeNext), then a drag puts the new tail
 * under the finger.
 */
class RigTouch(
    private val controller: EditorController,
    private val toCanvas: (Float, Float) -> Pair<Float, Float>,
    /** select distance in canvas px (ED_view3d_select_dist_px converted by the view zoom) */
    private val selectDist: () -> Float,
) {
    private var owning = false
    private var dragging = false
    private var extruding = false
    private var pivotX = 0f; private var pivotY = 0f
    private var lastX = 0f; private var lastY = 0f
    private var downX = 0f; private var downY = 0f // view px, for the long-press slop
    private var moved = false
    /** canvas px -> view px of the 3D view (View3DNative.nativeCanvasMatrix) while a 3D gesture runs */
    private var viewMatrix: FloatArray? = null
    private val handler = Handler(Looper.getMainLooper())
    private val slopPx = android.content.res.Resources.getSystem().displayMetrics.density * 8f
    private val longPress = Runnable { if (dragging && !moved && !extruding) startExtrude(lastX, lastY, follow = true) }

    private val rig get() = controller.rig

    /** Camera view. */
    fun handle(e: MotionEvent): Boolean {
        if (e.actionMasked == MotionEvent.ACTION_DOWN) { owning = controller.objectMode || controller.rigTouch; viewMatrix = null }
        return if (owning) gesture(e, toCanvas(e.x, e.y)) else false
    }

    /**
     * 3D view, armature Edit / Pose only (Object mode select is the 3D view's own). The pick goes through the
     * view's projection; drags are unprojected onto the bone plane (canvas z = 0), so a move follows the
     * finger on that plane and a rotation turns about the plane normal by the angle swept there.
     */
    fun handle3D(e: MotionEvent): Boolean {
        if (e.actionMasked == MotionEvent.ACTION_DOWN) {
            owning = controller.rigTouch
            viewMatrix = if (owning) View3DNative.nativeCanvasMatrix().takeIf { it.size == 16 } else null
            if (viewMatrix == null) owning = false
        }
        if (!owning) return false
        val p = viewToPlane(viewMatrix ?: return false, e.x, e.y) ?: return true
        val r = gesture(e, p, e.x, e.y)
        return if (e.actionMasked == MotionEvent.ACTION_DOWN && !owning) false else r // a miss orbits / selects as usual
    }

    private fun gesture(e: MotionEvent, c: Pair<Float, Float>, vx: Float = c.first, vy: Float = c.second): Boolean {
        val (x, y) = c
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                dragging = false; extruding = false; moved = false
                if (controller.objectMode) {
                    controller.setActiveObject(nearestBone(x, y) >= 0)
                    owning = false // a tap only; the rest of the gesture is ignored
                    return true
                }
                downX = e.x; downY = e.y
                val edit = rig.mode == RigController.MODE_EDIT
                if (edit && rig.extrudeNext) { // the armed Extrude tool: from the selected tips wherever the finger lands
                    rig.extrudeNext = false
                    lastX = x; lastY = y; dragging = true
                    if (startExtrude(x, y, follow = false)) return true
                    dragging = false; owning = false; return true
                }
                val m = viewMatrix
                val hit = if (m != null) rig.pick(vx, vy, RigController.SELECT_DIST_PX * android.content.res.Resources.getSystem().displayMetrics.density, canvasToWindow = m)
                    else rig.pick(x, y, selectDist())
                controller.render()
                if (hit < 0) { owning = false; return true } // the rest of a miss may pan / zoom the view
                val b = rig.bone(hit) ?: return true
                pivotX = if (edit) b.headX else b.poseHeadX; pivotY = if (edit) b.headY else b.poseHeadY
                if (!edit && rig.pivot == RigController.AROUND_MEDIAN) medianHead()?.let { pivotX = it.first; pivotY = it.second }
                lastX = x; lastY = y; dragging = true
                controller.history.markEdit() // the select is its own step, like Blender's click
                // a tail joint picked alone (BONE_TIPSEL without BONE_SELECTED): a long press extrudes from it
                if (edit && (b.flag and (RigController.SELECT_TAIL or RigController.SELECT_BONE)) == RigController.SELECT_TAIL)
                    handler.postDelayed(longPress, ViewConfiguration.getLongPressTimeout().toLong())
            }
            MotionEvent.ACTION_MOVE -> if (dragging) {
                if (!moved && hypot(e.x - downX, e.y - downY) > slopPx) { moved = true; handler.removeCallbacks(longPress) }
                val edit = rig.mode == RigController.MODE_EDIT
                when {
                    extruding -> rig.editTranslate(x - lastX, y - lastY) // the translate of extrude_move: new tips only
                    controller.tools.activeTool == GreaseTool.ROTATE -> {
                        // canvas y is down: counter-clockwise on screen is a negative atan2 step
                        val a = -(atan2(y - pivotY, x - pivotX) - atan2(lastY - pivotY, lastX - pivotX))
                        if (edit) rig.editRotate(a, pivotX, pivotY) else rig.poseRotate(a)
                    }
                    controller.tools.activeTool == GreaseTool.SCALE -> {
                        val d0 = hypot(lastX - pivotX, lastY - pivotY)
                        if (d0 > 1f) { val f = hypot(x - pivotX, y - pivotY) / d0
                            if (edit) rig.editScale(f, pivotX, pivotY) else rig.poseScale(f) }
                    }
                    else -> if (edit) rig.editTranslate(x - lastX, y - lastY) else rig.poseTranslate(x - lastX, y - lastY)
                }
                lastX = x; lastY = y
                controller.render()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                handler.removeCallbacks(longPress)
                if (dragging) controller.history.markEdit()
                dragging = false; extruding = false; owning = false
            }
        }
        return true
    }

    /**
     * ARMATURE_OT_extrude_move: extrude from the selected tips (new tips selected, zero length), then the
     * translate. [follow] false: the new tail jumps under the finger at (x, y) first.
     */
    private fun startExtrude(x: Float, y: Float, follow: Boolean): Boolean {
        // not (0, 0): pg_bke_armature_from_edit (run by every arm_update) drops zero-length bones, so a bare extrude vanishes
        if (rig.extrude(1f, 0f) <= 0) return false
        extruding = true
        if (follow) lastX += 1f // the new tail starts 1 px right of the tip: the drag deltas take that back out
        if (!follow) rig.bone(rig.activeBone)?.let { rig.editTranslate(x - it.tailX, y - it.tailY) }
        controller.render()
        return true
    }

    /** calculateCenterMedian of the selected pose heads (the pose pivot, Median Point). */
    private fun medianHead(): Pair<Float, Float>? {
        val sel = rig.bones().filter { it.selected }
        if (sel.isEmpty()) return null
        return sel.sumOf { it.poseHeadX.toDouble() }.toFloat() / sel.size to sel.sumOf { it.poseHeadY.toDouble() }.toFloat() / sel.size
    }

    /** Bone whose pose segment is within the select distance of (x, y), or -1. */
    private fun nearestBone(x: Float, y: Float): Int {
        val r = selectDist()
        return rig.bones().firstOrNull { b ->
            val dx = b.poseTailX - b.poseHeadX; val dy = b.poseTailY - b.poseHeadY
            val l2 = dx * dx + dy * dy
            val t = if (l2 < 1e-6f) 0f else (((x - b.poseHeadX) * dx + (y - b.poseHeadY) * dy) / l2).coerceIn(0f, 1f)
            hypot(b.poseHeadX + t * dx - x, b.poseHeadY + t * dy - y) <= r
        }?.index ?: -1
    }

    companion object {
        /**
         * View px -> canvas px on the plane z = 0: the inverse of the plane's homography (columns x, y, w of the
         * column-major canvas -> window matrix, rows x, y, w). Null when the plane is seen edge on.
         */
        fun viewToPlane(m: FloatArray, sx: Float, sy: Float): Pair<Float, Float>? {
            val a = m[0]; val b = m[4]; val c = m[12]
            val d = m[1]; val e = m[5]; val f = m[13]
            val g = m[3]; val h = m[7]; val i = m[15]
            // solve [a b c; d e f; g h i] (x, y, 1) ~ (sx, sy, 1): (a - g sx) x + (b - h sx) y = i sx - c ...
            val a1 = a - g * sx; val b1 = b - h * sx; val c1 = i * sx - c
            val a2 = d - g * sy; val b2 = e - h * sy; val c2 = i * sy - f
            val det = a1 * b2 - b1 * a2
            if (abs(det) < 1e-12f) return null
            return (c1 * b2 - b1 * c2) / det to (a1 * c2 - c1 * a2) / det
        }
    }
}
