package com.smitnk.projectgrease.nativebridge

import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
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

    fun setRendererHandle(handle: Long) {
        rendererHandle = handle
    }

    fun getRendererHandle(): Long = rendererHandle

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (rendererHandle == 0L) return true

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                when (controller.tools.activeTool) {
                    com.smitnk.projectgrease.editor.GreaseTool.SELECT -> {
                        controller.hitTestAndSelectStroke(event.x, event.y)
                    }
                    com.smitnk.projectgrease.editor.GreaseTool.ERASE -> {
                        controller.eraseAt(event.x, event.y)
                    }
                    else -> {
                        strokeOpen = controller.beginStroke()
                        if (strokeOpen) {
                            addPoint(event, 0)
                            controller.render()
                        }
                    }
                }
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (strokeOpen) {
                    addPoint(event, 0)
                    controller.render()
                }
                return true
            }
            MotionEvent.ACTION_UP -> {
                if (strokeOpen) {
                    addPoint(event, 0)
                    controller.endStroke()
                    strokeOpen = false
                    controller.render()
                }
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                if (strokeOpen) {
                    controller.cancelStroke()
                    strokeOpen = false
                    controller.render()
                }
                return true
            }
        }
        return true
    }

    private fun addPoint(event: MotionEvent, pointerIndex: Int) {
        val pressure = event.getPressure(pointerIndex).coerceAtLeast(0.01f)
        controller.addStrokePoint(
            event.getX(pointerIndex),
            event.getY(pointerIndex),
            pressure,
            event.eventTime.toFloat() / 1000f
        )
    }
}
