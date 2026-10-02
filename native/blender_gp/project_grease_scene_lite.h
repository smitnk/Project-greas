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

/* Object line art usage (eObjectLineArt_Usage). */
enum {
  PG_LITE_USAGE_INHERIT = 0,
  PG_LITE_USAGE_INCLUDE = 1,
  PG_LITE_USAGE_OCCLUSION_ONLY = 2,
  PG_LITE_USAGE_EXCLUDE = 3,
  PG_LITE_USAGE_INTERSECTION_ONLY = 4,
  PG_LITE_USAGE_NO_INTERSECTION = 5,
};

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
  int tottri;
  int totpoly;
  PGLiteEdge *edges;
  int totedge;
} PGMeshLite;

typedef struct PGObjectLite {
  char name[64];
  float matrix_world[4][4];
  int line_art_usage;
  PGMeshLite mesh;
} PGObjectLite;

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
} PGSceneLite;

PGSceneLite *pg_lite_scene_create(void);
void pg_lite_scene_free(PGSceneLite *scene);
/* Removes all objects (the camera is kept). */
void pg_lite_scene_clear(PGSceneLite *scene);

/* Wavefront OBJ: o/g start objects, v, f (any polygon size, v, v/vt, v/vt/vn, v//vn and negative
 * indices; polygons are fan-triangulated and keep their polygon index), l (loose edges), usemtl
 * (material index per distinct name, in order of first use). Appends the objects to the scene
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
