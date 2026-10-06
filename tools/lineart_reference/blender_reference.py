"""Reference output of Blender 3.6.23's own Line Art for the scenes in scenes.txt.

Run by tools/lineart_reference/run_blender_reference.sh:
    blender --background --factory-startup --python blender_reference.py -- scenes.txt OUTDIR

For each scene: an empty scene, the OBJ imported with Blender's importer (default axes, like
Scene-lite), a camera with the listed parameters placed by the same orbit as
pg_lite_camera_orbit(), the render size, and a Grease Pencil object with a default Line Art
modifier (source: scene) except overscan 0 and stroke depth offset 0, so the generated stroke points
are the Line Art chain points in world space. Optional key=value tokens after level_end set Line Art
options (see scenes.txt and apply_options()). OUTDIR/<name>.txt lists the strokes:
    stroke <point count>
    x y z            (one line per point, world space)
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector


def orbit_matrix(target, yaw, pitch, distance):
    """pg_lite_camera_orbit(): camera on an orbit around target, looking at it, Z up."""
    d = max(distance, 1e-4)
    p = Vector((target[0] + d * math.cos(pitch) * math.sin(yaw),
                target[1] - d * math.cos(pitch) * math.cos(yaw),
                target[2] + d * math.sin(pitch)))
    z = (p - Vector(target)).normalized()
    x = Vector((0.0, 0.0, 1.0)).cross(z)
    if x.length_squared < 1e-12:
        x = Vector((1.0, 0.0, 0.0))
    x.normalize()
    y = z.cross(x)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0] = x[i]
        m[i][1] = y[i]
        m[i][2] = z[i]
        m[i][3] = p[i]
    return m


LINE_TYPES = {
    "contour": "use_contour", "crease": "use_crease", "material": "use_material",
    "edge_mark": "use_edge_mark", "intersection": "use_intersection", "loose": "use_loose",
    "light_contour": "use_light_contour", "shadow": "use_shadow",
}
SHADOW_FILTER = ["NONE", "ILLUMINATED", "SHADED", "ILLUMINATED_ENCLOSED"]
SILHOUETTE_FILTER = {0: "NONE", 1: "GROUP", 2: "INDIVIDUAL"}


def bits8(value):
    return [bool((int(value) >> i) & 1) for i in range(8)]


def apply_options(options, scene, mod):
    """The Line Art options of scenes.txt (mirrored by apply_option() in test_lineart_reference.c)."""
    collections = {}
    for opt in options:
        key, value = opt.split("=", 1)
        if key == "types":
            for prop in LINE_TYPES.values():
                setattr(mod, prop, False)
            for t in value.split("+"):
                setattr(mod, LINE_TYPES[t], True)
        elif key == "crease":
            mod.crease_threshold = math.radians(float(value))
        elif key == "level":
            mod.use_multiple_levels = False
            mod.level_start = int(value)
        elif key == "levels":
            start, end = value.split(":")
            mod.use_multiple_levels = True
            mod.level_start = int(start)
            mod.level_end = int(end)
        elif key.startswith("usage:"):
            bpy.data.objects[key[6:]].lineart.usage = value
        elif key.startswith("coll:"):
            ob = bpy.data.objects[key[5:]]
            coll = collections.get(value)
            if coll is None:
                coll = bpy.data.collections.new(value)
                scene.collection.children.link(coll)
                collections[value] = coll
            for c in list(ob.users_collection):
                c.objects.unlink(ob)
            coll.objects.link(ob)
        elif key.startswith("collusage:"):
            collections[key[10:]].lineart_usage = value
        elif key.startswith("collmask:"):
            coll = collections[key[9:]]
            coll.lineart_use_intersection_mask = True
            coll.lineart_intersection_mask = bits8(value)
        elif key.startswith("matmask:"):
            mat = bpy.data.materials[key[8:]]
            mat.lineart.use_material_mask = True
            mat.lineart.use_material_mask_bits = bits8(value)
        elif key.startswith("matocc:"):
            bpy.data.materials[key[7:]].lineart.mat_occlusion = int(value)
        elif key == "maskswitch":
            mod.use_material_mask = bool(int(value) & 1)
            mod.use_material_mask_match = bool(int(value) & 2)
            mod.use_intersection_match = bool(int(value) & 4)
        elif key == "maskbits":
            mod.use_material_mask_bits = bits8(value)
        elif key == "isectmask":
            mod.use_intersection_mask = bits8(value)
        elif key == "chain":
            mod.chaining_image_threshold = float(value)
        elif key == "smooth":
            mod.smooth_tolerance = float(value)
        elif key == "loosechain":
            mod.use_loose_edge_chain = value == "1"
        elif key == "geomchain":
            mod.use_geometry_space_chain = value == "1"
        elif key == "light":
            kind, lyaw, lpitch, ldist = value.split(":")
            light = bpy.data.lights.new("Light", 'SUN' if kind == "sun" else 'POINT')
            lob = bpy.data.objects.new("Light", light)
            scene.collection.objects.link(lob)
            lob.matrix_world = orbit_matrix((0.0, 0.0, 0.0), float(lyaw), float(lpitch), float(ldist))
            mod.light_contour_object = lob
        elif key == "shadowsel":
            mod.shadow_region_filtering = SHADOW_FILTER[int(value)]
        elif key == "silhouette":
            mod.silhouette_filtering = SILHOUETTE_FILTER[int(value)]
        elif key == "source":
            kind, ref = value.split(":", 1)
            if kind == "object":
                mod.source_type = 'OBJECT'
                mod.source_object = bpy.data.objects[ref]
            else:
                mod.source_type = 'COLLECTION'
                mod.source_collection = collections[ref]
        elif key == "invertcoll":
            mod.use_invert_collection = value == "1"
        else:
            raise SystemExit("unknown option " + opt)


def run_scene(fields, scene_dir, out_dir):
    (name, obj, cam_type, lens, ortho_scale, yaw, pitch, distance, shift_x, shift_y,
     width, height, level_end) = fields[:13]
    options = fields[13:]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    bpy.ops.wm.obj_import(filepath=os.path.join(scene_dir, obj))

    cam_data = bpy.data.cameras.new("Camera")
    cam_data.type = 'ORTHO' if cam_type == "ortho" else 'PERSP'
    cam_data.lens = float(lens)
    cam_data.ortho_scale = float(ortho_scale)
    cam_data.sensor_width = 36.0
    cam_data.sensor_height = 24.0
    cam_data.sensor_fit = 'AUTO'
    cam_data.shift_x = float(shift_x)
    cam_data.shift_y = float(shift_y)
    cam_data.clip_start = 0.1
    cam_data.clip_end = 100.0
    cam = bpy.data.objects.new("Camera", cam_data)
    scene.collection.objects.link(cam)
    cam.matrix_world = orbit_matrix((0.0, 0.0, 0.0), float(yaw), float(pitch), float(distance))
    scene.camera = cam

    scene.render.resolution_x = int(width)
    scene.render.resolution_y = int(height)
    scene.render.resolution_percentage = 100

    gpd = bpy.data.grease_pencils.new("LineArt")
    gpo = bpy.data.objects.new("LineArt", gpd)
    scene.collection.objects.link(gpo)
    layer = gpd.layers.new("Lines")
    layer.frames.new(scene.frame_current)
    mat = bpy.data.materials.new("Line")
    bpy.data.materials.create_gpencil_data(mat)
    gpd.materials.append(mat)

    mod = gpo.grease_pencil_modifiers.new("Line Art", 'GP_LINEART')
    mod.source_type = 'SCENE'
    mod.target_layer = "Lines"
    mod.target_material = mat
    mod.use_multiple_levels = True
    mod.level_start = 0
    mod.level_end = int(level_end)
    mod.overscan = 0.0
    mod.stroke_depth_offset = 0.0
    apply_options(options, scene, mod)

    depsgraph = bpy.context.evaluated_depsgraph_get()
    depsgraph.update()
    evaluated = gpo.evaluated_get(depsgraph)
    frame = evaluated.data.layers["Lines"].frames[0]
    with open(os.path.join(out_dir, name + ".txt"), "w") as f:
        f.write("# Blender %s Line Art, scene %s\n" % (bpy.app.version_string, name))
        for stroke in frame.strokes:
            f.write("stroke %d\n" % len(stroke.points))
            for pt in stroke.points:
                co = evaluated.matrix_world @ pt.co
                f.write("%.7f %.7f %.7f\n" % (co.x, co.y, co.z))
    print("reference", name, len(frame.strokes), "strokes")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    manifest, out_dir = argv[0], argv[1]
    os.makedirs(out_dir, exist_ok=True)
    scene_dir = os.path.join(os.path.dirname(os.path.abspath(manifest)), "scenes")
    with open(manifest) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            run_scene(line.split(), scene_dir, out_dir)


main()
