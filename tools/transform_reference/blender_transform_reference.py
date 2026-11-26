"""Blender 3.6.23's own Grease Pencil edit-mode transform results, for
native/blender_gp/tests/test_transform_blender_parity.cc.

    xvfb-run -a blender --factory-startup --python blender_transform_reference.py -- OUT.json

Not with -b: without a drawn window the 3D View region has no region type / view data, so the
transform either crashes (ED_region_draw_cb_activate on a NULL region type) or, without a region,
runs as SPACE_EMPTY, which ignores the pivot point (t_around_get returns Bounding Box Center).
The script runs from a timer once the window exists, writes OUT.json and exits (status 1 on error).

Runs bpy.ops.transform.translate / rotate / resize / mirror on the selected points of a legacy GP
object in Edit Mode, inside a VIEW_3D temp_override (top orthographic view, so the canvas is the XY
plane), for each pivot, proportional falloff, Connected Only, multi-frame falloff and increment
snapping. Coordinates are the canvas units the app stores in bGPDspoint (x, y, 0), unconverted.
The fixture and the CASES table are duplicated in the C++ test; "cases" in the JSON lets it check
they agree.
"""
import json
import os
import sys
import traceback

import bpy

# frame -> strokes; each stroke is a list of (x, y, selected)
def line(x0, y0, dx, dy, n, sel):
    return [(x0 + dx * i, y0 + dy * i, sel(i)) for i in range(n)]

FIXTURE = {
    1: [line(0, 0, 10, 2, 12, lambda i: 4 <= i <= 6),     # partly selected
        line(40, 60, 6, -3, 8, lambda i: True),          # fully selected
        line(-30, 20, 5, 9, 7, lambda i: False)],       # unselected
    5: [line(0, 0, 11, 1, 10, lambda i: i in (2, 3)),
        line(-20, -40, 7, 5, 6, lambda i: False)],
    9: [line(5, 5, 9, -2, 10, lambda i: i >= 7)],
}
ACTIVE = 5

# name, op, value, pivot, prop (None | (falloff, size, connected)), snap, multiframe (None | falloff)
PIVOTS = ["MEDIAN_POINT", "BOUNDING_BOX_CENTER", "INDIVIDUAL_ORIGINS", "CURSOR"]
CURSOR = (13.0, -7.0)
FALLOFFS = ["SMOOTH", "SPHERE", "ROOT", "INVERSE_SQUARE", "SHARP", "LINEAR", "CONSTANT"]
CASES = []
for op, val in (("translate", (14.5, -9.25)), ("rotate", (0.7,)), ("resize", (1.6, 0.55)), ("mirror", (1,))):
    for p in PIVOTS:
        CASES.append(("%s_%s" % (op, p), op, val, p, None, False, False))
    CASES.append(("%s_multiframe" % op, op, val, "MEDIAN_POINT", None, False, False))
    if op == "mirror":  # TRANSFORM_OT_mirror has no proportional properties
        continue
    for f in FALLOFFS:
        CASES.append(("%s_prop_%s" % (op, f), op, val, "MEDIAN_POINT", (f, 35.0, False), False, False))
    CASES.append(("%s_prop_connected" % op, op, val, "MEDIAN_POINT", ("SMOOTH", 35.0, True), False, False))
    for p in PIVOTS[1:]:
        CASES.append(("%s_prop_%s" % (op, p), op, val, p, ("LINEAR", 50.0, False), False, False))
    CASES.append(("%s_multiframe_falloff_prop" % op, op, val, "MEDIAN_POINT", ("SMOOTH", 35.0, False), False, True))
# Axis constraints (transform_constraints.c): exact values with constraint_axis and orient_type, median
# pivot. A one-axis lock is constraint_axis with that axis; a plane lock (Shift+axis) the other two.
# Translate and resize exec take the value in the orientation space whatever the axes
# (T_INPUT_IS_VALUES_FINAL); rotate turns about the constrained axis, without the view flip.
# name, op, value (3), constraint_axis, orient_type, prop (None | (falloff, size, connected))
LOCKS = {"X": (True, False, False), "Y": (False, True, False), "Z": (False, False, True),
         "planeX": (False, True, True), "planeY": (True, False, True), "planeZ": (True, True, False)}
CON_CASES = []
for op, vals in (("translate", {"X": (5.5, 0, 0), "Y": (0, -7.25, 0), "Z": (0, 0, 3), "planeX": (0, 6, 2),
                                "planeY": (3, 0, -2), "planeZ": (4, -3, 0)}),
                 ("rotate", {k: (0.7, 0, 0) for k in LOCKS}),
                 ("resize", {"X": (1.6, 1, 1), "Y": (1, 0.55, 1), "Z": (1, 1, 2), "planeX": (1, 0.55, 2),
                             "planeY": (1.6, 1, 2), "planeZ": (1.6, 0.55, 1)})):
    for lock, v in vals.items():
        CON_CASES.append(("con_%s_%s_GLOBAL" % (op, lock), op, v, LOCKS[lock], "GLOBAL", None))
    for lock in ("X", "planeZ"):
        CON_CASES.append(("con_%s_%s_LOCAL" % (op, lock), op, vals[lock], LOCKS[lock], "LOCAL", None))
    CON_CASES.append(("con_%s_Y_prop" % op, op, vals["Y"], LOCKS["Y"], "GLOBAL", ("SMOOTH", 35.0, False)))
# Increment snapping is not here: an exec with given values (T_INPUT_IS_VALUES_FINAL) never snaps,
# and the modal needs a drawn region; the C++ test checks snap_increment_apply_ex directly.


def multiframe_of(name):
    return "multiframe" in name


