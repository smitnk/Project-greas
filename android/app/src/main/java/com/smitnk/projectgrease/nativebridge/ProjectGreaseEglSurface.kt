package com.smitnk.projectgrease.nativebridge

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
private const val TAP_SLOP_PX = 24f
private const val TAP_TIMEOUT_MS = 350L

@Composable
fun ProjectGreaseEglViewport(
    modifier: Modifier = Modifier,
    controller: EditorController
) {
  androidx.compose.foundation.layout.Box(modifier) {
    AndroidView(
        modifier = Modifier.matchParentSize(),
        factory = { context ->
            val view = ProjectGreaseDrawingSurfaceView(context, controller)
            view.holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(holder: SurfaceHolder) {
                    // One renderer (GL context + Blender document) for the editor's lifetime: a surface
                    // coming back (picker closed, app resumed) attaches to the same document.
                    val existing = controller.rendererHandleOrZero()
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
                    // only the window surface goes; the document stays (see detach_surface_keep_document)
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
            val handle = controller.rendererHandleOrZero()
            if (handle != 0L) {
                controller.detachRenderer()
                GPNative.nativeDetachSurface(handle)
                GPNative.nativeDestroyEglRenderer(handle)
            }
        }
    }
    TransformOverlay(controller, Modifier.matchParentSize())
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
    private var strokeOpen = false
    private var activePointerId = MotionEvent.INVALID_POINTER_ID
    private var moveOpen = false
    private var lastMoveX = 0f
    private var lastMoveY = 0f
    private var rotateOpen = false
    private var rotateCenterX = 0f
    private var rotateCenterY = 0f
    private var lastRotateAngle = 0f
    private var scaleOpen = false
    private var scaleCenterX = 0f
    private var scaleCenterY = 0f
    private var lastScaleRadius = 0f
    private var scaleAccumulated = 1f
    private var mirrorOpen = false
    private var mirrorCenterX = 0f
    private var mirrorCenterY = 0f
    private var mirrorStartX = 0f
    private var mirrorStartY = 0f
    private var panOpen = false
    // Native tool session gesture (Draw, Sculpt, Vertex Paint, Weight Paint): its tool, or -1.
    private var sessionTool = -1
    private var sessionStartMs = 0L
    private var lastPanX = 0f
    private var eraseLastX = Float.NaN
    private var eraseLastY = Float.NaN
    private var lastPanY = 0f
    private var pinchOpen = false
    /** Pointer ids of ghost touches (second contacts at a finger's own spot), ignored until they lift. */
    private val ghostPointers = HashSet<Int>()
    private var pinchStartDistance = 0f
    private var pinchStartZoom = 1f
    /** Pinch during a proportional transform changes the proportional size (Blender: wheel / PageUp). */
    private var propPinch = false
    private var pinchStartPropSize = 1f
    // Multi-finger gestures: two-finger zoom / pan / rotate; a quick two-finger tap undoes, three redoes.
    private var multiMax = 0
    private var multiStartMs = 0L
    private var multiMoved = 0f
    private var pinchStartAngle = 0f
    private var pinchStartCx = 0f
    private var pinchStartCy = 0f
    private var pinchStartRot = 0f
    private var pinchStartPanX = 0f
    private var pinchStartPanY = 0f

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
                "mode ${controller.mode} canvas %.0f,%.0f late $late ms handled $ms ms".format(c.first, c.second))
        }
        return r
    }

    private fun handleTouch(event: MotionEvent): Boolean {
        if (rendererHandle == 0L) return true

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                ghostPointers.clear()
                controller.screenPxPerUnit = pixelsPerUnit()
                controller.touchRadiusPx = 40f * resources.displayMetrics.density
                controller.uiScale = resources.displayMetrics.density
                val start = canvasPoint(event.x, event.y)
                if (controller.placingCursor2D) {
                    controller.setCursor2D(start.first, start.second)
                    controller.placingCursor2D = false
                    return true
                }
                activePointerId = event.getPointerId(0)
                // Draw, Sculpt, Vertex Paint and Weight Paint run in the native tool session: one call
                // per input batch with every sample; native applies the tool and renders.
                val tool = controller.sessionToolForTouch()
                if (tool >= 0) {
                    sessionStartMs = event.downTime
                    val samples = batch(event, 0, includeHistory = false, tool = tool)
                    val result = controller.toolSamples(tool, samples, samples.size / 4,
                        com.smitnk.projectgrease.editor.ToolSession.PHASE_BEGIN, pixelsPerUnit())
                    sessionTool = if (result != 0) tool else -1
                    if (sessionTool < 0) activePointerId = MotionEvent.INVALID_POINTER_ID
                    return true
                }
                // Modifier handles being edited on the canvas take the touch whatever the tool is.
                if (controller.gizmoActive) {
                    strokeOpen = controller.beginStroke()
                    if (strokeOpen) { addPoint(event, 0); controller.render() }
                    else activePointerId = MotionEvent.INVALID_POINTER_ID
                    return true
                }
                when (controller.tools.activeTool) {
                    com.smitnk.projectgrease.editor.GreaseTool.SELECT -> {
                        controller.hitTestAndSelectStroke(start.first, start.second)
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.ERASE -> {
                        // one undo step per erase gesture (endErase), so a second finger can still revert it
                        if (controller.eraseAt(start.first, start.second, controller.eraserRadius(pixelsPerUnit()), false)) controller.render()
                        eraseLastX = start.first
                        eraseLastY = start.second
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.FILL -> {
                        if (event.eventTime >= controller.lastFillEndMs) {
                            controller.fillAt(event.x, event.y)
                            controller.render()
                        }
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.EYEDROPPER -> {
                        controller.pickColorAt(event.x.toInt(), event.y.toInt())
                        controller.render()
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.MOVE -> {
                        // Click-select + transform drag = one undo step (Blender's tweak transform).
                        controller.history.beginBatch()
                        moveOpen = keepsSelection(start) || controller.hitTestAndSelectStroke(start.first, start.second)
                        if (moveOpen) {
                            lastMoveX = start.first
                            lastMoveY = start.second
                            controller.render()
                        }
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.ROTATE -> {
                        // Click-select + transform drag = one undo step (Blender's tweak transform).
                        controller.history.beginBatch()
                        rotateOpen = keepsSelection(start) || controller.hitTestAndSelectStroke(start.first, start.second)
                        if (rotateOpen) {
                            val center = controller.selectionPivot() ?: controller.selectedStrokeCenter()
                            if (center == null || center.size < 2) {
                                rotateOpen = false
                            } else {
                                rotateCenterX = center[0]
                                rotateCenterY = center[1]
                                lastRotateAngle = atan2(
                                    start.second - rotateCenterY,
                                    start.first - rotateCenterX
                                )
                                controller.render()
                            }
                        }
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.SCALE -> {
                        // Click-select + transform drag = one undo step (Blender's tweak transform).
                        controller.history.beginBatch()
                        scaleOpen = keepsSelection(start) || controller.hitTestAndSelectStroke(start.first, start.second)
                        if (scaleOpen) {
                            val center = controller.selectionPivot() ?: controller.selectedStrokeCenter()
                            if (center == null || center.size < 2) {
                                scaleOpen = false
                            } else {
                                scaleCenterX = center[0]
                                scaleCenterY = center[1]
                                lastScaleRadius = hypot(
                                    start.first - scaleCenterX,
                                    start.second - scaleCenterY
                                )
                                scaleAccumulated = 1f
                                if (lastScaleRadius < 1f) scaleOpen = false
                                else controller.render()
                            }
                        }
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.MIRROR -> {
                        // Click-select + transform drag = one undo step (Blender's tweak transform).
                        controller.history.beginBatch()
                        mirrorOpen = keepsSelection(start) || controller.hitTestAndSelectStroke(start.first, start.second)
                        if (mirrorOpen) {
                            val center = controller.selectionPivot() ?: controller.selectedStrokeCenter()
                            if (center == null || center.size < 2) {
                                mirrorOpen = false
                            } else {
                                mirrorCenterX = center[0]
                                mirrorCenterY = center[1]
                                mirrorStartX = start.first
                                mirrorStartY = start.second
                                controller.render()
                            }
                        }
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.PAN -> {
                        panOpen = true
                        lastPanX = event.x
                        lastPanY = event.y
                    }
                    else -> {
                        strokeOpen = controller.beginStroke()
                        if (strokeOpen) {
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
                if (event.pointerCount >= 2 && (moveOpen || rotateOpen || scaleOpen) && controller.transformSettings.proportional) {
                    propPinch = true
                    pinchStartDistance = pointerDistance(event).coerceAtLeast(1f)
                    pinchStartPropSize = controller.transformSettings.size
                    return true
                }
                // Reported (activity log): on this phone a second touch arrived 20-40 ms after almost every
                // stroke start, at the drawing finger's own spot, and cancelled the stroke as a pinch. A
                // real second finger cannot land that close: ignore it and keep drawing.
                if (event.pointerCount >= 2) {
                    val n = event.actionIndex
                    val near = (0 until event.pointerCount).any { i -> i != n &&
                        hypot(event.getX(n) - event.getX(i), event.getY(n) - event.getY(i)) < 40f * resources.displayMetrics.density }
                    // palm rejection: a palm resting on the screen while drawing is a contact much larger
                    // than the drawing fingertip (a pinch uses two fingertips of similar size)
                    val a = event.findPointerIndex(activePointerId)
                    val palm = a >= 0 && a != n && event.getTouchMajor(n) > maxOf(2f * event.getTouchMajor(a), 22f * resources.displayMetrics.density)
                    com.smitnk.projectgrease.CrashLog.event("TOUCH second touch: " + (0 until event.pointerCount).joinToString { i ->
                        "id ${event.getPointerId(i)} at %.0f,%.0f size %.0f".format(event.getX(i), event.getY(i), event.getTouchMajor(i)) } + " new index $n, active $activePointerId, pinch $pinchOpen -> " +
                        if (near) "ignored (at another finger's spot)" else if (palm) "ignored (palm)" else "gesture")
                    if (near || palm) {
                        ghostPointers += event.getPointerId(n)
                        return true
                    }
                }
                if (event.pointerCount >= 2) {
                    endSession(cancel = sessionTool == com.smitnk.projectgrease.editor.ToolSession.TOOL_DRAW)
                    if (strokeOpen) {
                        controller.cancelStroke()
                        strokeOpen = false
                    }
                    panOpen = false
                    if (multiMax < 2) {
                        // the first finger's erase was not meant: revert it before the gesture
                        if (controller.tools.activeTool == com.smitnk.projectgrease.editor.GreaseTool.ERASE && controller.endErase()) controller.undo()
                        multiStartMs = event.eventTime
                    }
                    multiMax = maxOf(multiMax, event.pointerCount)
                    pinchOpen = true
                    pinchStartDistance = pointerDistance(event).coerceAtLeast(1f)
                    pinchStartZoom = controller.view.zoom
                    pinchStartAngle = pointerAngle(event)
                    centroid(event).let { pinchStartCx = it.first; pinchStartCy = it.second }
                    pinchStartRot = controller.view.rotation
                    pinchStartPanX = controller.view.panX
                    pinchStartPanY = controller.view.panY
                }
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (propPinch && event.pointerCount >= 2) {
                    val distance = pointerDistance(event)
                    if (distance > 0f) controller.setProportionalSize(pinchStartPropSize * (distance / pinchStartDistance))
                    return true
                }
                if (pinchOpen && event.pointerCount >= 2) {
                    val distance = pointerDistance(event)
                    val (cx, cy) = centroid(event)
                    var turn = pointerAngle(event) - pinchStartAngle
                    if (turn > PI.toFloat()) turn -= 2f * PI.toFloat() else if (turn < -PI.toFloat()) turn += 2f * PI.toFloat()
                    multiMoved = maxOf(multiMoved, abs(distance - pinchStartDistance) + hypot(cx - pinchStartCx, cy - pinchStartCy) +
                        abs(turn) * pinchStartDistance * 0.5f)
                    if (event.pointerCount == 2 && multiMoved > TAP_SLOP_PX && pinchStartDistance > 0f && distance > 0f) {
                        controller.view.setZoom(pinchStartZoom * (distance / pinchStartDistance))
                        controller.view.setRotation((pinchStartRot + turn).let { r ->
                            // a zoom / pan pinch always turns a little: within 6 degrees of upright stays upright
                            val k = Math.round(r / (Math.PI / 2)).toFloat() * (Math.PI / 2).toFloat()
                            if (kotlin.math.abs(r - k) < 0.105f) k else r })
                        // the pan is applied before the view rotation: bring the finger delta into that frame
                        val (px, py) = com.smitnk.projectgrease.editor.CanvasMapping.rotate(cx - pinchStartCx, cy - pinchStartCy, 0f, 0f, -controller.view.rotation)
                        controller.view.setPan(pinchStartPanX + px, pinchStartPanY + py)
                        controller.render()
                    }
                    return true
                }
                // after a multi-finger gesture the remaining finger does not draw / erase
                if (multiMax >= 2) return true
                val pointerIndex = if (activePointerId != MotionEvent.INVALID_POINTER_ID) {
                    event.findPointerIndex(activePointerId)
                } else -1
                if (pointerIndex >= 0) {
                    val rawX = event.getX(pointerIndex)
                    val rawY = event.getY(pointerIndex)
                    val canvas = canvasPoint(rawX, rawY)
                    val x = canvas.first
                    val y = canvas.second
                    when {
                        sessionTool >= 0 -> {
                            val samples = batch(event, pointerIndex, includeHistory = true, tool = sessionTool)
                            controller.toolSamples(sessionTool, samples, samples.size / 4,
                                com.smitnk.projectgrease.editor.ToolSession.PHASE_MOVE)
                        }
                        panOpen -> {
                            val dx = rawX - lastPanX
                            val dy = rawY - lastPanY
                            if (dx != 0f || dy != 0f) {
                                controller.view.panBy(dx, dy)
                                lastPanX = rawX
                                lastPanY = rawY
                                controller.render()
                            }
                        }
                        controller.tools.activeTool == com.smitnk.projectgrease.editor.GreaseTool.ERASE && !strokeOpen -> {
                            val changed = eraseAlongPath(x, y)
                            if (changed) controller.render()
                        }
                        moveOpen -> {
                            val dx = x - lastMoveX
                            val dy = y - lastMoveY
                            if (dx != 0f || dy != 0f) {
                                controller.translateSelectedStroke(dx, dy)
                                lastMoveX = x
                                lastMoveY = y
                                controller.render()
                            }
                        }
                        rotateOpen -> {
                            val angle = atan2(y - rotateCenterY, x - rotateCenterX)
                            var delta = angle - lastRotateAngle
                            if (delta > PI.toFloat()) delta -= 2f * PI.toFloat()
                            else if (delta < -PI.toFloat()) delta += 2f * PI.toFloat()
                            if (delta != 0f) {
                                if (controller.rotateSelectedStrokeAround(
                                        delta, rotateCenterX, rotateCenterY
                                    )
                                ) {
                                    lastRotateAngle = angle
                                    controller.render()
                                }
                            }
                        }
                        scaleOpen -> {
                            val radius = hypot(x - scaleCenterX, y - scaleCenterY)
                            if (radius >= 1f && lastScaleRadius >= 1f) {
                                var factor = radius / lastScaleRadius
                                factor = factor.coerceIn(0.8f, 1.25f)
                                if (factor != 1f) {
                                    if (controller.scaleSelectedStrokeAround(
                                            factor, factor, scaleCenterX, scaleCenterY
                                        )
                                    ) {
                                        scaleAccumulated *= factor
                                        lastScaleRadius = radius
                                        controller.render()
                                    }
                                }
                            }
                        }
                        mirrorOpen -> {
                            // Mirror is committed on release so a drag cannot
                            // accidentally flip the stroke multiple times.
                            controller.render()
                        }
                        strokeOpen -> {
                            addPointsWithHistory(event, pointerIndex)
                            // the lasso / box outline is a Compose overlay: the drawing itself has not
                            // changed, and a full redraw per move made lassoing lag with many strokes
                            val t = controller.tools.activeTool
                            if (t != com.smitnk.projectgrease.editor.GreaseTool.LASSO && t != com.smitnk.projectgrease.editor.GreaseTool.BOX_SELECT) controller.render()
                        }
                    }
                } else if (strokeOpen) {
                    controller.cancelStroke()
                    strokeOpen = false
                    activePointerId = MotionEvent.INVALID_POINTER_ID
                    controller.render()
                }
                return true
            }
            MotionEvent.ACTION_POINTER_UP -> {
                if (ghostPointers.remove(event.getPointerId(event.actionIndex))) return true
                if (propPinch) {
                    propPinch = false
                    if (event.getPointerId(event.actionIndex) != activePointerId) return true
                }
                if (pinchOpen) {
                    pinchOpen = false
                    pinchStartDistance = 0f
                    pinchStartZoom = controller.view.zoom
                    return true
                }
                val pointerId = event.getPointerId(event.actionIndex)
                if (pointerId == activePointerId) {
                    val up = canvasPoint(event.getX(event.actionIndex), event.getY(event.actionIndex))
                    if (sessionTool >= 0) {
                        if ((event.flags and MotionEvent.FLAG_CANCELED) != 0) endSession(cancel = true)
                        else endSession(event, event.actionIndex)
                    } else if (mirrorOpen) {
                        commitMirror(up.first, up.second)
                    } else if (strokeOpen) {
                        if ((event.flags and MotionEvent.FLAG_CANCELED) != 0) {
                            controller.cancelStroke()
                        } else {
                            controller.endStroke()
                        }
                    }
                    resetGestureState()
                    controller.render()
                }
                return true
            }
            MotionEvent.ACTION_UP -> {
                if (multiMax >= 2) {
                    val tap = multiMoved <= TAP_SLOP_PX && event.eventTime - multiStartMs <= TAP_TIMEOUT_MS
                    if (tap) { if (multiMax == 2) controller.undo() else controller.redo() }
                    resetGestureState()
                    controller.render()
                    controller.onUiChanged?.invoke()
                    return true
                }
                val pointerIndex = event.findPointerIndex(activePointerId)
                if (sessionTool >= 0) {
                    if (pointerIndex >= 0) endSession(event, pointerIndex) else endSession(cancel = true)
                } else if (controller.tools.activeTool == com.smitnk.projectgrease.editor.GreaseTool.ERASE && !strokeOpen) {
                    controller.endErase()
                } else if (mirrorOpen && pointerIndex >= 0) {
                    val up = canvasPoint(event.getX(pointerIndex), event.getY(pointerIndex))
                    commitMirror(up.first, up.second)
                } else if (strokeOpen) {
                    if (pointerIndex >= 0) {
                        controller.endStroke()
                    } else {
                        controller.cancelStroke()
                    }
                }
                resetGestureState()
                controller.render()
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                endSession(cancel = sessionTool == com.smitnk.projectgrease.editor.ToolSession.TOOL_DRAW)
                if (controller.tools.activeTool == com.smitnk.projectgrease.editor.GreaseTool.ERASE) controller.endErase()
                if (strokeOpen) controller.cancelStroke()
                resetGestureState()
                controller.render()
                return true
            }
        }
        return true
    }

    private fun commitMirror(x: Float, y: Float) {
        if (!mirrorOpen) return
        val dx = x - mirrorStartX
        val dy = y - mirrorStartY
        if (hypot(dx, dy) < 12f) return
        if (abs(dx) >= abs(dy)) {
            // Horizontal drag -> reflect across the stroke's vertical centerline.
            controller.mirrorSelectedStrokeAround(
                true, false, mirrorCenterX, mirrorCenterY
            )
        } else {
            // Vertical drag -> reflect across the stroke's horizontal centerline.
            controller.mirrorSelectedStrokeAround(
                false, true, mirrorCenterX, mirrorCenterY
            )
        }
    }

    private fun eraseAlongPath(x: Float, y: Float): Boolean {
        val radius = controller.eraserRadius(pixelsPerUnit())
        val sx = if (eraseLastX.isNaN()) x else eraseLastX
        val sy = if (eraseLastY.isNaN()) y else eraseLastY
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
        eraseLastX = x
        eraseLastY = y
        return changed
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

    private fun canvasPoint(rawX:Float,rawY:Float):Pair<Float,Float> =
        com.smitnk.projectgrease.editor.CanvasMapping.toCanvas(
            rawX, rawY, width.toFloat(), height.toFloat(),
            controller.document.canvasWidth, controller.document.canvasHeight,
            controller.view.zoom, controller.view.panX, controller.view.panY, rotation = controller.view.rotation)

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
        val tool = sessionTool
        if (tool < 0) return
        val samples = if (tool == com.smitnk.projectgrease.editor.ToolSession.TOOL_DRAW) FloatArray(0)
            else batch(event, pointerIndex, includeHistory = true, tool = tool)
        controller.toolSamples(tool, samples, samples.size / 4, com.smitnk.projectgrease.editor.ToolSession.PHASE_END)
        sessionTool = -1
    }

    private fun endSession(cancel: Boolean) {
        val tool = sessionTool
        if (tool < 0) return
        controller.toolSamples(tool, FloatArray(0), 0,
            if (cancel) com.smitnk.projectgrease.editor.ToolSession.PHASE_CANCEL else com.smitnk.projectgrease.editor.ToolSession.PHASE_END)
        sessionTool = -1
    }

    private fun resetGestureState() {
        propPinch = false
        multiMax = 0
        multiMoved = 0f
        controller.history.endBatch()
        moveOpen = false
        rotateOpen = false
        scaleOpen = false
        mirrorOpen = false
        panOpen = false
        sessionTool = -1
        eraseLastX = Float.NaN
        eraseLastY = Float.NaN
        pinchOpen = false
        pinchStartDistance = 0f
        pinchStartZoom = controller.view.zoom
        scaleAccumulated = 1f
        lastScaleRadius = 0f
        activePointerId = MotionEvent.INVALID_POINTER_ID
        strokeOpen = false
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
