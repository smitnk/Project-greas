/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Scene-lite (see project_grease_scene_lite.h). The camera math that decides where Line Art
 * puts every line is Blender's: BKE_camera_sensor_size/fit (blenkernel/intern/camera.c),
 * focallength_to_fov (blenlib/intern/math_rotation.c) and lineart_matrix_perspective_44d /
 * lineart_matrix_ortho_44d (lineart_util.c) are carried verbatim (tools/verify_blender_verbatim.py)
 * as file-local copies, so this module links without blenkernel or the Line Art sources; the
 * glue reproduces the camera part of lineart_main_load_geometries() and lineart_main_init().
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "project_grease_scene_lite.h"

/* ---- Blender names used by the verbatim regions, mapped to file-local definitions ---------- */
enum { CAM_PERSP = 0, CAM_ORTHO = 1 };
enum { CAMERA_SENSOR_FIT_AUTO = 0, CAMERA_SENSOR_FIT_HOR = 1, CAMERA_SENSOR_FIT_VERT = 2 };

static void pg_lite_unit_m4_db(double m[4][4])
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) m[i][j] = i == j ? 1.0 : 0.0;
  }
}
#define unit_m4_db pg_lite_unit_m4_db
#define BKE_camera_sensor_size pg_lite_camera_sensor_size
#define BKE_camera_sensor_fit pg_lite_camera_sensor_fit
#define focallength_to_fov pg_lite_focallength_to_fov
#define lineart_matrix_perspective_44d pg_lite_matrix_perspective_44d
#define lineart_matrix_ortho_44d pg_lite_matrix_ortho_44d

static
/* BEGIN VERBATIM source/blender/blenkernel/intern/camera.c */
float BKE_camera_sensor_size(int sensor_fit, float sensor_x, float sensor_y)
{
  /* sensor size used to fit to. for auto, sensor_x is both x and y. */
  if (sensor_fit == CAMERA_SENSOR_FIT_VERT) {
    return sensor_y;
  }

  return sensor_x;
}
/* END VERBATIM */

static
/* BEGIN VERBATIM source/blender/blenkernel/intern/camera.c */
int BKE_camera_sensor_fit(int sensor_fit, float sizex, float sizey)
{
  if (sensor_fit == CAMERA_SENSOR_FIT_AUTO) {
    if (sizex >= sizey) {
      return CAMERA_SENSOR_FIT_HOR;
    }

    return CAMERA_SENSOR_FIT_VERT;
  }

  return sensor_fit;
}
/* END VERBATIM */

static
/* BEGIN VERBATIM source/blender/blenlib/intern/math_rotation.c */
float focallength_to_fov(float focal_length, float sensor)
{
  return 2.0f * atanf((sensor / 2.0f) / focal_length);
}
/* END VERBATIM */

static
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/lineart/lineart_util.c */
void lineart_matrix_perspective_44d(
    double (*mProjection)[4], double fFov_rad, double fAspect, double zMin, double zMax)
{
  double yMax;
  double yMin;
  double xMin;
  double xMax;

  if (fAspect < 1) {
    yMax = zMin * tan(fFov_rad * 0.5f);
    yMin = -yMax;
    xMin = yMin * fAspect;
    xMax = -xMin;
  }
  else {
    xMax = zMin * tan(fFov_rad * 0.5f);
    xMin = -xMax;
    yMin = xMin / fAspect;
    yMax = -yMin;
  }

  unit_m4_db(mProjection);

  mProjection[0][0] = (2.0f * zMin) / (xMax - xMin);
  mProjection[1][1] = (2.0f * zMin) / (yMax - yMin);
  mProjection[2][0] = (xMax + xMin) / (xMax - xMin);
  mProjection[2][1] = (yMax + yMin) / (yMax - yMin);
  mProjection[2][2] = -((zMax + zMin) / (zMax - zMin));
  mProjection[2][3] = -1.0f;
  mProjection[3][2] = -((2.0f * (zMax * zMin)) / (zMax - zMin));
  mProjection[3][3] = 0.0f;
}
/* END VERBATIM */

