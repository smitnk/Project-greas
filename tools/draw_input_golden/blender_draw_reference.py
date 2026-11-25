"""Blender 3.6.23's own draw-stroke input, for native/blender_gp/tests/test_draw_blender_parity.c.

    xvfb-run -a blender --factory-startup --python blender_draw_reference.py -- OUT.txt

Runs with a window (not -b): gpencil_paint_strokeend() sets up the region's GL viewport
(view3d_region_operator_needs_opengl), which crashes without a GPU context. The script quits Blender
itself and exits 1 on any error.

Draws each input trace with bpy.ops.gpencil.draw(mode='DRAW', stroke=[...]) (gpencil_draw_exec:
gpencil_draw_apply -> gpencil_stroke_addpoint per element, then gpencil_stroke_newfrombuffer) with
the default brushes, inside a VIEW_3D override, and writes the brush settings the app needs, the
input (region pixels, y up) and the committed points projected back to region pixels.
Each trace is drawn twice: with Post-Processing (use_settings_postprocess) off, for the input points,
and with the brush's default Post-Processing (on for Pencil, Pen, Ink Pen, Marker Bold/Chisel, as
BKE_gpencil_brush_preset_set sets GP_BRUSH_GROUP_SETTINGS), for the committed point count ("post"
line: enabled, smooth factor, smooth steps, subdivide, simplify_f, region px per unit, points).
simplify_f is in object units (BKE_gpencil_stroke_simplify_adaptive runs on gps->points in 3D), so the
host test applies it as simplify_f * px per unit in region pixels. Random settings stay off: gpencil_session_initdata() seeds the RNG from the
clock and the operator cannot fix it, so the random path is checked by the host test with a fixed
seed instead. The exec path has no fake points (gpencil_add_fake_points runs only in the modal
handler), so the host test replays the traces with fake points off; the view is orthographic so the
3D round trip of the points (win_to_delta, location_3d_to_region_2d) stays exact to float precision.
"""
import math
import os
import sys
import traceback

import bpy
from bpy_extras import view3d_utils
from mathutils import Vector

BRUSHES = ["Pencil", "Pen", "Ink Pen", "Marker Bold", "Marker Chisel", "Airbrush"]


