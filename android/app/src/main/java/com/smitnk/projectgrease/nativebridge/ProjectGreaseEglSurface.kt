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
                    controller.attachRenderer(handle)
                    if (handle != 0L && GPNative.nativeAttachSurface(handle, holder.surface)) {
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

private class ProjectGreaseDrawingSurfaceView(
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
    private var lastPanX = 0f
    private var lastPanY = 0f

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
                when (controller.tools.activeTool) {
                    com.smitnk.projectgrease.editor.GreaseTool.SELECT -> {
                        controller.hitTestAndSelectStroke(start.first, start.second)
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.ERASE -> {
                        controller.eraseAt(start.first, start.second)
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.FILL -> {
                        controller.fillAt(event.x, event.y)
                        controller.render()
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.MOVE -> {
                        moveOpen = controller.hitTestAndSelectStroke(event.x, event.y)
                        if (moveOpen) {
                            lastMoveX = event.x
                            lastMoveY = event.y
                            controller.render()
                        }
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.ROTATE -> {
                        rotateOpen = controller.hitTestAndSelectStroke(event.x, event.y)
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
                        scaleOpen = controller.hitTestAndSelectStroke(event.x, event.y)
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
                        mirrorOpen = controller.hitTestAndSelectStroke(event.x, event.y)
                        if (mirrorOpen) {
                            val center = controller.selectedStrokeCenter()
                            if (center == null || center.size < 2) {
                                mirrorOpen = false
        panOpen = false
                            } else {
                                mirrorCenterX = center[0]
                                mirrorCenterY = center[1]
                                mirrorStartX = start.first
                                mirrorStartY = start.second
                                controller.render()
                            }
                        }
                    }
                    GreaseTool.PAN -> {
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
                // Keep the original drawing/transform pointer. A second finger
                // never steals the active gesture.
                return true
            }
            MotionEvent.ACTION_MOVE -> {
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
                            addPoint(event, pointerIndex)
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
                val pointerId = event.getPointerId(event.actionIndex)
                if (pointerId == activePointerId) {
                    val up = canvasPoint(event.getX(event.actionIndex), event.getY(event.actionIndex))
                    if (mirrorOpen) {
                        commitMirror(up.first, up.second)
                    } else if (strokeOpen) {
                        if ((event.flags and MotionEvent.FLAG_CANCELED) != 0) {
                            controller.cancelStroke()
                        } else {
                            addPoint(event, event.actionIndex)
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
                if (mirrorOpen && pointerIndex >= 0) {
                    val up = canvasPoint(event.getX(pointerIndex), event.getY(pointerIndex))
                    commitMirror(up.first, up.second)
                } else if (strokeOpen) {
                    if (pointerIndex >= 0) {
                        addPoint(event, pointerIndex)
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

    private fun canvasPoint(rawX:Float,rawY:Float):Pair<Float,Float>{
        val cw=controller.document.canvasWidth.coerceAtLeast(1)
        val ch=controller.document.canvasHeight.coerceAtLeast(1)
        val fit=min(width.toFloat()/cw.toFloat(),height.toFloat()/ch.toFloat())*0.92f*controller.view.zoom
        val ox=(width.toFloat()-cw*fit)*0.5f+controller.view.panX
        val oy=(height.toFloat()-ch*fit)*0.5f+controller.view.panY
        return ((rawX-ox)/fit).coerceIn(0f,cw.toFloat()) to ((rawY-oy)/fit).coerceIn(0f,ch.toFloat())
    }

    private fun resetGestureState() {
        moveOpen = false
        rotateOpen = false
        scaleOpen = false
        mirrorOpen = false
        scaleAccumulated = 1f
        lastScaleRadius = 0f
        activePointerId = MotionEvent.INVALID_POINTER_ID
        strokeOpen = false
    }

    private fun addPoint(event: MotionEvent, pointerIndex: Int) {
        val pressure = event.getPressure(pointerIndex).coerceAtLeast(0.01f)
        val p = canvasPoint(event.getX(pointerIndex), event.getY(pointerIndex))
        controller.addStrokePoint(
            p.first,
            p.second,
            pressure,
            event.eventTime.toFloat() / 1000f
        )
    }
}