static
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/lineart/lineart_util.c */
void lineart_matrix_ortho_44d(double (*mProjection)[4],
                              double xMin,
                              double xMax,
                              double yMin,
                              double yMax,
                              double zMin,
                              double zMax)
{
  unit_m4_db(mProjection);

  mProjection[0][0] = 2.0f / (xMax - xMin);
  mProjection[1][1] = 2.0f / (yMax - yMin);
  mProjection[2][2] = -2.0f / (zMax - zMin);
  mProjection[3][0] = -((xMax + xMin) / (xMax - xMin));
  mProjection[3][1] = -((yMax + yMin) / (yMax - yMin));
  mProjection[3][2] = -((zMax + zMin) / (zMax - zMin));
  mProjection[3][3] = 1.0f;
}
/* END VERBATIM */

/* ---- small math --------------------------------------------------------------------------- */
static void unit_m4(float m[4][4])
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) m[i][j] = i == j ? 1.0f : 0.0f;
  }
}

static void normalize_v3(float v[3])
{
  const float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len > 1e-35f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

static void cross_v3(float r[3], const float a[3], const float b[3])
{
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
}

/* invert_m4_m4 (general 4x4 inverse, Gauss-Jordan with partial pivoting). */
static int invert_m4(float r[4][4], const float m[4][4])
{
  double a[4][8];
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) { a[i][j] = m[i][j]; a[i][j + 4] = i == j ? 1.0 : 0.0; }
  }
  for (int c = 0; c < 4; c++) {
    int p = c;
    for (int i = c + 1; i < 4; i++) if (fabs(a[i][c]) > fabs(a[p][c])) p = i;
    if (fabs(a[p][c]) < 1e-30) return 0;
    if (p != c) for (int j = 0; j < 8; j++) { double t = a[c][j]; a[c][j] = a[p][j]; a[p][j] = t; }
    const double d = a[c][c];
    for (int j = 0; j < 8; j++) a[c][j] /= d;
    for (int i = 0; i < 4; i++) {
      if (i == c) continue;
      const double f = a[i][c];
      for (int j = 0; j < 8; j++) a[i][j] -= f * a[c][j];
    }
  }
  for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) r[i][j] = (float)a[i][j + 4];
  return 1;
}

/* ---- scene / mesh ------------------------------------------------------------------------- */
static void mesh_free(PGMeshLite *me)
{
  free(me->verts); free(me->tris); free(me->tri_poly); free(me->tri_material); free(me->edges);
  memset(me, 0, sizeof(*me));
}

PGSceneLite *pg_lite_scene_create(void)
{
  PGSceneLite *scene = calloc(1, sizeof(PGSceneLite));
  if (!scene) return NULL;
  pg_lite_camera_default(&scene->camera);
  scene->width = 1920;
  scene->height = 1080;
  return scene;
}

void pg_lite_scene_clear(PGSceneLite *scene)
{
  if (!scene) return;
  for (int i = 0; i < scene->totobject; i++) mesh_free(&scene->objects[i].mesh);
  free(scene->objects);
  scene->objects = NULL;
  scene->totobject = 0;
}

void pg_lite_scene_free(PGSceneLite *scene)
{
  if (!scene) return;
  pg_lite_scene_clear(scene);
  free(scene);
}

typedef struct EdgeRef { int a, b, tri; } EdgeRef;

static int edge_ref_cmp(const void *pa, const void *pb)
{
  const EdgeRef *x = pa, *y = pb;
  if (x->a != y->a) return x->a < y->a ? -1 : 1;
  if (x->b != y->b) return x->b < y->b ? -1 : 1;
  return x->tri < y->tri ? -1 : (x->tri > y->tri);
}

