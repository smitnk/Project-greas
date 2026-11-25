"""Blender 3.6.23's own rig results, for native/blender_gp/tests/test_rig_blender_parity.cc.

    blender -b --factory-startup --python-exit-code 1 --python blender_rig_reference.py -- OUT.json

Builds a legacy Grease Pencil object and an armature, binds with Automatic Weights, keys the pose,
adds constraints and drivers, then at frames 1, 5 and 10 writes the pose bone matrices, head / tail,
the evaluated GP strokes (after the Armature and Time Offset modifiers) and the vertex group weights.
Everything is raw Blender units, armature space; the C++ test converts canvas px with
(px, -py, 0) / 1000 (PG_ARM_PIXELS_PER_UNIT, project_grease_armature_bke.c).
The rig constants below are duplicated in the C++ test; "rig" in the JSON lets it verify they agree.
"""
import json
import math
import sys

import bpy
from mathutils import Quaternion, Vector

PPU = 1000.0
FRAMES = (1, 5, 10)


def P(x, y):
    return (x / PPU, -y / PPU, 0.0)


# name, head px, tail px, roll, parent, connected
BONES = [
    ("Root", (500, 800), (500, 600), 0.0, None, False),
    ("Spine", (500, 600), (500, 400), 0.0, "Root", True),
    ("Arm.U", (500, 450), (700, 450), 0.3, "Spine", False),
    ("Arm.L", (700, 450), (850, 380), 0.0, "Arm.U", True),
    ("Head", (500, 400), (500, 250), 0.0, "Spine", True),
    ("IK.T", (900, 300), (900, 250), 0.0, None, False),
    ("Pole", (700, 300), (700, 250), 0.0, None, False),
    ("Src", (200, 500), (300, 500), 0.0, None, False),
    ("Follow", (200, 600), (300, 600), 0.0, None, False),
    ("RotCopy", (200, 700), (300, 700), 0.0, None, False),
    ("Aim", (300, 300), (400, 300), 0.0, None, False),
    ("Lim", (100, 400), (100, 300), 0.0, None, False),
    ("Kid", (200, 200), (250, 200), 0.0, None, False),
    ("Smile", (400, 100), (450, 100), 0.0, None, False),
    ("DrvBy", (500, 100), (550, 100), 0.0, None, False),
]


def quat_z(a):
    return (math.cos(a / 2), 0.0, 0.0, math.sin(a / 2))


def quat_axis(ax, a):
    n = math.sqrt(sum(c * c for c in ax))
    s = math.sin(a / 2) / n
    return (math.cos(a / 2), ax[0] * s, ax[1] * s, ax[2] * s)


# bone -> {frame: (loc, quat)}, ipo 'BEZIER' / 'LINEAR'
KEYS = {
    "Spine": ("BEZIER", {1: ((0, 0, 0), quat_z(0.0)), 10: ((0, 0, 0), quat_z(0.35))}),
    "Arm.U": ("LINEAR", {1: ((0, 0, 0), quat_z(-0.2)), 10: ((0.02, 0.03, 0), quat_z(0.5))}),
    "Head": ("BEZIER", {1: ((0, 0, 0), quat_z(0.0)), 10: ((0, 0, 0), quat_z(-0.3))}),
    "Src": ("BEZIER", {1: ((0.01, 0.02, 0), quat_z(0.2)),
                       10: ((0.08, -0.03, 0.01), quat_axis((0.3, 0.1, 0.9), 0.9))}),
    "IK.T": ("LINEAR", {1: ((0, 0, 0), quat_z(0.0)), 10: ((-0.05, 0.08, 0), quat_z(0.0))}),
    "Lim": ("BEZIER", {1: ((0, 0, 0), quat_z(0.0)), 10: ((0, 0, 0), quat_z(1.2))}),
}

# (px x, px y) polylines: 3 body strokes and 3 mouth drawings (frames 1, 2, 3), as add_stroke of the C++ test
BODY = [
    [(500 + 6 * i, 780 - 40 * i) for i in range(10)],
    [(480 + 20 * i, 440 + 3 * (i % 3)) for i in range(12)],
    [(860 - 5 * i, 380 + 12 * i) for i in range(8)],
]
MOUTH = {1: [(30 + 10 * i, 40 + 5 * i) for i in range(6)],
         2: [(30 + 10 * i, 60 - 4 * i) for i in range(6)],
         3: [(30 + 10 * i, 80 + 3 * (i % 2)) for i in range(6)]}
