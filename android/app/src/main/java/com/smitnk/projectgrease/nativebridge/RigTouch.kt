package com.smitnk.projectgrease.nativebridge

import android.view.MotionEvent
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.RigController
import kotlin.math.atan2
import kotlin.math.hypot

/**
 * Canvas touches in Object mode and in armature Edit / Pose mode (Camera view, canvas px).
 * Object: a tap on a bone makes the armature active, elsewhere the GP object (Blender's click select).
 * Edit / Pose: tap selects (RigController.pick); a drag from the picked bone runs the active transform
 * tool, like the stroke transform tools: Move (G), Rotate (R) about the bone head, Scale (S) by the
 * distance from the head. One drag is one undo step. Extra fingers are ignored while a drag runs.
 */
class RigTouch(
    private val controller: EditorController,
    private val toCanvas: (Float, Float) -> Pair<Float, Float>,
    /** select distance in canvas px (ED_view3d_select_dist_px converted by the view zoom) */
    private val selectDist: () -> Float,
) {
    private var owning = false
    private var dragging = false
    private var pivotX = 0f; private var pivotY = 0f
    private var lastX = 0f; private var lastY = 0f

    private val rig get() = controller.rig

    fun handle(e: MotionEvent): Boolean {
        if (e.actionMasked == MotionEvent.ACTION_DOWN) owning = controller.objectMode || controller.rigTouch
        if (!owning) return false
        val (x, y) = toCanvas(e.x, e.y)
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                dragging = false
                if (controller.objectMode) {
                    controller.setActiveObject(nearestBone(x, y) >= 0)
                    owning = false // a tap only; the rest of the gesture is ignored
                    return true
                }
                val hit = rig.pick(x, y, selectDist())
                controller.render()
                if (hit < 0) { owning = false; return true } // the rest of a miss may pan / zoom the view
                val b = rig.bone(hit) ?: return true
                val edit = rig.mode == RigController.MODE_EDIT
                pivotX = if (edit) b.headX else b.poseHeadX; pivotY = if (edit) b.headY else b.poseHeadY
                lastX = x; lastY = y; dragging = true
                controller.history.markEdit() // the select is its own step, like Blender's click
            }
            MotionEvent.ACTION_MOVE -> if (dragging) {
                val edit = rig.mode == RigController.MODE_EDIT
                when (controller.tools.activeTool) {
                    GreaseTool.ROTATE -> {
                        // canvas y is down: counter-clockwise on screen is a negative atan2 step
                        val a = -(atan2(y - pivotY, x - pivotX) - atan2(lastY - pivotY, lastX - pivotX))
                        if (edit) rig.editRotate(a, pivotX, pivotY) else rig.poseRotate(a)
                    }
                    GreaseTool.SCALE -> {
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
                if (dragging) controller.history.markEdit()
                dragging = false; owning = false
            }
        }
        return true
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
}