int pg_lite_mesh_build_edges(PGMeshLite *me, const int (*loose_edges)[2], int loose_count)
{
  free(me->edges);
  me->edges = NULL;
  me->totedge = 0;
  const int total = me->tottri * 3 + (loose_count > 0 ? loose_count : 0);
  if (total == 0) return 1;
  EdgeRef *refs = malloc(sizeof(EdgeRef) * (size_t)total);
  if (!refs) return 0;
  int n = 0;
  for (int t = 0; t < me->tottri; t++) {
    for (int k = 0; k < 3; k++) {
      int a = me->tris[t][k], b = me->tris[t][(k + 1) % 3];
      if (a > b) { int s = a; a = b; b = s; }
      refs[n++] = (EdgeRef){a, b, t};
    }
  }
  for (int i = 0; i < loose_count; i++) {
    int a = loose_edges[i][0], b = loose_edges[i][1];
    if (a == b) continue;
    if (a > b) { int s = a; a = b; b = s; }
    refs[n++] = (EdgeRef){a, b, -1};
  }
  qsort(refs, (size_t)n, sizeof(EdgeRef), edge_ref_cmp);
  me->edges = calloc((size_t)(n > 0 ? n : 1), sizeof(PGLiteEdge));
  if (!me->edges) { free(refs); return 0; }
  for (int i = 0; i < n;) {
    int j = i;
    while (j < n && refs[j].a == refs[i].a && refs[j].b == refs[i].b) j++;
    PGLiteEdge *e = &me->edges[me->totedge++];
    e->v[0] = refs[i].a;
    e->v[1] = refs[i].b;
    e->tri[0] = e->tri[1] = -1;
    int faces = 0;
    for (int k = i; k < j; k++) {
      if (refs[k].tri < 0) continue;
      if (faces < 2) e->tri[faces] = refs[k].tri; /* the first two users */
      faces++;
    }
    if (faces == 0) e->flag |= PG_LITE_EDGE_LOOSE;
    if (faces > 2) e->flag |= PG_LITE_EDGE_NON_MANIFOLD;
    if (faces == 2 && me->tri_poly && me->tri_poly[e->tri[0]] == me->tri_poly[e->tri[1]]) {
      e->flag |= PG_LITE_EDGE_POLY_INTERNAL;
    }
    if (faces >= 2 && me->tri_material &&
        me->tri_material[e->tri[0]] != me->tri_material[e->tri[1]]) {
      e->flag |= PG_LITE_EDGE_MATERIAL_BOUNDARY;
    }
    i = j;
  }
  free(refs);
  return 1;
}

/* ---- OBJ ---------------------------------------------------------------------------------- */
typedef struct Growable { void *data; int count, cap; size_t item; } Growable;

static void *grow_push(Growable *g)
{
  if (g->count == g->cap) {
    const int cap = g->cap ? g->cap * 2 : 64;
    void *d = realloc(g->data, g->item * (size_t)cap);
    if (!d) return NULL;
    g->data = d;
    g->cap = cap;
  }
  return (char *)g->data + g->item * (size_t)(g->count++);
}

typedef struct ObjPending {
  char name[64];
  Growable tris, polys, mats, loose; /* int[3], int, int, int[2] (global vertex indices) */
  int totpoly;
} ObjPending;

static void pending_init(ObjPending *p, const char *name)
{
  memset(p, 0, sizeof(*p));
  strncpy(p->name, name, sizeof(p->name) - 1);
  p->tris.item = sizeof(int[3]);
  p->polys.item = sizeof(int);
  p->mats.item = sizeof(int);
  p->loose.item = sizeof(int[2]);
}

static void pending_free(ObjPending *p)
{
  free(p->tris.data); free(p->polys.data); free(p->mats.data); free(p->loose.data);
}

/* OBJ vertex reference "i", "i/t", "i/t/n" or "i//n" -> 0-based index, -1 when invalid. */
static int obj_index(const char *tok, int vcount)
{
  char *end;
  const long i = strtol(tok, &end, 10);
  if (end == tok || i == 0) return -1;
  const long idx = i > 0 ? i - 1 : vcount + i;
  return (idx >= 0 && idx < vcount) ? (int)idx : -1;
}