SMILE = {1: 0.0, 5: 0.5, 10: 1.0}


def add_stroke(frame, pts):
    s = frame.strokes.new()
    s.display_mode = "3DSPACE"
    s.line_width = 10
    s.points.add(len(pts))
    for p, (x, y) in zip(s.points, pts):
        p.co = P(x, y)
        p.pressure = 1.0
        p.strength = 1.0


def main():
    out_path = sys.argv[sys.argv.index("--") + 1]
    scene = bpy.context.scene
    scene.frame_set(1)
    view = bpy.context.view_layer

    # ---- Grease Pencil object
    gpd = bpy.data.grease_pencils.new("PG")
    gp = bpy.data.objects.new("Project Grease", gpd)
    scene.collection.objects.link(gp)
    mat = bpy.data.materials.new("M")
    bpy.data.materials.create_gpencil_data(mat)
    gpd.materials.append(mat)
    body = gpd.layers.new("Body")
    f = body.frames.new(1)
    for pts in BODY:
        add_stroke(f, pts)
    mouth = gpd.layers.new("Mouth")
    for n, pts in MOUTH.items():
        add_stroke(mouth.frames.new(n), pts)

    # ---- armature (edit bones; rest pose)
    adata = bpy.data.armatures.new("Rig")
    arm = bpy.data.objects.new("Rig", adata)
    scene.collection.objects.link(arm)
    view.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, h, t, roll, parent, conn in BONES:
        eb = adata.edit_bones.new(name)
        eb.head, eb.tail, eb.roll = P(*h), P(*t), roll
        if parent:
            eb.parent = adata.edit_bones[parent]
            eb.use_connect = conn
    bpy.ops.object.mode_set(mode="OBJECT")

    # Time Offset, Fixed Frame, on the Mouth layer, before the Armature modifier that Bind appends (as PG)
    tmod = gp.grease_pencil_modifiers.new("Time", "GP_TIME")
    tmod.layer = "Mouth"
    tmod.mode = "FIX"

    # ---- Parent with Automatic Weights: GP selected, armature active. The weights go on each layer's
    # active frame (gpencil_add_verts_to_dgroups), which Blender sets on the first evaluation.
    scene.frame_set(1)
    bpy.context.evaluated_depsgraph_get().update()
    if body.active_frame is None:
        raise RuntimeError("Body has no active frame before bind")
    for o in view.objects:
        o.select_set(False)
    gp.select_set(True)
    arm.select_set(True)
    view.objects.active = arm
    # ED_gpencil_stroke_can_use rejects 3D strokes when the context has no 3D View area, which a
    # headless run has not set: borrow the one of the factory startup screen.
    win = bpy.context.window_manager.windows[0]
    area = next(a for a in win.screen.areas if a.type == "VIEW_3D")
    region = next(r for r in area.regions if r.type == "WINDOW")
    with bpy.context.temp_override(window=win, area=area, region=region, active_object=arm,
                                   selected_objects=[gp, arm], selected_editable_objects=[gp, arm]):
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    arm_mods = [m for m in gp.grease_pencil_modifiers if m.type == "GP_ARMATURE"]
    if not arm_mods:
        raise RuntimeError("parent_set(ARMATURE_AUTO) added no Armature modifier")

    # weights straight after bind (rest pose)
    weights = {}
    for li, layer in enumerate(gpd.layers):
        for fr in layer.frames:
            for si, s in enumerate(fr.strokes):
                key = "%s/%d/%d" % (layer.info, fr.frame_number, si)
                weights[key] = {vg.name: [max(0.0, s.points.weight_get(vertex_group_index=vg.index, point_index=pi))
                                          for pi in range(len(s.points))] for vg in gp.vertex_groups}

    # ---- pose keys
    view.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    pb = arm.pose.bones
    for name, (ipo, frames) in KEYS.items():
        for fr, (loc, q) in frames.items():
            pb[name].location = loc
            pb[name].rotation_quaternion = q
            for prop in ("location", "rotation_quaternion", "scale"):
                pb[name].keyframe_insert(prop, frame=fr)
    for fc in arm.animation_data.action.fcurves:
        bone = fc.data_path.split('"')[1]
        for kp in fc.keyframe_points:
            kp.interpolation = KEYS[bone][0]

    # ---- constraints (explicit spaces; the rest are Blender defaults)
    def tgt(con, sub, space="WORLD"):
        con.target, con.subtarget = arm, sub
        con.target_space = space

    ik = pb["Arm.L"].constraints.new("IK")
    ik.target, ik.subtarget = arm, "IK.T"
    ik.pole_target, ik.pole_subtarget = arm, "Pole"
    ik.pole_angle = 0.4
    ik.chain_count = 2
    c = pb["RotCopy"].constraints.new("COPY_ROTATION")
    tgt(c, "Src")
    c.owner_space = "WORLD"
    c = pb["Follow"].constraints.new("COPY_LOCATION")
    tgt(c, "Src", "POSE")
    c.owner_space = "POSE"
    c = pb["Aim"].constraints.new("DAMPED_TRACK")
    tgt(c, "IK.T")
    c.track_axis = "TRACK_Y"
    c = pb["Lim"].constraints.new("LIMIT_ROTATION")
    c.use_limit_z, c.min_z, c.max_z = True, -0.3, 0.5
    c.owner_space = "LOCAL"
    childof = pb["Kid"].constraints.new("CHILD_OF")
    childof.target, childof.subtarget = arm, "Src"
    scene.frame_set(1)
    childof.set_inverse_pending = True  # Set Inverse at frame 1, evaluated on the next update
    bpy.context.evaluated_depsgraph_get().update()

    # ---- drivers
    arm["Smile"] = 0.0
    arm.id_properties_ui("Smile").update(min=0.0, max=1.0, default=0.0)

    def drive(owner, path, index, expr, vtype, setup):
        fc = owner.driver_add(path, index) if index >= 0 else owner.driver_add(path)
        d = fc.driver
        d.type = "SCRIPTED"
        d.expression = expr
        v = d.variables.new()
        v.name = "var"
        v.type = vtype
        setup(v)

    def prop_var(v):
        v.targets[0].id = arm
        v.targets[0].data_path = '["Smile"]'

    def rot_var(v):
        t = v.targets[0]
        t.id = arm
        t.bone_target = "Src"
        t.transform_type = "ROT_Z"
        t.transform_space = "LOCAL_SPACE"
        t.rotation_mode = "AUTO"

    drive(pb["Smile"], "location", 0, "var*0.2", "SINGLE_PROP", prop_var)
    drive(pb["DrvBy"], "location", 1, "var*0.1", "TRANSFORMS", rot_var)
    tmod = gp.grease_pencil_modifiers["Time"]
    drive(tmod, "offset", -1, "1+var*2.99", "SINGLE_PROP", prop_var)
    bpy.ops.object.mode_set(mode="OBJECT")

    # ---- results
    result = {"blender": bpy.app.version_string, "units": "armature space, Blender units",
              "rig": {"bones": [b[0] for b in BONES], "vertex_groups": [vg.name for vg in gp.vertex_groups]},
              "weights": weights, "frames": {}}
    for fr in FRAMES:
        arm["Smile"] = SMILE[fr]
        arm.update_tag()  # an ID property write does not tag the depsgraph by itself
        scene.frame_set(fr)
        dg = bpy.context.evaluated_depsgraph_get()
        dg.update()
        ae = arm.evaluated_get(dg)
        bones = {}
        for b in ae.pose.bones:
            bones[b.name] = {"matrix": [list(r) for r in b.matrix], "head": list(b.head), "tail": list(b.tail)}
        ge = gp.evaluated_get(dg).data
        strokes = {}
        for layer in ge.layers:
            af = layer.active_frame
            strokes[layer.info] = {"frame": af.frame_number if af else None,
                                   "strokes": [[list(p.co) + [p.pressure, p.strength] for p in s.points]
                                               for s in (af.strokes if af else [])]}
        result["frames"][str(fr)] = {"bones": bones, "strokes": strokes, "smile": SMILE[fr]}
    with open(out_path, "w") as fh:
        json.dump(result, fh, indent=1)
    print("rig reference written", out_path)


main()
