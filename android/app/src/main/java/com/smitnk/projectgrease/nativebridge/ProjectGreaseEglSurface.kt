package com.smitnk.projectgrease.nativebridge

import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
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
    modifier: androidx.compose.ui.Modifier = androidx.compose.ui.Modifier
) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            SurfaceView(context).apply {
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