/* Turns the pending faces into an object with only the vertices it uses. */
static int pending_emit(PGSceneLite *scene, Growable *objects, ObjPending *p, const float (*verts)[3])
{
  if (p->tris.count == 0 && p->loose.count == 0) return 1;
  PGObjectLite *ob = grow_push(objects);
  if (!ob) return 0;
  memset(ob, 0, sizeof(*ob));
  strncpy(ob->name, p->name[0] ? p->name : "Object", sizeof(ob->name) - 1);
  unit_m4(ob->matrix_world);
  ob->line_art_usage = PG_LITE_USAGE_INHERIT;
  (void)scene;
  /* remap global vertex indices to local ones */
  int maxv = -1;
  int (*tris)[3] = p->tris.data;
  int (*loose)[2] = p->loose.data;
  for (int t = 0; t < p->tris.count; t++) for (int k = 0; k < 3; k++) if (tris[t][k] > maxv) maxv = tris[t][k];
  for (int l = 0; l < p->loose.count; l++) for (int k = 0; k < 2; k++) if (loose[l][k] > maxv) maxv = loose[l][k];
  int *map = malloc(sizeof(int) * (size_t)(maxv + 1));
  if (!map) return 0;
  for (int i = 0; i <= maxv; i++) map[i] = -1;
  int nv = 0;
  for (int t = 0; t < p->tris.count; t++) for (int k = 0; k < 3; k++) if (map[tris[t][k]] < 0) map[tris[t][k]] = nv++;
  for (int l = 0; l < p->loose.count; l++) for (int k = 0; k < 2; k++) if (map[loose[l][k]] < 0) map[loose[l][k]] = nv++;
  PGMeshLite *me = &ob->mesh;
  me->verts = malloc(sizeof(float[3]) * (size_t)(nv > 0 ? nv : 1));
  me->tris = malloc(sizeof(int[3]) * (size_t)(p->tris.count > 0 ? p->tris.count : 1));
  me->tri_poly = malloc(sizeof(int) * (size_t)(p->tris.count > 0 ? p->tris.count : 1));
  me->tri_material = malloc(sizeof(int) * (size_t)(p->tris.count > 0 ? p->tris.count : 1));
  int (*lo)[2] = malloc(sizeof(int[2]) * (size_t)(p->loose.count > 0 ? p->loose.count : 1));
  if (!me->verts || !me->tris || !me->tri_poly || !me->tri_material || !lo) { free(map); free(lo); mesh_free(me); objects->count--; return 0; }
  for (int i = 0; i <= maxv; i++) {
    if (map[i] < 0) continue;
    /* Blender's OBJ importer defaults (forward -Z, up Y): (x, y, z) -> (x, -z, y) */
    me->verts[map[i]][0] = verts[i][0];
    me->verts[map[i]][1] = -verts[i][2];
    me->verts[map[i]][2] = verts[i][1];
  }
  me->totvert = nv;
  for (int t = 0; t < p->tris.count; t++) {
    for (int k = 0; k < 3; k++) me->tris[t][k] = map[tris[t][k]];
    me->tri_poly[t] = ((int *)p->polys.data)[t];
    me->tri_material[t] = ((int *)p->mats.data)[t];
  }
  me->tottri = p->tris.count;
  me->totpoly = p->totpoly;
  for (int l = 0; l < p->loose.count; l++) { lo[l][0] = map[loose[l][0]]; lo[l][1] = map[loose[l][1]]; }
  const int ok = pg_lite_mesh_build_edges(me, (const int (*)[2])lo, p->loose.count);
  free(lo);
  free(map);
  if (!ok) { mesh_free(me); objects->count--; return 0; }
  return 1;
}