def traces():
    t = {}
    # straight-ish line, pressure ramp
    t["ramp"] = [(200 + 3 * i, 300 + round(10 * math.sin(i / 8)), 0.15 + 0.85 * i / 79, 0.01 * i)
                 for i in range(80)]
    # circle, pressure wave, near-zero tail (end truncation)
    pts = []
    for i in range(140):
        a = i / 140 * 2 * math.pi
        p = 0.5 + 0.4 * math.sin(3 * a)
        pts.append((600 + round(60 * math.cos(a)), 400 + round(60 * math.sin(a)), p, 0.008 * i))
    pts += [(pts[-1][0] + 2 * k, pts[-1][1] + 1, 0.0005, pts[-1][3] + 0.01 * k) for k in range(1, 4)]
    t["circle"] = pts
    # zigzag at constant pressure (angle factor, smoothing at corners)
    z = []
    x, y = 300, 600
    for i in range(70):
        seg = (i // 10) % 2
        x += 2
        y += 2 if seg == 0 else -2
        z.append((x, y, 0.8, 0.008 * i))
    t["zigzag"] = z
    # a straight line and a half circle at constant pressure: the default committed point counts
    t["line"] = [(200 + 4 * i, 500, 1.0, 0.01 * i) for i in range(101)]
    t["curve"] = [(round(700 + 150 * math.cos(i / 100 * math.pi), 3), round(300 + 150 * math.sin(i / 100 * math.pi), 3),
                   1.0, 0.01 * i) for i in range(101)]
    return t


def main():
    out_path = sys.argv[sys.argv.index("--") + 1]
    win = bpy.context.window_manager.windows[0]
    area = next(a for a in win.screen.areas if a.type == "VIEW_3D")
    region = next(r for r in area.regions if r.type == "WINDOW")
    rv3d = area.spaces.active.region_3d
    rv3d.view_perspective = "ORTHO"
    with bpy.context.temp_override(window=win, area=area, region=region):
        bpy.ops.object.gpencil_add(type="EMPTY")
        bpy.ops.object.mode_set(mode="PAINT_GPENCIL")
        # the ORTHO switch reaches the region matrices on the next redraw
        bpy.ops.wm.redraw_timer(type="DRAW_WIN_SWAP", iterations=1)
    ob = bpy.context.active_object
    ts = bpy.context.scene.tool_settings
    ts.gpencil_stroke_placement_view3d = "ORIGIN"
    # px per object unit on the drawing plane (the view plane through the origin, not the XY plane)
    o = view3d_utils.location_3d_to_region_2d(region, rv3d, ob.matrix_world @ Vector((0, 0, 0)))
    ppu = (view3d_utils.location_3d_to_region_2d(region, rv3d, ob.matrix_world @ (rv3d.view_rotation @ Vector((1, 0, 0)))) - o).length
    lines = ["# Blender %s draw input reference (blender_draw_reference.py); region px, y up" % bpy.app.version_string]
    for name in BRUSHES:
        b = bpy.data.brushes[name]
        g = b.gpencil_settings
        post = (g.use_settings_postprocess, g.pen_smooth_factor, g.pen_smooth_steps, g.pen_subdivision_steps,
                g.simplify_factor)
        g.use_settings_postprocess = False
        g.use_settings_random = False
        g.use_settings_stabilizer = False
        ts.gpencil_paint.brush = b
        curve = lambda cm: " ".join("%r %r" % tuple(p.location) for p in cm.curves[0].points)
        lines.append("brush %s|%r %d %d %r %d %r %r %r %d %s %d %s" % (
            name, g.pen_strength, int(g.use_pressure),
            int(g.use_strength_pressure), g.active_smooth_factor, g.input_samples, g.pen_jitter,
            g.angle_factor, g.angle, len(g.curve_sensitivity.curves[0].points), curve(g.curve_sensitivity),
            len(g.curve_strength.curves[0].points), curve(g.curve_strength)))
        for tname, tr in traces().items():
            stroke = [{"name": "", "location": (0, 0, 0), "mouse": (x, y), "mouse_event": (x, y),
                       "pressure": p, "size": 0, "pen_flip": False, "x_tilt": 0, "y_tilt": 0,
                       "time": t, "is_start": i == 0} for i, (x, y, p, t) in enumerate(tr)]
            frame = ob.data.layers.active.active_frame
            before = len(frame.strokes) if frame else 0
            with bpy.context.temp_override(window=win, area=area, region=region):
                r = bpy.ops.gpencil.draw(mode="DRAW", stroke=stroke, wait_for_input=False)
            frame = ob.data.layers.active.active_frame
            if r != {"FINISHED"} or len(frame.strokes) != before + 1:
                raise RuntimeError("draw %s %s: %s" % (name, tname, r))
            s = frame.strokes[-1]
            lines.append("trace %s %d %d" % (tname, len(tr), len(s.points)))
            for x, y, p, t in tr:
                lines.append("in %r %r %r %r" % (float(x), float(y), p, t))
            mw = ob.matrix_world
            for pt in s.points:
                co = view3d_utils.location_3d_to_region_2d(region, rv3d, mw @ pt.co)
                lines.append("out %r %r %r %r %r" % (co.x, co.y, pt.pressure, pt.strength, pt.time))
            g.use_settings_postprocess = post[0]
            with bpy.context.temp_override(window=win, area=area, region=region):
                r = bpy.ops.gpencil.draw(mode="DRAW", stroke=stroke, wait_for_input=False)
            g.use_settings_postprocess = False
            if r != {"FINISHED"}:
                raise RuntimeError("draw post %s %s: %s" % (name, tname, r))
            lines.append("post %d %r %d %d %r %r %d" % (int(post[0]), post[1], post[2], post[3], post[4], ppu,
                                                         len(frame.strokes[-1].points)))
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("BLENDER-DRAW-REFERENCE written", out_path)


try:
    main()
except Exception:
    traceback.print_exc()
    sys.stdout.flush()
    os._exit(1)
sys.stdout.flush()
os._exit(0)