def main():
    out = sys.argv[sys.argv.index("--") + 1]
    scene = bpy.context.scene
    view = bpy.context.view_layer
    for o in list(scene.objects):
        bpy.data.objects.remove(o)
    gpd = bpy.data.grease_pencils.new("GP")
    gp = bpy.data.objects.new("GP", gpd)
    scene.collection.objects.link(gp)
    mat = bpy.data.materials.new("M")
    bpy.data.materials.create_gpencil_data(mat)
    gpd.materials.append(mat)
    layer = gpd.layers.new("L")
    frames = {}
    for fn, strokes in FIXTURE.items():
        f = layer.frames.new(fn)
        frames[fn] = f
        for pts in strokes:
            s = f.strokes.new()
            s.display_mode = "3DSPACE"
            s.points.add(len(pts))
            for i, (x, y, _) in enumerate(pts):
                s.points[i].co = (x, y, 0.0)
    scene.frame_set(ACTIVE)
    view.objects.active = gp
    gp.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT_GPENCIL")
    ts = scene.tool_settings
    scene.cursor.location = (CURSOR[0], CURSOR[1], 0.0)

    win = bpy.context.window_manager.windows[0]
    area = next(a for a in win.screen.areas if a.type == "VIEW_3D")
    region = next(r for r in area.regions if r.type == "WINDOW")

    def reset(multi):
        gpd.use_multiedit = multi
        for fn, strokes in FIXTURE.items():
            scene.frame_set(fn)
            f = frames[fn]
            f.select = multi  # GP_FRAME_SELECT
            for s, pts in zip(f.strokes, strokes):
                # the RNA stroke select setter (de)selects every point; the point setter only finds
                # strokes of the active frame (rna_GPencil_stroke_point_find_stroke), hence frame_set
                s.select = any(sel for _, _, sel in pts)
                for i, (x, y, sel) in enumerate(pts):
                    s.points[i].co = (x, y, 0.0)
                    s.points[i].select = sel
        scene.frame_set(ACTIVE)
        sel = [p.select for f in frames.values() for s in f.strokes for p in s.points]
        want = [p[2] for strokes in FIXTURE.values() for pts in strokes for p in pts]
        if sel != want:
            raise RuntimeError("selection did not take: %s" % [(i, a, b) for i, (a, b) in enumerate(zip(sel, want)) if a != b])

    results = []
    for name, op, val, pivot, prop, snap, mf in CASES:
        multi = multiframe_of(name)
        reset(multi)
        ts.transform_pivot_point = pivot
        ts.use_snap = snap
        ts.snap_elements = {"INCREMENT"}
        ts.use_gpencil_select_mask_point = True
        ts.gpencil_sculpt.use_multiframe_falloff = mf  # GP_SCULPT_SETT_FLAG_FRAME_FALLOFF
        kw = dict(use_proportional_edit=prop is not None)
        if prop:
            kw.update(proportional_edit_falloff=prop[0], proportional_size=prop[1],
                      use_proportional_connected=prop[2], use_proportional_projected=False)
        with bpy.context.temp_override(window=win, area=area, region=region, active_object=gp,
                                       object=gp, edit_object=gp):
            if op == "translate":
                bpy.ops.transform.translate(value=(val[0], val[1], 0.0), orient_type="GLOBAL", **kw)
            elif op == "rotate":
                bpy.ops.transform.rotate(value=val[0], orient_axis="Z", orient_type="GLOBAL", **kw)
            elif op == "resize":
                bpy.ops.transform.resize(value=(val[0], val[1], 1.0), orient_type="GLOBAL", **kw)
            else:
                bpy.ops.transform.mirror(orient_type="GLOBAL", constraint_axis=(True, False, False))
        res = {}
        for fn in FIXTURE:
            res[str(fn)] = [[[p.co[0], p.co[1]] for p in s.points] for s in frames[fn].strokes]
        results.append({"name": name, "frames": res})
    for name, op, val, axes, orient, prop in CON_CASES:
        reset(False)
        ts.transform_pivot_point = "MEDIAN_POINT"
        ts.use_snap = False
        ts.gpencil_sculpt.use_multiframe_falloff = False
        kw = dict(use_proportional_edit=prop is not None, constraint_axis=axes, orient_type=orient)
        if prop:
            kw.update(proportional_edit_falloff=prop[0], proportional_size=prop[1],
                      use_proportional_connected=prop[2], use_proportional_projected=False)
        with bpy.context.temp_override(window=win, area=area, region=region, active_object=gp,
                                       object=gp, edit_object=gp):
            if op == "translate":
                bpy.ops.transform.translate(value=val, **kw)
            elif op == "rotate":
                bpy.ops.transform.rotate(value=val[0], orient_axis="Z", **kw)
            else:
                bpy.ops.transform.resize(value=val, **kw)
        res = {}
        for fn in FIXTURE:
            res[str(fn)] = [[list(p.co) for p in s.points] for s in frames[fn].strokes]
        results.append({"name": name, "frames": res})
    with open(out, "w") as fh:
        json.dump({"blender": bpy.app.version_string, "active": ACTIVE, "cursor": CURSOR,
                   "cases": [c[0] for c in CASES] + [c[0] for c in CON_CASES], "results": results}, fh, indent=1)


def run():
    status = 0
    try:
        main()
    except Exception:
        traceback.print_exc()
        status = 1
    sys.stdout.flush()
    os._exit(status)  # no quit.blend, no exit handlers


if bpy.app.background:
    sys.stderr.write("blender_transform_reference.py needs a window: run it under xvfb-run, without -b\n")
    sys.exit(1)
bpy.app.timers.register(run, first_interval=0.5)
