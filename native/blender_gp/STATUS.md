# Native backend status

## Done

- Created a dedicated native backend branch.
- Pinned upstream Blender v3.6.23 source by exact commit.
- Added an import script for the upstream Blender source.
- Added the GP source/dependency manifest from the investigation.
- Added a Project Grease adapter boundary carrying X/Y/Z, pressure, strength and time.
- Explicitly excluded the previously investigated Blender Android application port from the implementation.

## Not claimed as complete

The current adapter is scaffolding only. It does not yet render a Blender stroke.

## Next build gate

1. Import the pinned upstream Blender tree.
2. Build the smallest desktop dependency set.
3. Replace the adapter's placeholder data state with:
   - BKE_gpencil_data_addnew
   - BKE_gpencil_layer_addnew
   - BKE_gpencil_frame_addnew
   - BKE_gpencil_stroke_add
4. Connect the real GP draw cache.
5. Initialize the real Blender GPU/DRW context.
6. Render one stroke.
7. Only after desktop proof, add the Android EGL/GLES surface adapter.

This order prevents the Android port from hiding native dependency/linking problems.
