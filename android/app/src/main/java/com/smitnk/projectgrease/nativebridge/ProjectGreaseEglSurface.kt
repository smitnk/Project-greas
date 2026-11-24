package com.smitnk.projectgrease.nativebridge

import com.smitnk.projectgrease.editor.RigController
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.hypot
import kotlin.math.min
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.viewinterop.AndroidView
import com.smitnk.projectgrease.editor.EditorController

/** Finger travel (px) and time (ms) under which a two / three finger touch is a tap (undo / redo). */

private const val TAP_TIMEOUT_MS = 350L

@Composable
fun ProjectGreaseEglViewport(
    modifier: Modifier = Modifier,
    controller: EditorController
) {
  androidx.compose.foundation.layout.Box(modifier) {
    // the drawing view, so onDispose can zero its native handle before the renderer is destroyed
    val viewRef = androidx.compose.runtime.remember { arrayOfNulls<ProjectGreaseDrawingSurfaceView>(1) }
    AndroidView(
        modifier = Modifier.matchParentSize(),
        factory = { context ->
            val view = ProjectGreaseDrawingSurfaceView(context, controller)
            viewRef[0] = view
            view.holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(holder: SurfaceHolder) {
                    // One renderer (GL context + Blender document) for the editor's lifetime: a surface
                    // coming back (picker closed, app resumed) attaches to the same document.
                    val existing = controller.rendererHandleOrZero()
                    controller.cancelActiveGestures()
                    val handle = if (existing != 0L) existing else GPNative.nativeCreateEglRenderer()
                    view.setRendererHandle(handle)
                    if (existing != 0L) {
                        if (GPNative.nativeAttachSurface(handle, holder.surface)) controller.surfaceAttached()
                        return
                    }
                    if (handle != 0L && GPNative.nativeAttachSurface(handle, holder.surface)) {
                        controller.attachRenderer(handle)
                        Log.i(
                            "ProjectGrease",
                            "EGL/GLES " + GPNative.nativeGlesVersion(handle) +
                                " surface connected; Blender GP backend=" +
                                GPNative.nativeBlenderGpConnected(handle)
                        )
                        controller.render()
                    }
                }

                override fun surfaceChanged(
                    holder: SurfaceHolder,
                    format: Int,
                    width: Int,
                    height: Int
                ) {
                    // Same surface, new size: the renderer re-reads the surface size on every
                    // render. Re-attaching here went through detach_window(), which destroys the
                    // Blender GP document and starts an empty "Layer 1" one (New Project lost its
                    // template, a resize or rotation lost the drawing).
                    if (view.getRendererHandle() != 0L) controller.render()
                }

                override fun surfaceDestroyed(holder: SurfaceHolder) {
                    // only the window surface goes; the document stays (see detach_surface_keep_document).
                    // The view's handle is zeroed when the editor is disposed (renderer destroyed): never
                    // use a handle the controller no longer owns.
                    val handle = view.getRendererHandle()
                    controller.surfaceDetached()
                    if (handle != 0L) GPNative.nativeDetachSurface(handle)
                }
            })
            view
        }
    )
    // The editor screen leaves (Home, another project): now the renderer and its document go.
    androidx.compose.runtime.DisposableEffect(controller) {
        onDispose {
            val view = viewRef[0]
            val handle = controller.rendererHandleOrZero().takeIf { it != 0L } ?: view?.getRendererHandle() ?: 0L
            // zero the view's copy first: surfaceDestroyed / touches arriving after this must not see a freed pointer
            view?.setRendererHandle(0L)
            controller.onCancelGestures = null
            controller.isGestureOpenInView = null
            if (handle != 0L) {
                controller.detachRenderer() // cancels gestures, snapshots the document, waits for a running fill
                GPNative.nativeDetachSurface(handle)
                GPNative.nativeDestroyEglRenderer(handle)
            }
        }
    }
    TransformOverlay(controller, Modifier.matchParentSize())
    com.smitnk.projectgrease.ui.RigHandleOverlay(controller, Modifier.matchParentSize())
    View3DBar(controller, Modifier.align(androidx.compose.ui.Alignment.TopEnd)) // Camera / 3D switch
  }
}

/** Proportional-editing radius circle (Blender draws it while transforming) and the 2D cursor. */
@Composable
private fun TransformOverlay(controller: EditorController, modifier: Modifier) {
    val settings = controller.transformSettings
    if (!settings.proportional && settings.pivot != com.smitnk.projectgrease.editor.ProjectGreaseSelect.PIVOT_CURSOR) return
    androidx.compose.foundation.Canvas(modifier.testTag("transformOverlay")) {
        controller.overlayTick
        val doc = controller.document
        fun toView(x: Float, y: Float) = com.smitnk.projectgrease.editor.CanvasMapping.toView(
            x, y, size.width, size.height, doc.canvasWidth, doc.canvasHeight, controller.view.zoom, controller.view.panX, controller.view.panY, controller.view.rotation)
        val ppu = com.smitnk.projectgrease.editor.CanvasMapping.pixelsPerUnit(size.width, size.height, doc.canvasWidth, doc.canvasHeight, controller.view.zoom)
        if (settings.proportional) {
            val c = controller.selectionPivot()
            if (c != null) {
                val (vx, vy) = toView(c[0], c[1])
                drawCircle(androidx.compose.ui.graphics.Color.White.copy(alpha = 0.7f), settings.size * ppu,
                    androidx.compose.ui.geometry.Offset(vx, vy), style = androidx.compose.ui.graphics.drawscope.Stroke(width = 2f))
            }
        }
        if (settings.pivot == com.smitnk.projectgrease.editor.ProjectGreaseSelect.PIVOT_CURSOR) {
            val (cx, cy) = toView(settings.cursorX, settings.cursorY)
            val o = androidx.compose.ui.geometry.Offset(cx, cy)
            drawCircle(androidx.compose.ui.graphics.Color.Red, 12f, o, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 3f))
            drawLine(androidx.compose.ui.graphics.Color.Black, o.copy(x = cx - 20f), o.copy(x = cx + 20f), 2f)
            drawLine(androidx.compose.ui.graphics.Color.Black, o.copy(y = cy - 20f), o.copy(y = cy + 20f), 2f)
        }
    }
}

