"""Real Blender 3.6.23 side of the .blend open / save proof (native/blender_gp/tests/test_blend_io.cc).

    blender -b --factory-startup --python-exit-code 1 --python blender_blend_io_reference.py -- make OUT.blend OUT.json
    blender -b --factory-startup --python-exit-code 1 --python blender_blend_io_reference.py -- dump IN.blend OUT.json

make: builds a legacy Grease Pencil object (2 layers, 3 frames, materials, vertex colors, a mask, Thickness /
Tint modifiers) and an armature (bone chain, Automatic Weights bind = Armature modifier + vertex groups, pose,
keyed action), saves it with Blender's own writer, then dumps it. dump: opens a .blend (the one the app wrote)
with Blender's own reader and writes the same dump. Raw Blender values (object / armature space).
"""
import json
import sys

import bpy

PPU = 1000.0


def P(x, y):
    return (x / PPU, -y / PPU, 0.0)


BONES = [("Root", (500, 800), (500, 600), 0.0, None, False),
         ("Spine", (500, 600), (500, 400), 0.0, "Root", True),
         ("Arm", (500, 450), (700, 450), 0.3, "Spine", False),
         ("Hand", (700, 450), (850, 380), 0.0, "Arm", True)]
INK = {1: [[(500 + 6 * i, 780 - 40 * i) for i in range(10)], [(480 + 20 * i, 440 + 3 * (i % 3)) for i in range(12)]],
       5: [[(860 - 5 * i, 380 + 12 * i) for i in range(8)]]}
COLOR = {1: [[(400 + 30 * i, 300 + (i % 2) * 25) for i in range(6)]]}


def add_stroke(frame, pts, k):
    s = frame.strokes.new()
    s.display_mode = "3DSPACE"
    s.line_width = 10 + 7 * k
    s.material_index = k % 2
    s.use_cyclic = k == 1
    s.hardness = 0.75 if k else 1.0
    s.start_cap_mode = "FLAT" if k == 2 else "ROUND"
    s.vertex_color_fill = (0.1, 0.2, 0.3, 0.4 if k == 1 else 0.0)
    s.points.add(len(pts))
    for i, (p, (x, y)) in enumerate(zip(s.points, pts)):
        p.co = P(x, y)
        p.pressure = 0.5 + 0.05 * i
        p.strength = 1.0 - 0.03 * i
        p.vertex_color = (0.9, 0.5, 0.1, 0.25 * k)


def make(blend_path):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    scene = bpy.context.scene
    scene.render.fps = 12
    scene.frame_start, scene.frame_end = 1, 20
    scene.frame_set(1)
    view = bpy.context.view_layer
    gpd = bpy.data.grease_pencils.new("Drawing")
    gp = bpy.data.objects.new("Drawing", gpd)
    scene.collection.objects.link(gp)
    for name, stroke, fill, show_fill in (("Ink", (0.05, 0.05, 0.08, 1.0), (0.6, 0.6, 0.6, 1.0), False),
                                          ("Paint", (0.8, 0.1, 0.2, 0.9), (0.2, 0.5, 0.9, 0.7), True)):
        m = bpy.data.materials.new(name)
        bpy.data.materials.create_gpencil_data(m)
        m.grease_pencil.color = stroke
        m.grease_pencil.fill_color = fill
        m.grease_pencil.show_fill = show_fill
        gpd.materials.append(m)
    bpy.data.materials["Paint"].grease_pencil.pass_index = 3
    bpy.data.materials["Paint"].grease_pencil.mode = "DOTS"
    ink = gpd.layers.new("Ink")
    color = gpd.layers.new("Color")
    color.opacity = 0.8
    color.blend_mode = "MULTIPLY"
    color.tint_color, color.tint_factor = (0.2, 0.4, 0.6), 0.3
    color.line_change = 2
    color.pass_index = 4
    k = 0
    for layer, frames in ((ink, INK), (color, COLOR)):
        for n, strokes in frames.items():
            f = layer.frames.new(n)
            if n == 5:
                f.keyframe_type = "BREAKDOWN"
            for pts in strokes:
                add_stroke(f, pts, k)
                k += 1
    color.use_mask_layer = True
    color.mask_layers.add(ink)
    t = gp.grease_pencil_modifiers.new("Thickness", "GP_THICK")
    t.layer = "Ink"
    t.thickness_factor = 1.5
    t = gp.grease_pencil_modifiers.new("Tint", "GP_TINT")
    t.layer = "Color"
    t.factor, t.color = 0.4, (0.3, 0.9, 0.2)
    adata = bpy.data.armatures.new("Rig")
    arm = bpy.data.objects.new("Rig", adata)
    scene.collection.objects.link(arm)
    view.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, h, tl, roll, parent, conn in BONES:
        eb = adata.edit_bones.new(name)
        eb.head, eb.tail, eb.roll = P(*h), P(*tl), roll
        if parent:
            eb.parent = adata.edit_bones[parent]
            eb.use_connect = conn
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.context.evaluated_depsgraph_get().update()
    for o in view.objects:
        o.select_set(False)
    gp.select_set(True)
    arm.select_set(True)
    win = bpy.context.window_manager.windows[0]
    area = next(a for a in win.screen.areas if a.type == "VIEW_3D")
    region = next(r for r in area.regions if r.type == "WINDOW")
    with bpy.context.temp_override(window=win, area=area, region=region, active_object=arm,
                                   selected_objects=[gp, arm], selected_editable_objects=[gp, arm]):
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    if not [m for m in gp.grease_pencil_modifiers if m.type == "GP_ARMATURE"]:
        raise RuntimeError("no Armature modifier after bind")
    view.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    pb = arm.pose.bones
    for fr, ang in ((1, 0.0), (10, 0.4), (20, -0.2)):
        pb["Spine"].rotation_quaternion = (1.0, 0.0, 0.0, ang)
        pb["Spine"].rotation_quaternion.normalize()
        pb["Arm"].location = (0.0, 0.01 * fr, 0.0)
        pb["Spine"].keyframe_insert("rotation_quaternion", frame=fr)
        pb["Arm"].keyframe_insert("location", frame=fr)
    for fc in arm.animation_data.action.fcurves:
        if "Arm" in fc.data_path:
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
    scene.frame_set(1)
    pb["Hand"].scale = (1.0, 1.2, 1.0)  # an unkeyed pose value
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.wm.save_as_mainfile(filepath=blend_path, compress=False)
    bpy.ops.wm.save_as_mainfile(filepath=blend_path.replace(".blend", "_zstd.blend"), compress=True, copy=True)


