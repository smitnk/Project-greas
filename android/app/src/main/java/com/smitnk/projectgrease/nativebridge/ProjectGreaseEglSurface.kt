package com.smitnk.projectgrease.nativebridge

import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView

@Composable
fun ProjectGreaseEglViewport(
    modifier: Modifier = Modifier,
    drawingEnabled: Boolean = true,
    strokeWidth: Float = 8f,
    materialIndex: Int = 0
) {
    AndroidView(
        modifier = modifier,
        update = { view ->
            view.drawingEnabled = drawingEnabled
            view.strokeWidth = strokeWidth
            view.materialIndex = materialIndex
        },
        factory = { context ->
            val view = ProjectGreaseDrawingSurfaceView(context)
            view.holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(holder: SurfaceHolder) {
                    val handle = GPNative.nativeCreateEglRenderer()
                    view.setRendererHandle(handle)
                    if (handle != 0L && GPNative.nativeAttachSurface(handle, holder.surface)) {
                        Log.i(
                            "ProjectGrease",
                            "EGL/GLES " + GPNative.nativeGlesVersion(handle) +
                                " surface connected; Blender GP backend=" +
                                GPNative.nativeBlenderGpConnected(handle)
                        )
                        GPNative.nativeRenderEgl(handle)
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
                        GPNative.nativeRenderEgl(handle)
                    }
                }

                override fun surfaceDestroyed(holder: SurfaceHolder) {
                    val handle = view.getRendererHandle()
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
    context: android.content.Context
) : SurfaceView(context) {
    var drawingEnabled: Boolean = true
    var strokeWidth: Float = 8f
    var materialIndex: Int = 0

    private var rendererHandle: Long = 0L
    private var strokeOpen = false

    fun setRendererHandle(handle: Long) {
        rendererHandle = handle
    }

    fun getRendererHandle(): Long = rendererHandle

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!drawingEnabled || rendererHandle == 0L) return true

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                strokeOpen = GPNative.nativeBeginStrokeEglRenderer(
                    rendererHandle, materialIndex, strokeWidth
                )
                if (strokeOpen) {
                    addPoint(event, 0)
                    GPNative.nativeRenderEgl(rendererHandle)
                }
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                if (strokeOpen) {
                    // For the first drawing path, only the active pointer is
                    // forwarded. Multi-touch navigation can be added separately.
                    addPoint(event, 0)
                    GPNative.nativeRenderEgl(rendererHandle)
                }
                return true
            }

            MotionEvent.ACTION_UP -> {
                if (strokeOpen) {
                    addPoint(event, 0)
                    GPNative.nativeEndStrokeEglRenderer(rendererHandle)
                    strokeOpen = false
                    GPNative.nativeRenderEgl(rendererHandle)
                }
                return true
            }

            MotionEvent.ACTION_CANCEL -> {
                if (strokeOpen) {
                    // The current native bridge has no transactional cancel
                    // primitive yet; end the stroke so no input remains open.
                    GPNative.nativeEndStrokeEglRenderer(rendererHandle)
                    strokeOpen = false
                }
                return true
            }
        }

        return true
    }

    private fun addPoint(event: MotionEvent, pointerIndex: Int) {
        val pressure = event.getPressure(pointerIndex).coerceAtLeast(0.01f)
        GPNative.nativeAddPointEglRenderer(
            rendererHandle,
            event.getX(pointerIndex),
            event.getY(pointerIndex),
            0f,
            pressure,
            1f,
            event.eventTime.toFloat() / 1000f
        )
    }
}
