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
import androidx.compose.ui.viewinterop.AndroidView
import com.smitnk.projectgrease.editor.EditorController

@Composable
fun ProjectGreaseEglViewport(
    modifier: Modifier = Modifier,
    controller: EditorController
) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            val view = ProjectGreaseDrawingSurfaceView(context, controller)
            view.holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(holder: SurfaceHolder) {
                    val handle = GPNative.nativeCreateEglRenderer()
                    view.setRendererHandle(handle)
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
                    val handle = view.getRendererHandle()
                    if (handle != 0L) {
                        GPNative.nativeAttachSurface(handle, holder.surface)
                        controller.attachRenderer(handle)
                        controller.render()
                    }
                }

                override fun surfaceDestroyed(holder: SurfaceHolder) {
                    val handle = view.getRendererHandle()
                    controller.detachRenderer()
                    if (handle != 0L) {
                        GPNative.nativeDetachSurface(handle)
                        GPNative.nativeDestroyEglRenderer(handle)
                        view.setRendererHandle(0L)
                    }
                }
            })
            view
        }
    )
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
    private var pinchStartDistance = 0f
    private var pinchStartZoom = 1f

    fun setRendererHandle(handle: Long) {
        rendererHandle = handle
    }

    fun getRendererHandle(): Long = rendererHandle

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (rendererHandle == 0L) return true

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                val start = canvasPoint(event.x, event.y)
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
                when (controller.tools.activeTool) {
                    com.smitnk.projectgrease.editor.GreaseTool.SELECT -> {
                        controller.hitTestAndSelectStroke(start.first, start.second)
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.ERASE -> {
                        controller.eraseAt(start.first, start.second)
                        eraseLastX = start.first
                        eraseLastY = start.second
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.FILL -> {
                        controller.fillAt(event.x, event.y)
                        controller.render()
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.EYEDROPPER -> {
                        controller.pickColorAt(event.x.toInt(), event.y.toInt())
                        controller.render()
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.MOVE -> {
                        moveOpen = controller.hitTestAndSelectStroke(start.first, start.second)
                        if (moveOpen) {
                            lastMoveX = start.first
                            lastMoveY = start.second
                            controller.render()
                        }
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.ROTATE -> {
                        rotateOpen = controller.hitTestAndSelectStroke(start.first, start.second)
                        if (rotateOpen) {
                            val center = controller.selectedStrokeCenter()
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
                        scaleOpen = controller.hitTestAndSelectStroke(start.first, start.second)
                        if (scaleOpen) {
                            val center = controller.selectedStrokeCenter()
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
                        mirrorOpen = controller.hitTestAndSelectStroke(start.first, start.second)
                        if (mirrorOpen) {
                            val center = controller.selectedStrokeCenter()
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
                if (event.pointerCount >= 2) {
                    endSession(cancel = sessionTool == com.smitnk.projectgrease.editor.ToolSession.TOOL_DRAW)
                    if (strokeOpen) {
                        controller.cancelStroke()
                        strokeOpen = false
                    }
                    panOpen = false
                    pinchOpen = true
                    pinchStartDistance = pointerDistance(event).coerceAtLeast(1f)
                    pinchStartZoom = controller.view.zoom
                }
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (pinchOpen && event.pointerCount >= 2) {
                    val distance = pointerDistance(event)
                    if (pinchStartDistance > 0f && distance > 0f) {
                        controller.view.setZoom(
                            pinchStartZoom * (distance / pinchStartDistance)
                        )
                        controller.render()
                    }
                    return true
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
                        controller.tools.activeTool == com.smitnk.projectgrease.editor.GreaseTool.ERASE -> {
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
                            controller.render()
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
                val pointerIndex = event.findPointerIndex(activePointerId)
                if (sessionTool >= 0) {
                    if (pointerIndex >= 0) endSession(event, pointerIndex) else endSession(cancel = true)
                } else if (controller.tools.activeTool == com.smitnk.projectgrease.editor.GreaseTool.ERASE) {
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
        val radius = controller.eraserRadius().coerceAtLeast(1f)
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
            controller.view.zoom, controller.view.panX, controller.view.panY)

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