internal class ProjectGreaseDrawingSurfaceView(
    context: android.content.Context,
    private val controller: EditorController
) : SurfaceView(context) {
    private var rendererHandle = 0L
    // the platform slop scales with screen density; a fixed 24 px was 24 dp on mdpi phones and 6 dp on xxxhdpi
    private val tapSlopPx = android.view.ViewConfiguration.get(context).scaledTouchSlop * 1.5f
    // Object / armature Edit / Pose touches (RigTouch.kt), Camera view only
    private val rigTouch = RigTouch(controller, ::canvasPoint) {
        RigController.SELECT_DIST_PX * resources.displayMetrics.density / pixelsPerUnit().coerceAtLeast(1e-3f) }
    // the 3D view's navigation / object touches (View3D.kt); the Camera view never reaches it
    private val view3dTouch = View3DTouch(context.resources.displayMetrics.density, tapSlopPx, { controller.render() }) { e ->
        val cancel = MotionEvent.obtain(e).apply { action = MotionEvent.ACTION_CANCEL }
        handleTouch(cancel)
        cancel.recycle()
    }

    /** The one gesture this view has open, with its own data (the ~25 flags it replaces could disagree). */
    private sealed class Gesture {
        object Idle : Gesture()
        /** A touch that landed during playback: nothing starts until it is known to be a view gesture or an edit. */
        class Deferred(val down: MotionEvent) : Gesture()
        /** Native tool session (Draw, Sculpt, Vertex Paint, Weight Paint). */
        class Session(val tool: Int) : Gesture()
        /** Controller stroke (primitives, lasso, box, gizmo handles...). */
        object Stroke : Gesture()
        /** One undo step per erase gesture (endErase only ever pairs with a beginErase). */
        class Erase(var lastX: Float, var lastY: Float) : Gesture()
        /** Filled on finger up: a second finger (pinch / undo tap) cancels it. */
        class Fill(val downX: Float, val downY: Float) : Gesture()
        class Pan(var lastX: Float, var lastY: Float) : Gesture()
        /** Move / Rotate / Scale / Mirror: this view opened a history batch for it and closes exactly that one. */
        sealed class Transform : Gesture() {
            /** Pinch during a proportional transform changes the proportional size (Blender: wheel / PageUp). */
            var propPinch = false
            var propStartDistance = 0f
            var propStartSize = 1f
        }
        class Move(var lastX: Float, var lastY: Float) : Transform()
        class Rotate(val cx: Float, val cy: Float, var lastAngle: Float) : Transform()
        class Scale(val cx: Float, val cy: Float, var lastRadius: Float) : Transform()
        class Mirror(val cx: Float, val cy: Float, val startX: Float, val startY: Float) : Transform()
        /** Two or more fingers: zoom / pan / rotate while [pinching]; a quick tap undoes (2) or redoes (3). */
        class Multi(var fingers: Int, var startMs: Long) : Gesture() {
            var moved = 0f
            var pinching = true
            var startDistance = 0f
            var startZoom = 1f
            var startAngle = 0f
            var startCx = 0f
            var startCy = 0f
            var startRot = 0f
            var startPanX = 0f
            var startPanY = 0f
        }
    }
    private var gesture: Gesture = Gesture.Idle

    // Pointer bookkeeping (which contact is which), shared by every gesture.
    private var activePointerId = MotionEvent.INVALID_POINTER_ID
    /** Pointer ids of ghost touches (second contacts at a finger's own spot), ignored until they lift. */
    private val ghostPointers = HashSet<Int>()
    /** A second contact that landed while drawing: it only becomes a pinch / pan once it moves
     *  (log 20: a resting finger or palm, size 0 on this phone, cancelled every stroke). */
    private var pendingPointer = MotionEvent.INVALID_POINTER_ID
    private var pendingX = 0f
    private var pendingY = 0f
    private var pendingLateMs = 0L // second touch's delay after the first
    /** Where the drawing contact touched down (screen px): a contact still there is resting. */
    private var activeDownX = 0f
    private var activeDownY = 0f
    private var sessionStartMs = 0L

    fun setRendererHandle(handle: Long) {
        rendererHandle = handle
    }

    fun getRendererHandle(): Long = rendererHandle

    /** Activity log: each touch down / up (tool, mode, canvas point), how late Android delivered it
     *  (the main thread was busy) and how long handling took; moves only when slow. */
    override fun onTouchEvent(event: MotionEvent): Boolean {
        val late = android.os.SystemClock.uptimeMillis() - event.eventTime
        val t0 = System.nanoTime()
        val r = handleTouch(event)
        val ms = (System.nanoTime() - t0) / 1_000_000
        val a = event.actionMasked
        if (a == MotionEvent.ACTION_DOWN || a == MotionEvent.ACTION_UP || a == MotionEvent.ACTION_POINTER_DOWN || ms > 100 || late > 150) {
            val c = canvasPoint(event.x, event.y)
            com.smitnk.projectgrease.CrashLog.event("TOUCH ${MotionEvent.actionToString(a)} x${event.pointerCount} tool ${controller.tools.activeTool} " +
                "mode ${controller.mode} canvas %.0f,%.0f late $late ms handled $ms ms".format(c.first, c.second) +
                (if (a == MotionEvent.ACTION_DOWN && (c.first < 0f || c.second < 0f || c.first > controller.document.canvasWidth ||
                    c.second > controller.document.canvasHeight)) " off-canvas" else ""))
        }
        return r
    }

    /** A deferred touch turned out to edit: playback stops and the touch starts for real where it landed. */
    private fun startDeferred(d: Gesture.Deferred) {
        controller.stopPlayback()
        handleTouch(d.down)
        d.down.recycle()
    }

    private fun handleTouch(event: MotionEvent): Boolean {
        if (rendererHandle == 0L) return true
        if (View3DUi.active && view3dTouch.handle(event)) return true
        if (!View3DUi.active && rigTouch.handle(event)) return true

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                // a gesture whose UP never arrived (Compose took the touch, a dialog opened) must not leak into this one
                if (activePointerId != MotionEvent.INVALID_POINTER_ID || gesture !== Gesture.Idle) {
                    when (gesture) {
                        is Gesture.Session -> endSession(cancel = true)
                        is Gesture.Stroke -> controller.cancelStroke()
                        is Gesture.Erase -> controller.endErase()
                        else -> {}
                    }
                    resetGestureState()
                }
                // safety net: no batch outlives a gesture (a code path that threw between begin and end would keep every
                // later edit queued, never recorded)
                controller.history.endAllBatches()
                ghostPointers.clear()
                pendingPointer = MotionEvent.INVALID_POINTER_ID
                // playback and editing do not mix (the tick would move the native frame under the stroke), but
                // a view gesture does not edit: during playback the touch waits until it is known which it is
                if (controller.animation.playing && controller.tools.activeTool != com.smitnk.projectgrease.editor.GreaseTool.PAN) {
                    activePointerId = event.getPointerId(0)
                    activeDownX = event.x; activeDownY = event.y
                    gesture = Gesture.Deferred(MotionEvent.obtain(event))
                    return true
                }
                if (controller.tools.activeTool != com.smitnk.projectgrease.editor.GreaseTool.PAN) controller.stopPlayback()
                controller.screenPxPerUnit = pixelsPerUnit()
                controller.touchRadiusPx = 40f * resources.displayMetrics.density
                controller.uiScale = resources.displayMetrics.density
                val start = canvasPoint(event.x, event.y)
                if (controller.placingCursor2D) {
                    controller.setCursor2D(start.first, start.second)
                    controller.placingCursor2D = false
                    return true
                }
                if (controller.weightSampleArmed) { controller.sampleWeightAt(start.first, start.second); return true }
                activePointerId = event.getPointerId(0)
                activeDownX = event.x; activeDownY = event.y
                // Draw, Sculpt, Vertex Paint and Weight Paint run in the native tool session: one call
                // per input batch with every sample; native applies the tool and renders.
                val tool = controller.sessionToolForTouch()
                if (tool >= 0) {
                    sessionStartMs = event.downTime
                    val samples = batch(event, 0, includeHistory = false, tool = tool)
                    val result = controller.toolSamples(tool, samples, samples.size / 4,
                        com.smitnk.projectgrease.editor.ToolSession.PHASE_BEGIN, pixelsPerUnit())
                    if (result != 0) gesture = Gesture.Session(tool) else activePointerId = MotionEvent.INVALID_POINTER_ID
                    return true
                }
                // Modifier handles being edited on the canvas take the touch whatever the tool is.
                if (controller.gizmoActive) {
                    if (controller.beginStroke()) { gesture = Gesture.Stroke; addPoint(event, 0); controller.render() }
                    else activePointerId = MotionEvent.INVALID_POINTER_ID
                    return true
                }
                when (controller.tools.activeTool) {
                    com.smitnk.projectgrease.editor.GreaseTool.SELECT -> {
                        controller.hitTestAndSelectStroke(start.first, start.second)
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.ERASE -> {
                        // one undo step per erase gesture (endErase), so a second finger can still revert it
                        controller.beginErase()
                        gesture = Gesture.Erase(start.first, start.second)
                        if (controller.eraseAt(start.first, start.second, controller.eraserRadius(pixelsPerUnit()), false)) controller.render()
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.FILL -> {
                        if (event.eventTime >= controller.lastFillEndMs) gesture = Gesture.Fill(event.x, event.y)
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.EYEDROPPER -> {
                        controller.pickColorAt(event.x.toInt(), event.y.toInt())
                        controller.render()
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.MOVE,
                    com.smitnk.projectgrease.editor.GreaseTool.ROTATE,
                    com.smitnk.projectgrease.editor.GreaseTool.SCALE,
                    com.smitnk.projectgrease.editor.GreaseTool.MIRROR -> {
                        // Click-select + transform drag = one undo step (Blender's tweak transform).
                        val kept = keepsSelection(start)
                        // the batch opens before the click-select: the select records an edit of its own, which would be a second undo step
                        controller.history.beginBatch()
                        if (kept || controller.hitTestAndSelectStroke(start.first, start.second)) {
                            beginTransformEdit(start, kept)
                            val t = transformFor(controller.tools.activeTool, start)
                            if (t != null) { gesture = t; controller.render() }
                            else controller.history.endBatch() // nothing to turn / scale about: close the batch it opened
                        } else controller.history.endBatch() // nothing hit
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.PAN -> {
                        gesture = Gesture.Pan(event.x, event.y)
                    }
                    else -> {
                        if (controller.beginStroke()) {
                            gesture = Gesture.Stroke
                            addPoint(event, 0)
                            controller.render()
                        } else {
                            activePointerId = MotionEvent.INVALID_POINTER_ID
                        }
                    }
                }
                return true
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                val g0 = gesture
                if (event.pointerCount >= 2 && g0 is Gesture.Transform && g0 !is Gesture.Mirror && controller.transformSettings.proportional) {
                    g0.propPinch = true
                    g0.propStartDistance = pointerDistance(event).coerceAtLeast(1f)
                    g0.propStartSize = controller.transformSettings.size
                    return true
                }
                val drawing = g0 is Gesture.Session || g0 is Gesture.Stroke
                // Reported (activity log): on this phone a second touch arrived 20-40 ms after almost every
                // stroke start, at the drawing finger's own spot, and cancelled the stroke as a pinch. A
                // real second finger cannot land that close: ignore it and keep drawing.
                if (event.pointerCount >= 2) {
                    val n = event.actionIndex
                    val near = event.pointerCount == 2 && (0 until event.pointerCount).any { i -> i != n &&
                        hypot(event.getX(n) - event.getX(i), event.getY(n) - event.getY(i)) < 40f * resources.displayMetrics.density }
                    // palm rejection: a palm resting on the screen while drawing is a contact much larger
                    // than the drawing fingertip (a pinch uses two fingertips of similar size)
                    val a = event.findPointerIndex(activePointerId)
                    val palm = a >= 0 && a != n && event.getTouchMajor(n) > maxOf(2f * event.getTouchMajor(a), 22f * resources.displayMetrics.density)
                    com.smitnk.projectgrease.CrashLog.event("TOUCH second touch: " + (0 until event.pointerCount).joinToString { i ->
                        "id ${event.getPointerId(i)} at %.0f,%.0f size %.0f".format(event.getX(i), event.getY(i), event.getTouchMajor(i)) } +
                        " new index $n, active $activePointerId, pinch ${g0 is Gesture.Multi && g0.pinching} -> " +
                        if (near) "ignored (at another finger's spot)" else if (palm) "ignored (palm)"
                        else if (event.pointerCount == 2 && drawing) "pending (gesture only if it moves)" else "gesture")
                    if (near || palm) {
                        ghostPointers += event.getPointerId(n)
                        return true
                    }
                    if (event.pointerCount == 2 && drawing) {
                        pendingPointer = event.getPointerId(n); pendingX = event.getX(n); pendingY = event.getY(n)
                        pendingLateMs = event.eventTime - event.downTime
                        ghostPointers += pendingPointer
                        return true
                    }
                }
                startGesture(event)
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val d = gesture
                if (d is Gesture.Deferred) {
                    val i = event.findPointerIndex(activePointerId)
                    if (i >= 0 && hypot(event.getX(i) - activeDownX, event.getY(i) - activeDownY) > tapSlopPx) {
                        startDeferred(d)
                        return handleTouch(event)
                    }
                    return true
                }
                if (pendingPointer != MotionEvent.INVALID_POINTER_ID) {
                    val i = event.findPointerIndex(pendingPointer)
                    if (i >= 0 && event.pointerCount >= 2 &&
                        hypot(event.getX(i) - pendingX, event.getY(i) - pendingY) > 24f * resources.displayMetrics.density) {
                        val slop = 24f * resources.displayMetrics.density
                        val a = event.findPointerIndex(activePointerId)
                        // log 21: the first contact was a thumb / palm resting low on the screen and the
                        // finger that landed second drew; the resting one hands the touch over
                        // log 26: a pinch whose anchor finger stays still looked the same; pinch fingers land
                        // together (14-36 ms apart there), a resting palm is there well before the drawing finger
                        if (a >= 0 && pendingLateMs >= 150L && hypot(event.getX(a) - activeDownX, event.getY(a) - activeDownY) < slop) {
                            com.smitnk.projectgrease.CrashLog.event("TOUCH first contact resting, second moves -> the second draws")
                            val resting = activePointerId
                            val mover = pendingPointer
                            endSession(cancel = true)
                            if (gesture is Gesture.Stroke) { controller.cancelStroke(); gesture = Gesture.Idle }
                            val down = MotionEvent.obtain(event.downTime, event.eventTime, MotionEvent.ACTION_DOWN,
                                pendingX, pendingY, event.metaState)
                            handleTouch(down)
                            down.recycle()
                            if (activePointerId != MotionEvent.INVALID_POINTER_ID) activePointerId = mover
                            ghostPointers += resting
                            return true
                        }
                        com.smitnk.projectgrease.CrashLog.event("TOUCH pending second touch moved -> gesture")
                        ghostPointers.remove(pendingPointer)
                        pendingPointer = MotionEvent.INVALID_POINTER_ID
                        startGesture(event)
                        // the pinch is measured from where the fingers landed, not from where the second
                        // finger had got to by now: zoom / turn / pan were short by that first part
                        val m = gesture
                        if (event.pointerCount == 2 && m is Gesture.Multi) {
                            m.startDistance = hypot(pendingX - activeDownX, pendingY - activeDownY).coerceAtLeast(1f)
                            m.startAngle = atan2(pendingY - activeDownY, pendingX - activeDownX)
                            m.startCx = (activeDownX + pendingX) / 2f; m.startCy = (activeDownY + pendingY) / 2f
                        }
                        return true
                    }
                }
                val g = gesture
                if (g is Gesture.Transform && g.propPinch && event.pointerCount >= 2) {
                    val distance = pointerDistance(event)
                    if (distance > 0f) controller.setProportionalSize(g.propStartSize * (distance / g.propStartDistance))
                    return true
                }
                if (g is Gesture.Multi) {
                    if (g.pinching && event.pointerCount >= 2) {
                        val distance = pointerDistance(event)
                        val (cx, cy) = centroid(event)
                        var turn = pointerAngle(event) - g.startAngle
                        if (turn > PI.toFloat()) turn -= 2f * PI.toFloat() else if (turn < -PI.toFloat()) turn += 2f * PI.toFloat()
                        g.moved = maxOf(g.moved, abs(distance - g.startDistance) + hypot(cx - g.startCx, cy - g.startCy) +
                            abs(turn) * g.startDistance * 0.5f)
                        if (event.pointerCount == 2 && g.moved > tapSlopPx && g.startDistance > 0f && distance > 0f) {
                            controller.view.setZoom(g.startZoom * (distance / g.startDistance))
                            controller.view.setRotation((g.startRot + turn).let { r ->
                                // a zoom / pan pinch always turns a little: within 6 degrees of upright stays upright
                                val k = Math.round(r / (Math.PI / 2)).toFloat() * (Math.PI / 2).toFloat()
                                if (kotlin.math.abs(r - k) < 0.105f) k else r })
                            // the pan is applied before the view rotation: bring the finger delta into that frame
                            val (px, py) = com.smitnk.projectgrease.editor.CanvasMapping.rotate(cx - g.startCx, cy - g.startCy, 0f, 0f, -controller.view.rotation)
                            controller.view.setPan(g.startPanX + px, g.startPanY + py)
                            controller.render()
                        }
                        return true
                    }
                    // after a multi-finger gesture the remaining finger does not draw / erase
                    if (g.fingers >= 2) return true
                }
                val pointerIndex = if (activePointerId != MotionEvent.INVALID_POINTER_ID) {
                    event.findPointerIndex(activePointerId)
                } else -1
                if (pointerIndex >= 0) {
                    val rawX = event.getX(pointerIndex)
                    val rawY = event.getY(pointerIndex)
                    val canvas = canvasPoint(rawX, rawY)
                    val x = canvas.first
                    val y = canvas.second
                    when (g) {
                        is Gesture.Session -> {
                            val samples = batch(event, pointerIndex, includeHistory = true, tool = g.tool)
                            controller.toolSamples(g.tool, samples, samples.size / 4,
                                com.smitnk.projectgrease.editor.ToolSession.PHASE_MOVE)
                        }
                        is Gesture.Pan -> {
                            val dx = rawX - g.lastX
                            val dy = rawY - g.lastY
                            if (dx != 0f || dy != 0f) {
                                controller.view.panBy(dx, dy)
                                g.lastX = rawX
                                g.lastY = rawY
                                controller.render()
                            }
                        }
                        is Gesture.Erase -> {
                            val changed = eraseAlongPath(g, x, y)
                            if (changed) controller.render()
                        }
                        is Gesture.Move -> {
                            val dx = x - g.lastX
                            val dy = y - g.lastY
                            if (dx != 0f || dy != 0f) {
                                controller.translateSelectedStroke(dx, dy)
                                g.lastX = x
                                g.lastY = y
                                controller.render()
                            }
                        }
                        is Gesture.Rotate -> {
                            val angle = atan2(y - g.cy, x - g.cx)
                            var delta = angle - g.lastAngle
                            if (delta > PI.toFloat()) delta -= 2f * PI.toFloat()
                            else if (delta < -PI.toFloat()) delta += 2f * PI.toFloat()
                            if (delta != 0f) {
                                if (controller.rotateSelectedStrokeAround(delta, g.cx, g.cy)) {
                                    g.lastAngle = angle
                                    controller.render()
                                }
                            }
                        }
                        is Gesture.Scale -> {
                            val radius = hypot(x - g.cx, y - g.cy)
                            if (radius >= 1f && g.lastRadius >= 1f) {
                                var factor = radius / g.lastRadius
                                factor = factor.coerceIn(0.8f, 1.25f)
                                if (factor != 1f) {
                                    if (controller.scaleSelectedStrokeAround(factor, factor, g.cx, g.cy)) {
                                        g.lastRadius = radius
                                        controller.render()
                                    }
                                }
                            }
                        }
                        is Gesture.Mirror -> {
                            // Mirror is committed on release so a drag cannot
                            // accidentally flip the stroke multiple times.
                            controller.render()
                        }
                        is Gesture.Stroke -> {
                            addPointsWithHistory(event, pointerIndex)
                            // the lasso / box outline is a Compose overlay: the drawing itself has not
                            // changed, and a full redraw per move made lassoing lag with many strokes
                            val t = controller.tools.activeTool
                            if (t != com.smitnk.projectgrease.editor.GreaseTool.LASSO && t != com.smitnk.projectgrease.editor.GreaseTool.BOX_SELECT) controller.render()
                        }
                        else -> {}
                    }
                } else if (g is Gesture.Stroke) {
                    controller.cancelStroke()
                    gesture = Gesture.Idle
                    activePointerId = MotionEvent.INVALID_POINTER_ID
                    controller.render()
                }
                return true
            }
            MotionEvent.ACTION_POINTER_UP -> {
                if (event.getPointerId(event.actionIndex) == pendingPointer) {
                    // a second finger that landed with the first and lifted without moving is a
                    // two-finger tap (undo), not a resting touch: it never moved, so it never became a gesture
                    val a = event.findPointerIndex(activePointerId)
                    val still = a >= 0 && hypot(event.getX(a) - activeDownX, event.getY(a) - activeDownY) <= tapSlopPx
                    val quick = event.eventTime - event.downTime <= TAP_TIMEOUT_MS
                    ghostPointers.remove(pendingPointer)
                    pendingPointer = MotionEvent.INVALID_POINTER_ID
                    if (pendingLateMs < 150L && still && quick) {
                        startGesture(event)
                        (gesture as? Gesture.Multi)?.startMs = event.downTime
                        return true
                    }
                }
                if (ghostPointers.remove(event.getPointerId(event.actionIndex))) return true
                val g = gesture
                if (g is Gesture.Transform && g.propPinch) {
                    g.propPinch = false
                    if (event.getPointerId(event.actionIndex) != activePointerId) return true
                }
                if (g is Gesture.Multi && g.pinching) {
                    g.pinching = false
                    g.startDistance = 0f
                    g.startZoom = controller.view.zoom
                    return true
                }
                val pointerId = event.getPointerId(event.actionIndex)
                if (pointerId == activePointerId) {
                    val up = canvasPoint(event.getX(event.actionIndex), event.getY(event.actionIndex))
                    when (g) {
                        is Gesture.Session -> {
                            if ((event.flags and MotionEvent.FLAG_CANCELED) != 0) endSession(cancel = true)
                            else endSession(event, event.actionIndex)
                        }
                        is Gesture.Erase -> controller.endErase()
                        is Gesture.Mirror -> commitMirror(g, up.first, up.second)
                        is Gesture.Stroke -> {
                            if ((event.flags and MotionEvent.FLAG_CANCELED) != 0) {
                                controller.cancelStroke()
                            } else {
                                controller.endStroke()
                            }
                        }
                        else -> {}
                    }
                    resetGestureState()
                    controller.render()
                }
                return true
            }
            MotionEvent.ACTION_UP -> {
                val d = gesture
                if (d is Gesture.Deferred) {
                    // a tap during playback edits (a dot, a fill, a pick): stop and run it as one
                    startDeferred(d)
                    return handleTouch(event)
                }
                val g = gesture
                if (g is Gesture.Fill) {
                    canvasPoint(g.downX, g.downY).let { c -> controller.fillAt(g.downX, g.downY, c.first, c.second) }
                }
                if (g is Gesture.Multi && g.fingers >= 2) {
                    val tap = g.moved <= tapSlopPx && event.eventTime - g.startMs <= TAP_TIMEOUT_MS
                    if (tap) { controller.stopPlayback(); if (g.fingers == 2) controller.undo() else controller.redo() }
                    resetGestureState()
                    controller.render()
                    controller.onUiChanged?.invoke()
                    return true
                }
                val pointerIndex = event.findPointerIndex(activePointerId)
                when (g) {
                    is Gesture.Session -> if (pointerIndex >= 0) endSession(event, pointerIndex) else endSession(cancel = true)
                    is Gesture.Erase -> controller.endErase()
                    is Gesture.Mirror -> if (pointerIndex >= 0) {
                        val up = canvasPoint(event.getX(pointerIndex), event.getY(pointerIndex))
                        commitMirror(g, up.first, up.second)
                    }
                    is Gesture.Stroke -> if (pointerIndex >= 0) controller.endStroke() else controller.cancelStroke()
                    else -> {}
                }
                resetGestureState()
                controller.render()
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                val g = gesture
                endSession(cancel = g is Gesture.Session && g.tool == com.smitnk.projectgrease.editor.ToolSession.TOOL_DRAW)
                if (g is Gesture.Erase) controller.endErase()
                if (g is Gesture.Stroke) controller.cancelStroke()
                resetGestureState()
                controller.render()
                return true
            }
        }
        return true
    }

    /** The transform the tool's drag runs, about the selection pivot (null: nothing to turn / scale about). */
    private fun transformFor(tool: com.smitnk.projectgrease.editor.GreaseTool, start: Pair<Float, Float>): Gesture.Transform? {
        if (tool == com.smitnk.projectgrease.editor.GreaseTool.MOVE) return Gesture.Move(start.first, start.second)
        val center = controller.selectionPivot() ?: controller.selectedStrokeCenter()
        if (center == null || center.size < 2) return null
        val cx = center[0]; val cy = center[1]
        return when (tool) {
            com.smitnk.projectgrease.editor.GreaseTool.ROTATE -> Gesture.Rotate(cx, cy, atan2(start.second - cy, start.first - cx))
            com.smitnk.projectgrease.editor.GreaseTool.SCALE ->
                hypot(start.first - cx, start.second - cy).let { r -> if (r < 1f) null else Gesture.Scale(cx, cy, r) }
            else -> Gesture.Mirror(cx, cy, start.first, start.second)
        }
    }

    private fun commitMirror(g: Gesture.Mirror, x: Float, y: Float) {
        val dx = x - g.startX
        val dy = y - g.startY
        if (hypot(dx, dy) < 12f) return
        if (abs(dx) >= abs(dy)) {
            // Horizontal drag -> reflect across the stroke's vertical centerline.
            controller.mirrorSelectedStrokeAround(
                true, false, g.cx, g.cy
            )
        } else {
            // Vertical drag -> reflect across the stroke's horizontal centerline.
            controller.mirrorSelectedStrokeAround(
                false, true, g.cx, g.cy
            )
        }
    }

    private fun eraseAlongPath(g: Gesture.Erase, x: Float, y: Float): Boolean {
        val radius = controller.eraserRadius(pixelsPerUnit())
        val sx = g.lastX
        val sy = g.lastY
        val dx = x - sx
        val dy = y - sy
        val distance = hypot(dx, dy)
        val step = (radius * 0.35f).coerceAtLeast(2f)
        val samples = maxOf(1, kotlin.math.ceil(distance / step).toInt())
        var changed = false
        for (i in 1..samples) {
            val t = i.toFloat() / samples.toFloat()
            if (controller.eraseAt(sx + dx * t, sy + dy * t, radius, false)) {
                changed = true
            }
        }
        g.lastX = x
        g.lastY = y
        return changed
    }

    /**
     * Something was hit: only now does the transform key a held frame (inside the one-undo-step batch opened before the pick)
     * (a tap on empty canvas used to key the frame and leave a batch open). A new key is a copy of the
     * held drawing, so a stroke picked before it is picked again on the key.
     */
    private fun beginTransformEdit(start: Pair<Float, Float>, selectionKept: Boolean) {
        val keyed = controller.autokeyHeld()
        if (keyed && !selectionKept) controller.hitTestAndSelectStroke(start.first, start.second)
    }

    /** A Move / Rotate / Scale / Mirror touch on or near the lasso selection transforms that selection
     *  (Blender: G / R / S act on what is selected) instead of re-selecting the stroke under the finger. */
    private fun keepsSelection(p: Pair<Float, Float>): Boolean {
        val b = controller.selectionBounds() ?: return false
        val slop = 48f / pixelsPerUnit().coerceAtLeast(1e-3f)
        return p.first in b[0] - slop..b[2] + slop && p.second in b[1] - slop..b[3] + slop
    }

    private fun pointerAngle(event: MotionEvent): Float =
        if (event.pointerCount < 2) 0f else atan2(event.getY(1) - event.getY(0), event.getX(1) - event.getX(0))

    private fun centroid(event: MotionEvent): Pair<Float, Float> {
        var x = 0f; var y = 0f
        for (i in 0 until event.pointerCount) { x += event.getX(i); y += event.getY(i) }
        return x / event.pointerCount to y / event.pointerCount
    }

    private fun pointerDistance(event: MotionEvent): Float {
        if (event.pointerCount < 2) return 0f
        val dx = event.getX(0) - event.getX(1)
        val dy = event.getY(0) - event.getY(1)
        return hypot(dx, dy)
    }

    /** Two or more fingers: the stroke in progress is dropped and the pinch / pan / turn starts. */
    private fun startGesture(event: MotionEvent) {
        if (event.pointerCount < 2) return
        val g = gesture
        when (g) {
            is Gesture.Session -> endSession(cancel = g.tool == com.smitnk.projectgrease.editor.ToolSession.TOOL_DRAW)
            is Gesture.Stroke -> controller.cancelStroke()
            // a transform drag in progress ends here as its own undo step, so the two-finger tap
            // (undo) runs with no batch open and undoes exactly that drag
            is Gesture.Transform -> controller.history.endBatch()
            // the first finger's erase was not meant: revert it before the gesture
            is Gesture.Erase -> controller.cancelErase()
            is Gesture.Deferred -> g.down.recycle()
            else -> {}
        }
        val m = if (g is Gesture.Multi && g.fingers >= 2) g else Gesture.Multi(0, event.eventTime)
        m.fingers = maxOf(m.fingers, event.pointerCount)
        m.pinching = true
        m.startDistance = pointerDistance(event).coerceAtLeast(1f)
        m.startZoom = controller.view.zoom
        m.startAngle = pointerAngle(event)
        centroid(event).let { m.startCx = it.first; m.startCy = it.second }
        m.startRot = controller.view.rotation
        m.startPanX = controller.view.panX
        m.startPanY = controller.view.panY
        gesture = m
    }

    private fun canvasPoint(rawX:Float,rawY:Float):Pair<Float,Float> =
        com.smitnk.projectgrease.editor.CanvasMapping.toCanvas(
            rawX, rawY, width.toFloat(), height.toFloat(),
            controller.document.canvasWidth, controller.document.canvasHeight,
            controller.view.zoom, controller.view.panX, controller.view.panY, clamp = false, rotation = controller.view.rotation)

    /** On-screen pixels per canvas unit: the brushes' pixel radius and hit tests use it. */
    private fun pixelsPerUnit():Float =
        com.smitnk.projectgrease.editor.CanvasMapping.pixelsPerUnit(
            width.toFloat(), height.toFloat(), controller.document.canvasWidth,
            controller.document.canvasHeight, controller.view.zoom)

    /** Every sample of this event for one pointer (historical first), in canvas units. */
    private fun batch(event: MotionEvent, pointerIndex: Int, includeHistory: Boolean, tool: Int): FloatArray =
        com.smitnk.projectgrease.editor.ToolSampleBatch.from(
            MotionTouch(event, pointerIndex), sessionStartMs, isPenTool(event.getToolType(pointerIndex)), includeHistory
        ) { rx, ry -> controller.snapForTool(tool, canvasPoint(rx, ry)) }

    /** Ends the session gesture with this event's samples (brushes) or none (Draw, like before). */
    private fun endSession(event: MotionEvent, pointerIndex: Int) {
        val g = gesture as? Gesture.Session ?: return
        val samples = if (g.tool == com.smitnk.projectgrease.editor.ToolSession.TOOL_DRAW) FloatArray(0)
            else batch(event, pointerIndex, includeHistory = true, tool = g.tool)
        controller.toolSamples(g.tool, samples, samples.size / 4, com.smitnk.projectgrease.editor.ToolSession.PHASE_END)
        gesture = Gesture.Idle
    }

    private fun endSession(cancel: Boolean) {
        val g = gesture as? Gesture.Session ?: return
        controller.toolSamples(g.tool, FloatArray(0), 0,
            if (cancel) com.smitnk.projectgrease.editor.ToolSession.PHASE_CANCEL else com.smitnk.projectgrease.editor.ToolSession.PHASE_END)
        gesture = Gesture.Idle
    }

    init {
        controller.onCancelGestures = { resetGestureState() }
        controller.isGestureOpenInView = {
            val g = gesture
            activePointerId != MotionEvent.INVALID_POINTER_ID || g is Gesture.Stroke || (g is Gesture.Multi && g.pinching)
        }
    }
    private fun resetGestureState() {
        if (gesture is Gesture.Transform) controller.history.endBatch()
        gesture = Gesture.Idle
        activePointerId = MotionEvent.INVALID_POINTER_ID
    }

    private fun isPenTool(toolType: Int) =
        toolType == MotionEvent.TOOL_TYPE_STYLUS || toolType == MotionEvent.TOOL_TYPE_ERASER

    private fun addPoint(event: MotionEvent, pointerIndex: Int) {
        addSample(
            event.getX(pointerIndex),
            event.getY(pointerIndex),
            event.getPressure(pointerIndex),
            event.getToolType(pointerIndex),
            event.eventTime,
            event.downTime
        )
    }

    /** Android batches touch samples between frames; each one is a real input event. */
    private fun addPointsWithHistory(event: MotionEvent, pointerIndex: Int) {
        for (h in 0 until event.historySize) {
            addSample(
                event.getHistoricalX(pointerIndex, h),
                event.getHistoricalY(pointerIndex, h),
                event.getHistoricalPressure(pointerIndex, h),
                event.getToolType(pointerIndex),
                event.getHistoricalEventTime(h),
                event.downTime
            )
        }
        addPoint(event, pointerIndex)
    }

    private fun addSample(
        rawX: Float,
        rawY: Float,
        rawPressure: Float,
        toolType: Int,
        eventTimeMs: Long,
        gestureStartMs: Long
    ) {
        val isPen = toolType == MotionEvent.TOOL_TYPE_STYLUS ||
            toolType == MotionEvent.TOOL_TYPE_ERASER
        val p = canvasPoint(rawX, rawY)
        controller.addStrokePoint(
            p.first,
            p.second,
            com.smitnk.projectgrease.editor.TouchInputRules.pressureFor(isPen, rawPressure),
            com.smitnk.projectgrease.editor.TouchInputRules.elapsedSeconds(eventTimeMs, gestureStartMs)
        )
    }
}

/** MotionEvent pointer as a TouchHistory: index historySize is the current sample. */
private class MotionTouch(private val event: MotionEvent, private val pointerIndex: Int) :
    com.smitnk.projectgrease.editor.TouchHistory {
    override val historySize: Int get() = event.historySize
    override fun x(h: Int) = if (h < event.historySize) event.getHistoricalX(pointerIndex, h) else event.getX(pointerIndex)
    override fun y(h: Int) = if (h < event.historySize) event.getHistoricalY(pointerIndex, h) else event.getY(pointerIndex)
    override fun pressure(h: Int) =
        if (h < event.historySize) event.getHistoricalPressure(pointerIndex, h) else event.getPressure(pointerIndex)
    override fun timeMs(h: Int) = if (h < event.historySize) event.getHistoricalEventTime(h) else event.eventTime
}
