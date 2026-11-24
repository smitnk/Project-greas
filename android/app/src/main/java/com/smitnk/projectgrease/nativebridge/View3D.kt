package com.smitnk.projectgrease.nativebridge

import android.view.MotionEvent
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.smitnk.projectgrease.editor.EditorController
import kotlin.math.hypot

/** The 3D viewport's native state (native/blender_gp/android_gp_view3d.inc, Blender 3.6.23 view3d
 *  navigation and GP stroke placement). Touch coordinates are view pixels, y down. */
object View3DNative {
    init { System.loadLibrary("projectgrease_jni") }
    // RegionView3D.view (RV3D_VIEW_*)
    const val VIEW_FRONT = 1
    const val VIEW_RIGHT = 4
    const val VIEW_TOP = 5
    // gpencil_v3d_align (GP_PROJECT_*) and the placement modes built from it
    const val PROJECT_VIEWSPACE = 1
    const val PROJECT_DEPTH_VIEW = 4
    const val PROJECT_CURSOR = 32
    const val PLACE_ORIGIN = PROJECT_VIEWSPACE
    const val PLACE_CURSOR = PROJECT_VIEWSPACE or PROJECT_CURSOR
    const val PLACE_SURFACE = PROJECT_VIEWSPACE or PROJECT_DEPTH_VIEW
    const val PLACE_VIEW = 0
    // gp_sculpt.lock_axis (GP_LOCKAXIS_*): View, Side Y-Z, Front X-Z, Top X-Y, Cursor
    const val LOCK_VIEW = 0
    const val LOCK_X = 1
    const val LOCK_Y = 2
    const val LOCK_Z = 3
    const val LOCK_CURSOR = 4
    const val OB_ARMATURE = 1
    const val OB_EMPTY = 2

    external fun nativeSetActive(on: Boolean)
    external fun nativeActive(): Boolean
    external fun nativeOrbit(dx: Float, dy: Float, uiScale: Float)
    external fun nativePan(dx: Float, dy: Float)
    external fun nativeZoom(factor: Float, x: Float, y: Float)
    external fun nativeSetAxisView(view: Int)
    external fun nativeTogglePersp()
    /** persp, view, dist, viewquat[4], ofs[3], last 3D frame ms */
    external fun nativeState(): FloatArray
    external fun nativeSetPlacement(align: Int, lockAxis: Int)
    external fun nativeCursorAt(x: Float, y: Float)
    /** extend: true adds to the selection; a negative probe is nativeHitAt. Returns the object or -1. */
    external fun nativeSelectAt(x: Float, y: Float, extend: Boolean, uiScale: Float): Int
    external fun nativeHitAt(x: Float, y: Float, uiScale: Float): Int
    external fun nativeMoveSelected(dx: Float, dy: Float)
    external fun nativeAddObject(type: Int): Int
    external fun nativeObjectCount(): Int
    /** type, location xyz, selected */
    external fun nativeObject(index: Int): FloatArray?
    external fun nativeCanvasToScreen(x: Float, y: Float, z: Float): FloatArray?
    external fun nativeReset()
}

/** UI state of the 3D view (the bar and the touch handler read it). */
object View3DUi {
    var active by mutableStateOf(false)
    /** one finger draws (true) or navigates / selects (false) */
    var draw by mutableStateOf(false)
    var placement by mutableIntStateOf(View3DNative.PLACE_ORIGIN)
    var lock by mutableIntStateOf(View3DNative.LOCK_Y)
    var persp by mutableStateOf(true)

    fun setActive(on: Boolean, controller: EditorController) {
        active = on
        View3DNative.nativeSetActive(on)
        View3DNative.nativeSetPlacement(placement, lock)
        persp = View3DNative.nativeState()[0] == 1f
        controller.render()
    }
}

/** 3D-view touches: one finger orbits (turntable), or drags the selected object it starts on, or a tap
 *  selects the origin under it / places the 3D cursor; two fingers pan and pinch-zoom. In Draw, one
 *  finger is left to the editor's tools (the stroke is placed on the drawing plane when it ends). */
