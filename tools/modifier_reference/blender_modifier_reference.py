"""Blender 3.6.23's own evaluated Grease Pencil modifier output, for test_l7_modifier_parity.cc.

    blender -b --factory-startup --python-exit-code 1 --python blender_modifier_reference.py -- OUT.txt

The strokes are test_l7's add_stroke() fixture in object space (canvas px / 1000, y flipped, as the
harness's space()). For each case one modifier is added with the listed settings (the rest are
Blender's defaults), the depsgraph is evaluated at frame 1 and the evaluated strokes are written:
    case <name>
    stroke <point count> <line_width>
    x y z pressure strength      (one line per point, object space)
"""
import math
import sys

import bpy

PPU = 1000.0  # PG_MOD_PIXELS_PER_UNIT


def fixture():
    """add_stroke(gpf, 12, 10, 50, 18); add_stroke(gpf, 7, 40, 140, 6); add_stroke(gpf, 3, 5, 200, 2)."""
    out = []
    for n, x0, y0, wobble in ((12, 10.0, 50.0, 18.0), (7, 40.0, 140.0, 6.0), (3, 5.0, 200.0, 2.0)):
        pts = []
        for i in range(n):
            x = x0 + 14.0 * i
            y = y0 + wobble * math.sin(0.8 * i) + 2.0 * (i % 3)
            pts.append((x / PPU, -y / PPU, 0.0, 0.3 + 0.06 * i, 0.9 - 0.03 * i))
        out.append(pts)
    return out


# name (as in test_l7's case list) -> (modifier type, settings)
CASES = [
    ("Smooth default", "GP_SMOOTH", {}),
    ("Smooth 0.6 x3 all", "GP_SMOOTH", {"factor": 0.6, "step": 3, "use_edit_strength": True, "use_edit_thickness": True}),
    ("Subdivide default", "GP_SUBDIV", {}),
    ("Subdivide simple 2", "GP_SUBDIV", {"subdivision_type": "SIMPLE", "level": 2}),
    ("Simplify adaptive 0.3", "GP_SIMPLIFY", {"mode": "ADAPTIVE", "factor": 0.3}),
    ("Thickness normalize 12", "GP_THICK", {"use_normalized_thickness": True, "thickness": 12}),
    ("Offset loc/rot/scale", "GP_OFFSET", {"location": (0.3, -0.2, 0.0), "rotation": (0.0, 0.0, 0.4), "scale": (0.5, 0.0, 0.0)}),
]


def main():
    out_path = sys.argv[sys.argv.index("--") + 1]
    scene = bpy.context.scene
    scene.frame_set(1)
    gpd = bpy.data.grease_pencils.new("PG")
    ob = bpy.data.objects.new("Project Grease", gpd)
    scene.collection.objects.link(ob)
    mat = bpy.data.materials.new("M")
    bpy.data.materials.create_gpencil_data(mat)
    gpd.materials.append(mat)
    frame = gpd.layers.new("Layer").frames.new(1)
    src = fixture()
    for pts in src:
        s = frame.strokes.new()
        s.display_mode = "3DSPACE"
        s.line_width = 10  # BKE_gpencil_stroke_add(gpf, 0, n, 10, false)
        s.material_index = 0
        s.points.add(len(pts))
        for p, (x, y, z, pr, st) in zip(s.points, pts):
            p.co = (x, y, z)
            p.pressure = pr
            p.strength = st
    lines = []
    for name, kind, settings in CASES:
        md = ob.grease_pencil_modifiers.new(name[:60], kind)
        for k, v in settings.items():
            setattr(md, k, v)
        dg = bpy.context.evaluated_depsgraph_get()
        dg.update()
        ev = ob.evaluated_get(dg).data
        strokes = ev.layers[0].frames[0].strokes
        if kind == "GP_SUBDIV" and len(strokes[0].points) == len(src[0]):
            raise RuntimeError("evaluated data has no modifier applied")
        lines.append("case " + name)
        for s in strokes:
            lines.append("stroke %d %d" % (len(s.points), s.line_width))
            for p in s.points:
                lines.append("%.9g %.9g %.9g %.9g %.9g" % (p.co[0], p.co[1], p.co[2], p.pressure, p.strength))
        ob.grease_pencil_modifiers.remove(md)
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote %d cases to %s" % (len(CASES), out_path))


main()
