import bpy

# Project Grease startup: guarantee a real Grease Pencil object exists and
# enter Blender's native Grease Pencil Draw Mode. This does not create a
# replacement renderer or canvas.
gp = next((obj for obj in bpy.data.objects if obj.type == 'GREASEPENCIL'), None)

if gp is None:
    bpy.ops.object.grease_pencil_add(type='STROKE', location=(0.0, 0.0, 0.0))
    gp = bpy.context.object

if gp is not None:
    bpy.context.view_layer.objects.active = gp
    gp.select_set(True)
    try:
        bpy.ops.object.mode_set(mode='PAINT_GREASE_PENCIL')
    except RuntimeError:
        try:
            bpy.ops.grease_pencil.paintmode_toggle()
        except RuntimeError:
            pass