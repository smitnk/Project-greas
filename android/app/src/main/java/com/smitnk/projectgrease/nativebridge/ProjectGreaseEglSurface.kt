package com.smitnk.projectgrease.nativebridge

import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.MotionEvent
import androidx.compose.runtime.Composable
import androidx.compose.ui.viewinterop.AndroidView

/**
 * Android surface bridge for the Blender-compatible rendering path.
 *
 * Native code creates EGL/GLES, makes that context current, then attaches
 * the Blender GP backend to the externally-owned current context when the
 * Android-compatible backend is linked into the JNI library.
 */
@Composable
fun ProjectGreaseEglViewport(
    modifier: androidx.compose.ui.Modifier = androidx.compose.ui.Modifier,
    drawingEnabled: Boolean = true,
    strokeWidth: Float = 8f,
    materialIndex: Int = 0
) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            ProjectGreaseDrawingSurfaceView(context).apply {
                this.drawingEnabled = drawingEnabled
                this.strokeWidth = strokeWidth
                this.materialIndex = materialIndex
                holder.addCallback(object : SurfaceHolder.Callback {
                    private var rendererHandle = 0L

                    override fun surfaceCreated(holder: SurfaceHolder) {
                        rendererHandle = GPNative.nativeCreateEglRenderer()
                        if (rendererHandle != 0L &&
                            GPNative.nativeAttachSurface(rendererHandle, holder.surface)
                        ) {
                            Log.i(
                                "ProjectGrease",
                                "EGL/GLES " + GPNative.nativeGlesVersion(rendererHandle) +
                                    " surface connected; Blender GP backend=" +
                                    GPNative.nativeBlenderGpConnected(rendererHandle)
                            )
                            GPNative.nativeRenderEgl(rendererHandle)
                        }
                    }

                    override fun surfaceChanged(
                        holder: SurfaceHolder,
                        format: Int,
                        width: Int,
                        height: Int
                    ) {
                        if (rendererHandle != 0L) {
                            GPNative.nativeAttachSurface(rendererHandle, holder.surface)
                            GPNative.nativeRenderEgl(rendererHandle)
                        }
                    }

                    override fun surfaceDestroyed(holder: SurfaceHolder) {
                        if (rendererHandle != 0L) {
                            GPNative.nativeDetachSurface(rendererHandle)
                            GPNative.nativeDestroyEglRenderer(rendererHandle)
                            rendererHandle = 0L
                        }
                    }
                })
            }
        }
    )
}

private class ProjectGreaseDrawingSurfaceView(context: android.content.Context) : SurfaceView(context) {
    var drawingEnabled: Boolean = true
    var strokeWidth: Float = 8f
    var materialIndex: Int = 0
    private var rendererHandle: Long = 0L
    private var strokeOpen = false

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!drawingEnabled || rendererHandle == 0L) return true
        val action = event.actionMasked
        when (action) {
            MotionEvent.ACTION_DOWN -> {
                strokeOpen = GPNative.nativeBeginStrokeEglRenderer(rendererHandle, materialIndex, strokeWidth)
                if (strokeOpen) addPoint(event, 0)
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (strokeOpen) {
                    for (i in 0 until event.pointerCount) addPoint(event, i)
                    GPNative.nativeRenderEgl(rendererHandle)
                }
                return true
            }
            MotionEvent.ACTION_UP -> {
                if (strokeOpen) {
                    addPoint(event, event.actionIndex.coerceIn(0, event.pointerCount - 1))
                    GPNative.nativeEndStrokeEglRenderer(rendererHandle)
                    strokeOpen = false
                    GPNative.nativeRenderEgl(rendererHandle)
                }
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                if (strokeOpen) {
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
