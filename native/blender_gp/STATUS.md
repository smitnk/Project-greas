# Native backend status

## Verified

- Blender v3.6.23 source is pinned by exact commit.
- Real Legacy GP data structures are used: bGPdata, bGPDlayer, bGPDframe, bGPDstroke, bGPDspoint.
- Stroke point fields include X/Y/Z, pressure, strength and time.
- Native stroke editing primitives exist behind the Project Grease adapter.
- Legacy GP batch-cache dirty/free callbacks are installed.
- The focused Android GPU/presentation boundary is compiled.
- Android owns EGL/GLES and the native backend attaches to the externally-owned context.
- Android shell CI run 42 on 2026-09-27 passed APK build and APK verification.
- Native Blender GP backend CI run 245 on 2026-09-27 passed the native backend test workflow.

## Current application status

- Freehand drawing is end-to-end through real Legacy GP stroke allocation.
- Pressure capture is available through Android MotionEvent input.
- Transform operations now follow the Blender 3.6.23 Legacy GP transform-conversion rule of transforming selected points, rather than blindly transforming every point in a stroke.
- Move/Rotate/Scale/Mirror are still Project Grease gesture adapters around the Legacy GP point data; the full Blender transform operator/context stack is intentionally not imported.
- UI/controller architecture has now been separated from the native bridge.
- FeatureRegistry is the single capability source for UI availability.
- Timeline, layers, materials, selection and advanced tools have UI/controller boundaries but are not all connected end-to-end.
- Lasso selection and eraser remain IN_PROGRESS because their current Android hit-testing logic is not yet a direct adaptation of the Blender 3.6.23 Legacy GP editor implementation.
- Persistence, export and full playback are not implemented.

## Important limitation

A successful APK build proves compilation/package correctness; it does not prove that every editor feature works on a physical device. Runtime drawing and advanced editing still require device-level validation.

## Architecture constraint

This project remains a focused 2D application using Blender Legacy Grease Pencil as its native foundation. It is not a full Blender Android port.
