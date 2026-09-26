package com.smitnk.projectgrease.nativebridge

import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.runtime.Composable
import androidx.compose.ui.viewinterop.AndroidView

/**
 * Android surface bridge for the Blender-compatible rendering path.
 *
 * This layer deliberately stops at EGL/GLES transport. Blender GP data is
 * connected after this surface/context proof succeeds.
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
