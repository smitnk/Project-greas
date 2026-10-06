/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Scene-lite: the minimal 3D reference data Blender's Line Art needs, beside the 2D GP backend
 * (SPEC_LINE_ART_ARCHITECTURE batch 1). Line Art reads evaluated meshes of scene objects and a
 * camera; Project Grease has no Main/Object/Mesh/Depsgraph, so this module holds just what the
 * Line Art loaders read: per object a world matrix, vertices, triangles that remember their
 * polygon and material, and the unique edges with their two adjacent triangles and edge flags;
 * and one camera with Blender's Camera parameters. Opt-in: nothing in the 2D path uses it.
 *
 * Coordinates are Blender's (right-handed, Z up). Matrices are Blender float[4][4] layout
 * (m[3] is the translation column).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Edge flags (meaning of Blender's mesh edge data Line Art reads). */
enum {
  PG_LITE_EDGE_LOOSE = 1 << 0,             /* not used by any face */
  PG_LITE_EDGE_SHARP = 1 << 1,             /* sharp_edge attribute */
  PG_LITE_EDGE_SEAM = 1 << 2,              /* UV seam */
  PG_LITE_EDGE_FREESTYLE = 1 << 3,         /* freestyle edge mark ("edge marks" line type) */
  PG_LITE_EDGE_MATERIAL_BOUNDARY = 1 << 4, /* the two faces use different materials */
  PG_LITE_EDGE_NON_MANIFOLD = 1 << 5,      /* used by more than two triangles */
  PG_LITE_EDGE_POLY_INTERNAL = 1 << 6,     /* triangulation diagonal inside one polygon: not a
                                            * mesh edge (Line Art only registers real edges) */
};

/* Object line art usage: the values of eObjectLineArt_Usage (DNA_object_types.h). */
enum {
  PG_LITE_USAGE_INHERIT = 0,
  PG_LITE_USAGE_INCLUDE = (1 << 0),
  PG_LITE_USAGE_OCCLUSION_ONLY = (1 << 1),
  PG_LITE_USAGE_EXCLUDE = (1 << 2),
  PG_LITE_USAGE_INTERSECTION_ONLY = (1 << 3),
  PG_LITE_USAGE_NO_INTERSECTION = (1 << 4),
  PG_LITE_USAGE_FORCE_INTERSECTION = (1 << 5),
};

/* Object line art flags (eObjectLineArt_Flags). */
enum {
  PG_LITE_OBJECT_OWN_CREASE = (1 << 0),
  PG_LITE_OBJECT_OWN_INTERSECTION_PRIORITY = (1 << 1),
};

/* Collection line art usage (eCollectionLineArt_Usage, DNA_collection_types.h). */
enum {
  PG_LITE_COLLECTION_INCLUDE = 0,
  PG_LITE_COLLECTION_OCCLUSION_ONLY = (1 << 0),
  PG_LITE_COLLECTION_EXCLUDE = (1 << 1),
  PG_LITE_COLLECTION_INTERSECTION_ONLY = (1 << 2),
  PG_LITE_COLLECTION_NO_INTERSECTION = (1 << 3),
  PG_LITE_COLLECTION_FORCE_INTERSECTION = (1 << 4),
};
/* Collection line art flags (eCollectionLineArt_Flags) and the Collection::flag hide bits. */
enum {
  PG_LITE_COLLECTION_USE_INTERSECTION_MASK = (1 << 0),
  PG_LITE_COLLECTION_USE_INTERSECTION_PRIORITY = (1 << 1),
};
enum {
  PG_LITE_COLLECTION_HIDE_VIEWPORT = (1 << 0), /* COLLECTION_HIDE_VIEWPORT */
  PG_LITE_COLLECTION_HIDE_RENDER = (1 << 3),   /* COLLECTION_HIDE_RENDER */
};

/* Material line art flags (eMaterialLineArtFlags, DNA_material_types.h). */
enum {
  PG_LITE_MATERIAL_MASK_ENABLED = (1 << 0),
  PG_LITE_MATERIAL_CUSTOM_OCCLUSION_EFFECTIVENESS = (1 << 1),
  PG_LITE_MATERIAL_CUSTOM_INTERSECTION_PRIORITY = (1 << 2),
};

/* Light::type values that matter to Line Art: LA_SUN is orthographic; every other light (and a
 * non-light light_contour_object) is a perspective "camera" for shadows / light contour. */
enum { PG_LITE_LIGHT_POINT = 0, PG_LITE_LIGHT_SUN = 1, PG_LITE_LIGHT_SPOT = 2, PG_LITE_LIGHT_AREA = 4 };

typedef struct PGLiteEdge {
  int v[2];
  int tri[2]; /* adjacent triangles, -1 when missing (tri[0] == -1: loose edge) */
  int flag;
} PGLiteEdge;

typedef struct PGMeshLite {
  float (*verts)[3];
  int totvert;
  int (*tris)[3];
  int *tri_poly;     /* original polygon index of every triangle */
  int *tri_material; /* material index of every triangle */
  int *tri_smooth;   /* 1 when the polygon is shaded smooth (OBJ "s 1"), 0 flat (Blender's
                      * OBJ importer default; flat faces are Mesh "sharp_face") */
  int tottri;
  int totpoly;
  PGLiteEdge *edges;
  int totedge;
} PGMeshLite;

/* Vertex groups of a mesh (bDeformGroup names + MDeformVert weights, stored dense):
 * weights[g * totvert + v] is the weight of vertex v in group g, 0 for vertices not in the group
 * (Blender's BKE_defvert_ensure_index() gives those weight 0 as well). */
typedef struct PGVertexGroupsLite {
  char (*names)[64];
  float *weights;
  int totgroup;
} PGVertexGroupsLite;

typedef struct PGObjectLite {
  char name[64];
  float matrix_world[4][4];
  /* ObjectLineArt (Object::lineart): usage (PG_LITE_USAGE_*), flags (PG_LITE_OBJECT_*), the own
   * crease threshold (radians, with PG_LITE_OBJECT_OWN_CREASE) and own intersection priority. */
  int line_art_usage;
  int line_art_flags;
  float line_art_crease_threshold;
  int line_art_intersection_priority;
  /* The one collection the object is linked to (index in PGSceneLite::collections), -1 = the
   * scene's master collection. Blender objects may be in several collections; Scene-lite keeps
   * one, which is what the OBJ importer and the reference scenes produce. */
  int collection;
  PGMeshLite mesh;
  PGVertexGroupsLite vgroups;
} PGObjectLite;

/* Collection (DNA_collection_types.h subset). parent: index of the parent collection, -1 = a
 * child of the master collection; children are visited in index order (Collection::children). */
typedef struct PGCollectionLite {
  char name[64];
  int parent;
  int flag;                          /* PG_LITE_COLLECTION_HIDE_* */
  int lineart_usage;                 /* PG_LITE_COLLECTION_* usage */
  int lineart_flags;                 /* PG_LITE_COLLECTION_USE_* */
  int lineart_intersection_mask;     /* 8 bits */
  int lineart_intersection_priority; /* 0..255 */
} PGCollectionLite;

/* Material (Material::lineart and the MA_BL_CULL_BACKFACE blend flag). PGMeshLite::tri_material
 * indexes PGSceneLite::materials directly (one slot list shared by the scene, which is what
 * Blender's OBJ importer produces per material name); -1 = no material, as when
 * BKE_object_material_get() returns NULL. */
typedef struct PGMaterialLite {
  char name[64];
  int lineart_flags;         /* PG_LITE_MATERIAL_* */
  int material_mask_bits;    /* 8 bits */
  int mat_occlusion;         /* 0..255, Blender default 1 */
  int intersection_priority; /* 0..255 */
  int use_backface_culling;  /* MA_BL_CULL_BACKFACE */
} PGMaterialLite;

/* The Line Art modifier's light_contour_object (shadow / light contour need it). */
typedef struct PGLightLite {
  int present;
  int type; /* PG_LITE_LIGHT_* */
  float matrix_world[4][4];
} PGLightLite;

/* Camera (DNA_camera_types.h subset; defaults of a new Blender camera). */
typedef struct PGCameraLite {
  int type;           /* 0 = CAM_PERSP, 1 = CAM_ORTHO */
  float lens;         /* mm, 50 */
  float ortho_scale;  /* 6 */
  float sensor_x, sensor_y; /* mm, 36 / 24 */
  int sensor_fit;     /* CAMERA_SENSOR_FIT_AUTO / HOR / VERT = 0 / 1 / 2 */
  float shift_x, shift_y;
  float clip_start, clip_end; /* 0.1 / 100 */
  float matrix_world[4][4];
} PGCameraLite;

typedef struct PGSceneLite {
  PGObjectLite *objects;
  int totobject;
  PGCameraLite camera;
  int width, height; /* render size (scene->r.xsch / ysch) */
  PGCollectionLite *collections;
  int totcollection;
  PGMaterialLite *materials;
  int totmaterial;
  PGLightLite light;
} PGSceneLite;

PGSceneLite *pg_lite_scene_create(void);
void pg_lite_scene_free(PGSceneLite *scene);
/* Removes all objects, collections and materials (the camera and the light are kept). */
void pg_lite_scene_clear(PGSceneLite *scene);

/* Adds a collection with Line Art defaults (include, no mask / priority) under `parent` (-1 =
 * master collection). Returns its index, -1 on failure. */
int pg_lite_add_collection(PGSceneLite *scene, const char *name, int parent);
/* The material of that name, created with Blender's defaults (mat_occlusion 1) when missing.
 * Returns its index, -1 on failure. */
int pg_lite_material_ensure(PGSceneLite *scene, const char *name);
/* Index by name, -1 when missing. */
int pg_lite_find_object(const PGSceneLite *scene, const char *name);
int pg_lite_find_material(const PGSceneLite *scene, const char *name);
/* Adds a vertex group to an object (all weights 0). Returns its index, -1 on failure. */
int pg_lite_object_add_vertex_group(PGObjectLite *ob, const char *name);

/* Wavefront OBJ: o/g start objects, v, f (any polygon size, v, v/vt, v/vt/vn, v//vn and negative
 * indices; polygons are fan-triangulated and keep their polygon index), l (loose edges), usemtl
 * (scene material per name via pg_lite_material_ensure(); -1 before any usemtl), s (smooth groups: s 0 / off = flat,
 * the default). Appends the objects to the scene
 * (identity matrix, usage INHERIT); returns the number added, 0 on malformed input (then
 * nothing is added). Also converts OBJ's Y-up to Blender's Z-up like Blender's importer
 * (forward -Z, up Y): (x, y, z) -> (x, -z, y). */
int pg_lite_load_obj(PGSceneLite *scene, const char *text, int length);

/* Builds unique edges, triangle adjacency and the derived flags (loose, material boundary,
 * non-manifold) from verts/tris/tri_material. Called by the OBJ loader. Returns 0 on failure. */
int pg_lite_mesh_build_edges(PGMeshLite *mesh, const int (*loose_edges)[2], int loose_count);

/* Camera defaults of a new Blender camera, looking down -Y from (0, -10, 0)... see the .c. */
void pg_lite_camera_default(PGCameraLite *camera);
/* Places the camera on an orbit around `target`: yaw around Z, pitch above the XY plane
 * (radians), at `distance`, looking at the target with Z up. */
void pg_lite_camera_orbit(PGCameraLite *camera, const float target[3], float yaw, float pitch, float distance);

/* Line Art's view-projection (lineart_main_load_geometries): projection from lens / sensor /
 * fit / ortho_scale / clip and the render aspect, times the inverse of the camera matrix with
 * its axes normalized. `overscan` as in the modifier (0 = none). Column-major double[4][4]. */
void pg_lite_view_projection(const PGCameraLite *camera, int width, int height, float overscan,
                             double r_view_projection[4][4]);
/* Line Art's shift_x / shift_y (lineart_main_get_view_vector / init): adjusted for sensor fit. */
void pg_lite_camera_shift(const PGCameraLite *camera, int width, int height, double *r_shift_x, double *r_shift_y);

/* World point -> Line Art frame-buffer coordinates (-1..1, after perspective division and shift,
 * like LineartVert::fbcoord). Returns 0 when the point is behind the camera (w <= 0). */
int pg_lite_project(const double view_projection[4][4], double shift_x, double shift_y,
                    const float world[3], double r_fb[2]);

/* Counts for UI/tests: objects, vertices, triangles, edges, loose edges. */
void pg_lite_stats(const PGSceneLite *scene, int r_counts[5]);

#ifdef __cplusplus
}
#endif