int pg_lite_load_obj(PGSceneLite *scene, const char *text, int length)
{
  if (!scene || !text || length <= 0) return 0;
  Growable verts = {NULL, 0, 0, sizeof(float[3])};
  Growable objects = {NULL, 0, 0, sizeof(PGObjectLite)};
  char matnames[64][64];
  int matcount = 0, curmat = 0, ok = 1;
  ObjPending p;
  pending_init(&p, "Object");
  char *buf = malloc((size_t)length + 1);
  if (!buf) return 0;
  memcpy(buf, text, (size_t)length);
  buf[length] = '\0';
  char *save = NULL;
  for (char *line = strtok_r(buf, "\n", &save); line && ok; line = strtok_r(NULL, "\n", &save)) {
    char *cr = strchr(line, '\r');
    if (cr) *cr = '\0';
    while (*line == ' ' || *line == '\t') line++;
    if (line[0] == 'v' && (line[1] == ' ' || line[1] == '\t')) {
      float *v = grow_push(&verts);
      if (!v || sscanf(line + 2, "%f %f %f", &v[0], &v[1], &v[2]) != 3) { ok = 0; break; }
      if (!isfinite(v[0]) || !isfinite(v[1]) || !isfinite(v[2])) { ok = 0; break; }
    }
    else if ((line[0] == 'o' || line[0] == 'g') && (line[1] == ' ' || line[1] == '\t' || line[1] == '\0')) {
      char name[64] = "";
      if (line[1]) sscanf(line + 2, "%63s", name);
      if (p.tris.count || p.loose.count) {
        if (!pending_emit(scene, &objects, &p, verts.data)) { ok = 0; break; }
        pending_free(&p);
        pending_init(&p, name);
      }
      else if (name[0]) {
        strncpy(p.name, name, sizeof(p.name) - 1);
      }
    }
    else if (strncmp(line, "usemtl", 6) == 0 && (line[6] == ' ' || line[6] == '\t')) {
      char name[64] = "";
      sscanf(line + 7, "%63s", name);
      curmat = -1;
      for (int i = 0; i < matcount; i++) if (strcmp(matnames[i], name) == 0) curmat = i;
      if (curmat < 0) {
        if (matcount < 64) { strcpy(matnames[matcount], name); curmat = matcount++; }
        else curmat = 63;
      }
    }
    else if ((line[0] == 'f' || line[0] == 'l') && (line[1] == ' ' || line[1] == '\t')) {
      int idx[256], n = 0;
      char *ts = NULL;
      for (char *tok = strtok_r(line + 2, " \t", &ts); tok; tok = strtok_r(NULL, " \t", &ts)) {
        if (n == 256) { ok = 0; break; }
        if ((idx[n++] = obj_index(tok, verts.count)) < 0) { ok = 0; break; }
      }
      if (!ok) break;
      if (line[0] == 'l') {
        for (int i = 0; i + 1 < n; i++) {
          int *e = grow_push(&p.loose);
          if (!e) { ok = 0; break; }
          e[0] = idx[i]; e[1] = idx[i + 1];
        }
      }
      else {
        if (n < 3) { ok = 0; break; }
        for (int i = 1; i + 1 < n; i++) {
          int *t = grow_push(&p.tris);
          int *pi = grow_push(&p.polys);
          int *mi = grow_push(&p.mats);
          if (!t || !pi || !mi) { ok = 0; break; }
          t[0] = idx[0]; t[1] = idx[i]; t[2] = idx[i + 1];
          *pi = p.totpoly;
          *mi = curmat;
        }
        p.totpoly++;
      }
    }
    /* vt, vn, s, mtllib, comments: not needed by Line Art */
  }
  if (ok) ok = pending_emit(scene, &objects, &p, verts.data);
  pending_free(&p);
  free(buf);
  free(verts.data);
  PGObjectLite *obs = objects.data;
  if (!ok || objects.count == 0) {
    for (int i = 0; i < objects.count; i++) mesh_free(&obs[i].mesh);
    free(obs);
    return 0;
  }
  PGObjectLite *all = realloc(scene->objects, sizeof(PGObjectLite) * (size_t)(scene->totobject + objects.count));
  if (!all) {
    for (int i = 0; i < objects.count; i++) mesh_free(&obs[i].mesh);
    free(obs);
    return 0;
  }
  memcpy(all + scene->totobject, obs, sizeof(PGObjectLite) * (size_t)objects.count);
  scene->objects = all;
  scene->totobject += objects.count;
  const int added = objects.count;
  free(obs);
  return added;
}

/* ---- camera ------------------------------------------------------------------------------- */
void pg_lite_camera_orbit(PGCameraLite *cam, const float target[3], float yaw, float pitch, float distance)
{
  const float d = distance > 1e-4f ? distance : 1e-4f;
  const float p[3] = {target[0] + d * cosf(pitch) * sinf(yaw), target[1] - d * cosf(pitch) * cosf(yaw),
                      target[2] + d * sinf(pitch)};
  float z[3] = {p[0] - target[0], p[1] - target[1], p[2] - target[2]}; /* camera looks down -Z */
  normalize_v3(z);
  const float up[3] = {0.0f, 0.0f, 1.0f};
  float x[3], y[3];
  cross_v3(x, up, z);
  if (x[0] * x[0] + x[1] * x[1] + x[2] * x[2] < 1e-12f) { x[0] = 1.0f; x[1] = x[2] = 0.0f; } /* looking straight down/up */
  normalize_v3(x);
  cross_v3(y, z, x);
  unit_m4(cam->matrix_world);
  for (int i = 0; i < 3; i++) {
    cam->matrix_world[0][i] = x[i];
    cam->matrix_world[1][i] = y[i];
    cam->matrix_world[2][i] = z[i];
    cam->matrix_world[3][i] = p[i];
  }
}