def r(v):
    return [float(x) for x in v]


def weight(s, group, point):
    try:
        return s.points.weight_get(vertex_group_index=group, point_index=point)
    except RuntimeError:  # "No groups for this stroke": the stroke has no weights at all
        return -1.0


def dump():
    scene = bpy.context.scene
    gp = next(o for o in bpy.data.objects if o.type == "GPENCIL")
    gpd = gp.data
    arm = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
    out = {"blender": bpy.app.version_string, "fps": scene.render.fps, "frame_end": scene.frame_end,
           "materials": [], "vertex_groups": [vg.name for vg in gp.vertex_groups], "layers": [], "modifiers": []}
    for m in gpd.materials:
        g = m.grease_pencil
        out["materials"].append({"name": m.name, "color": r(g.color), "fill_color": r(g.fill_color),
                                 "show_stroke": g.show_stroke, "show_fill": g.show_fill, "hide": g.hide,
                                 "lock": g.lock, "mode": g.mode, "pass_index": g.pass_index})
    for layer in gpd.layers:
        L = {"info": layer.info, "opacity": layer.opacity, "hide": layer.hide, "lock": layer.lock,
             "blend_mode": layer.blend_mode, "tint": r(layer.tint_color) + [layer.tint_factor],
             "line_change": layer.line_change, "pass_index": layer.pass_index,
             "use_mask": layer.use_mask_layer, "masks": [m.name for m in layer.mask_layers], "frames": []}
        for f in layer.frames:
            F = {"number": f.frame_number, "key": f.keyframe_type, "strokes": []}
            for s in f.strokes:
                pts = []
                for i, p in enumerate(s.points):
                    w = {vg.name: weight(s, vg.index, i) for vg in gp.vertex_groups}
                    pts.append({"co": r(p.co), "pressure": p.pressure, "strength": p.strength,
                                "vertex_color": r(p.vertex_color), "weights": {k: v for k, v in w.items() if v >= 0}})
                F["strokes"].append({"line_width": s.line_width, "material_index": s.material_index,
                                     "cyclic": s.use_cyclic, "hardness": s.hardness,
                                     "caps": [s.start_cap_mode, s.end_cap_mode], "fill": r(s.vertex_color_fill),
                                     "points": pts})
            L["frames"].append(F)
        out["layers"].append(L)
    for m in gp.grease_pencil_modifiers:
        M = {"type": m.type, "layer": getattr(m, "layer", ""), "show": m.show_viewport}
        if m.type == "GP_THICK":
            M.update(thickness=m.thickness, factor=m.thickness_factor, normalize=m.use_normalized_thickness)
        elif m.type == "GP_TINT":
            M.update(factor=m.factor, color=r(m.color), mode=m.vertex_mode)
        elif m.type == "GP_ARMATURE":
            M.update(object=m.object.type if m.object else "",  # names: the app keeps none for objects
                     vgroups=m.use_vertex_groups,
                     envelope=m.use_bone_envelopes, volume=m.use_deform_preserve_volume)
        out["modifiers"].append(M)
    if arm:
        a = {"bones": [], "pose": [], "fcurves": []}
        for b in arm.data.bones:
            a["bones"].append({"name": b.name, "parent": b.parent.name if b.parent else "",
                               "connect": b.use_connect, "head": r(b.head_local), "tail": r(b.tail_local),
                               "matrix": [r(row) for row in b.matrix_local]})
        for pbone in arm.pose.bones:
            a["pose"].append({"name": pbone.name, "loc": r(pbone.location), "quat": r(pbone.rotation_quaternion),
                              "scale": r(pbone.scale)})
        act = arm.animation_data.action if arm.animation_data else None
        for fc in (act.fcurves if act else []):
            a["fcurves"].append({"path": fc.data_path, "index": fc.array_index,
                                 "keys": [r(k.co) + r(k.handle_left) + r(k.handle_right) + [k.interpolation]
                                          for k in fc.keyframe_points]})
        out["armature"] = a
    # evaluated at frame 10 (action, Armature / Thickness / Tint modifiers): the file works in Blender
    scene.frame_set(10)
    dg = bpy.context.evaluated_depsgraph_get()
    ev = gp.evaluated_get(dg).data
    out["evaluated"] = [[r(p.co) + [p.pressure, p.strength] for s in layer.active_frame.strokes for p in s.points]
                        for layer in ev.layers if layer.active_frame]
    out["evaluated_thickness"] = [[s.line_width for s in layer.active_frame.strokes]
                                  for layer in ev.layers if layer.active_frame]
    return out


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    if args[0] == "make":
        make(args[1])
        bpy.ops.wm.open_mainfile(filepath=args[1])  # dump what was written, as read back by Blender
    else:
        bpy.ops.wm.open_mainfile(filepath=args[1])
    with open(args[2], "w") as fh:
        json.dump(dump(), fh, indent=1)
    print("blend io reference written", args[2])


main()