internal class View3DTouch(
    private val density: Float,
    private val slopPx: Float,
    private val redraw: () -> Unit,
    /** hands the editor a cancel when two fingers take over a stroke in Draw */
    private val cancelEditorGesture: (MotionEvent) -> Unit,
) {
    private var owning = false
    private var multi = false
    private var moved = false
    private var dragObject = false
    private var downX = 0f; private var downY = 0f
    private var lastX = 0f; private var lastY = 0f; private var lastDist = 0f

    private fun centroid(e: MotionEvent): Pair<Float, Float> {
        var x = 0f; var y = 0f; var n = 0
        for (i in 0 until e.pointerCount) {
            if (e.actionMasked == MotionEvent.ACTION_POINTER_UP && i == e.actionIndex) continue
            x += e.getX(i); y += e.getY(i); n++
        }
        return if (n == 0) e.x to e.y else x / n to y / n
    }

    private fun spread(e: MotionEvent): Float {
        val idx = (0 until e.pointerCount).filter { !(e.actionMasked == MotionEvent.ACTION_POINTER_UP && it == e.actionIndex) }
        return if (idx.size < 2) 0f else hypot(e.getX(idx[0]) - e.getX(idx[1]), e.getY(idx[0]) - e.getY(idx[1]))
    }

    private fun rebase(e: MotionEvent) {
        centroid(e).let { lastX = it.first; lastY = it.second }
        lastDist = spread(e)
    }

    /** true: the 3D view used the event. */
    fun handle(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                multi = false; moved = false; dragObject = false
                owning = !View3DUi.draw
                if (!owning) return false
                downX = e.x; downY = e.y
                rebase(e)
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (!owning) { cancelEditorGesture(e); owning = true }
                multi = true
                rebase(e)
            }
            MotionEvent.ACTION_MOVE -> {
                if (!owning) return false
                val (cx, cy) = centroid(e)
                val dx = cx - lastX; val dy = cy - lastY
                if (multi && e.pointerCount >= 2) {
                    View3DNative.nativePan(dx, dy) // viewmove_apply
                    val d = spread(e)
                    if (lastDist > 1f && d > 1f) View3DNative.nativeZoom(lastDist / d, cx, cy) // fingers apart: closer
                    lastDist = d
                } else if (!multi) {
                    if (!moved && hypot(e.x - downX, e.y - downY) > slopPx) {
                        moved = true
                        // Blender's select-and-drag: a drag that starts on an origin selects and moves that object
                        dragObject = View3DNative.nativeHitAt(downX, downY, density) >= 0
                        if (dragObject) View3DNative.nativeSelectAt(downX, downY, false, density)
                    }
                    if (moved) {
                        if (dragObject) View3DNative.nativeMoveSelected(dx, dy) else View3DNative.nativeOrbit(dx, dy, density)
                    }
                }
                lastX = cx; lastY = cy
                redraw()
            }
            MotionEvent.ACTION_POINTER_UP -> { if (!owning) return false; rebase(e) }
            MotionEvent.ACTION_UP -> {
                if (!owning) return false
                if (!multi && !moved) {
                    // click select (deselect_all on nothing); a click on empty space also places the 3D cursor
                    if (View3DNative.nativeSelectAt(e.x, e.y, false, density) < 0) View3DNative.nativeCursorAt(e.x, e.y)
                    redraw()
                }
                owning = false
            }
            MotionEvent.ACTION_CANCEL -> { val o = owning; owning = false; return o }
        }
        return true
    }
}

/** The Camera / 3D switch and, in 3D, Blender's numpad views, the persp toggle and the GP placement
 *  options. shortcut: plain text buttons; the UI redesign restyles them. */
@Composable
fun View3DBar(controller: EditorController, modifier: Modifier = Modifier) {
    @Composable
    fun B(label: String, tag: String, onClick: () -> Unit) =
        TextButton(onClick = { onClick(); controller.render() }, modifier = Modifier.testTag(tag)) { Text(label, fontSize = 12.sp, color = Color.White) }
    Row(
        modifier.padding(6.dp).background(Color(0xAA303030), RoundedCornerShape(8.dp)).horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(0.dp)
    ) {
        if (!View3DUi.active) {
            B("3D", "view3dToggle") { View3DUi.setActive(true, controller) }
            return@Row
        }
        B("0 Camera", "view3dToggle") { View3DUi.setActive(false, controller) }
        B("1 Front", "view3dFront") { View3DNative.nativeSetAxisView(View3DNative.VIEW_FRONT); View3DUi.persp = false }
        B("3 Right", "view3dRight") { View3DNative.nativeSetAxisView(View3DNative.VIEW_RIGHT); View3DUi.persp = false }
        B("7 Top", "view3dTop") { View3DNative.nativeSetAxisView(View3DNative.VIEW_TOP); View3DUi.persp = false }
        B(if (View3DUi.persp) "5 Persp" else "5 Ortho", "view3dPersp") {
            View3DNative.nativeTogglePersp(); View3DUi.persp = View3DNative.nativeState()[0] == 1f
        }
        B(if (View3DUi.draw) "Draw" else "Navigate", "view3dDraw") { View3DUi.draw = !View3DUi.draw }
        val places = listOf(View3DNative.PLACE_ORIGIN to "Origin", View3DNative.PLACE_CURSOR to "3D Cursor",
            View3DNative.PLACE_VIEW to "View", View3DNative.PLACE_SURFACE to "Surface")
        B(places.first { it.first == View3DUi.placement }.second, "view3dPlacement") {
            View3DUi.placement = places[(places.indexOfFirst { it.first == View3DUi.placement } + 1) % places.size].first
            View3DNative.nativeSetPlacement(View3DUi.placement, View3DUi.lock)
        }
        val planes = listOf("View", "Side Y-Z", "Front X-Z", "Top X-Y", "Cursor")
        B(planes[View3DUi.lock], "view3dPlane") {
            View3DUi.lock = (View3DUi.lock + 1) % planes.size
            View3DNative.nativeSetPlacement(View3DUi.placement, View3DUi.lock)
        }
        B("+Empty", "view3dAddEmpty") { View3DNative.nativeAddObject(View3DNative.OB_EMPTY) }
        B("+Armature", "view3dAddArmature") { View3DNative.nativeAddObject(View3DNative.OB_ARMATURE) }
    }
}