void pg_lite_camera_default(PGCameraLite *cam)
{
  memset(cam, 0, sizeof(*cam));
  cam->type = CAM_PERSP;
  cam->lens = 50.0f;
  cam->ortho_scale = 6.0f;
  cam->sensor_x = 36.0f;
  cam->sensor_y = 24.0f;
  cam->sensor_fit = CAMERA_SENSOR_FIT_AUTO;
  cam->clip_start = 0.1f;
  cam->clip_end = 100.0f;
  const float target[3] = {0.0f, 0.0f, 0.0f};
  pg_lite_camera_orbit(cam, target, 0.8f, 0.45f, 12.0f);
}

void pg_lite_view_projection(const PGCameraLite *cam, int width, int height, float overscan, double r[4][4])
{
  double proj[4][4];
  float cam_obmat[4][4], inv[4][4];
  const int w = width > 0 ? width : 1, h = height > 0 ? height : 1;
  /* lineart_main_init(): line art expects no scaling on cameras */
  memcpy(cam_obmat, cam->matrix_world, sizeof(cam_obmat));
  normalize_v3(cam_obmat[0]);
  normalize_v3(cam_obmat[1]);
  normalize_v3(cam_obmat[2]);
  /* lineart_main_load_geometries() */
  float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
  int fit = BKE_camera_sensor_fit(cam->sensor_fit, (float)w, (float)h);
  double asp = ((double)w / (double)h);
  if (cam->type == CAM_PERSP) {
    if (fit == CAMERA_SENSOR_FIT_VERT && asp > 1) {
      sensor *= asp;
    }
    if (fit == CAMERA_SENSOR_FIT_HOR && asp < 1) {
      sensor /= asp;
    }
    const double fov = focallength_to_fov(cam->lens / (1 + overscan), sensor);
    lineart_matrix_perspective_44d(proj, fov, asp, cam->clip_start, cam->clip_end);
  }
  else {
    const double hw = cam->ortho_scale / 2;
    lineart_matrix_ortho_44d(proj, -hw, hw, -hw / asp, hw / asp, cam->clip_start, cam->clip_end);
  }
  if (!invert_m4(inv, cam_obmat)) unit_m4(inv);
  /* mul_m4db_m4db_m4fl(result, proj, inv) */
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      double s = 0.0;
      for (int k = 0; k < 4; k++) s += proj[k][j] * (double)inv[i][k];
      r[i][j] = s;
    }
  }
}

void pg_lite_camera_shift(const PGCameraLite *cam, int width, int height, double *r_x, double *r_y)
{
  const int w = width > 0 ? width : 1, h = height > 0 ? height : 1;
  double asp = ((double)w / (double)h);
  int fit = BKE_camera_sensor_fit(cam->sensor_fit, (float)w, (float)h);
  *r_x = fit == CAMERA_SENSOR_FIT_HOR ? cam->shift_x : cam->shift_x / asp;
  *r_y = fit == CAMERA_SENSOR_FIT_VERT ? cam->shift_y : cam->shift_y * asp;
}

int pg_lite_project(const double vp[4][4], double shift_x, double shift_y, const float co[3], double r_fb[2])
{
  /* mul_v4_m4v3_db */
  double out[4];
  for (int i = 0; i < 4; i++) {
    out[i] = vp[0][i] * co[0] + vp[1][i] * co[1] + vp[2][i] * co[2] + vp[3][i];
  }
  if (out[3] <= 0.0) return 0;
  r_fb[0] = out[0] / out[3] - shift_x * 2;
  r_fb[1] = out[1] / out[3] - shift_y * 2;
  return 1;
}

void pg_lite_stats(const PGSceneLite *scene, int r[5])
{
  memset(r, 0, sizeof(int) * 5);
  if (!scene) return;
  r[0] = scene->totobject;
  for (int i = 0; i < scene->totobject; i++) {
    const PGMeshLite *me = &scene->objects[i].mesh;
    r[1] += me->totvert;
    r[2] += me->tottri;
    r[3] += me->totedge;
    for (int e = 0; e < me->totedge; e++) if (me->edges[e].flag & PG_LITE_EDGE_LOOSE) r[4]++;
  }
}
