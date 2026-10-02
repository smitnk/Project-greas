/* Host test for the live modifier stack (project_grease_modifier_stack.c). It links the real pinned
 * Blender 3.6.23 stroke code (gpencil_geom_legacy.cc, gpencil_legacy.c, rand.cc, ...), see
 * tools/run_native_modifier_stack_tests.sh, so Smooth/Simplify/Subdivide/Offset/Noise run their
 * verbatim deformStroke() bodies against the real BKE_gpencil_stroke_* functions. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "MEM_guardedalloc.h"
#include "project_grease_modifier_stack.h"

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static bool near(float a, float b, float eps = 1e-4f)
{
  return std::fabs(a - b) <= eps * std::fmax(1.0f, std::fabs(b));
}

struct Doc {
  bGPdata *gpd;
  bGPDlayer *gpl;
  bGPDframe *f1;
  bGPDframe *f5;
};

static void add_stroke(bGPDframe *gpf, int n, float x0, float y0, float wobble)
{
  bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, 0, n, 10, false);
  for (int i = 0; i < n; i++) {
    bGPDspoint *pt = &gps->points[i];
    pt->x = x0 + 12.0f * i;
    pt->y = y0 + wobble * std::sin(0.9f * i) + 3.0f * (i % 2);
    pt->z = 0.0f;
    pt->pressure = 0.4f + 0.05f * i;
    pt->strength = 0.8f;
    pt->time = 0.01f * i;
    pt->vert_color[0] = 0.2f;
    pt->vert_color[1] = 0.4f;
    pt->vert_color[2] = 0.6f;
    pt->vert_color[3] = 0.5f;
  }
  (void)0;
}

static Doc make_doc()
{
  Doc d;
  d.gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "test gpd"));
  d.gpl = BKE_gpencil_layer_addnew(d.gpd, "Layer", true, false);
  d.f1 = BKE_gpencil_frame_addnew(d.gpl, 1);
  d.f5 = BKE_gpencil_frame_addnew(d.gpl, 5);
  add_stroke(d.f1, 9, 10.0f, 50.0f, 20.0f);
  add_stroke(d.f1, 7, 30.0f, 120.0f, 8.0f);
  add_stroke(d.f5, 6, 5.0f, 70.0f, 15.0f);
  return d;
}

static void free_doc(Doc &d)
{
  BKE_gpencil_free_layers(&d.gpd->layers);
  MEM_freeN(d.gpd);
}

struct Pt {
  float v[11];
};
static std::vector<std::vector<Pt>> snapshot(const bGPDframe *gpf)
{
  std::vector<std::vector<Pt>> out;
  for (const bGPDstroke *gps = (const bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) {
    std::vector<Pt> s;
    for (int i = 0; i < gps->totpoints; i++) {
      const bGPDspoint *p = &gps->points[i];
      Pt q = {{p->x, p->y, p->z, p->pressure, p->strength, p->time, p->vert_color[0],
               p->vert_color[1], p->vert_color[2], p->vert_color[3], (float)gps->thickness}};
      s.push_back(q);
    }
    out.push_back(s);
  }
  return out;
}
static bool same(const std::vector<std::vector<Pt>> &a, const std::vector<std::vector<Pt>> &b,
                 float eps = 0.0f)
{
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i].size() != b[i].size()) {
      return false;
    }
    for (size_t j = 0; j < a[i].size(); j++) {
      for (int k = 0; k < 11; k++) {
        if (std::fabs(a[i][j].v[k] - b[i][j].v[k]) > eps) {
          return false;
        }
      }
    }
  }
  return true;
}

static PGModEntry entry(int type)
{
  PGModEntry e;
  memset(&e, 0, sizeof(e));
  pg_mod_entry_init(&e, type);
  return e;
}

static std::vector<std::vector<Pt>> eval(Doc &d, const PGModEntry *e, int n, int cfra)
{
  bGPDframe ev;
  pg_mod_eval_frame(d.gpd, d.gpl, d.f1, e, n, cfra, &ev);
  auto s = snapshot(&ev);
  pg_mod_eval_free(&ev);
  return s;
}

/* Entries tuned so that every type visibly changes the test strokes. */
static PGModEntry tuned(int type)
{
  PGModEntry e = entry(type);
  float *p = e.params;
  switch (type) {
    case PG_MOD_THICKNESS: p[PG_P_THICK_THICKNESS] = 40; p[PG_P_THICK_FACTOR] = 1.5f; break;
    case PG_MOD_OPACITY: p[PG_P_OPACITY_FACTOR] = 0.5f; break;
    case PG_MOD_TINT: p[PG_P_TINT_FACTOR] = 0.7f; p[PG_P_TINT_R] = 1; p[PG_P_TINT_G] = 0; p[PG_P_TINT_B] = 0; break;
    case PG_MOD_COLOR: p[PG_P_COLOR_H] = 0.2f; p[PG_P_COLOR_S] = 0.5f; p[PG_P_COLOR_V] = 1.2f; break;
    case PG_MOD_LENGTH: p[PG_P_LENGTH_START] = 0.2f; p[PG_P_LENGTH_END] = 0.1f; break;
    case PG_MOD_SMOOTH: p[PG_P_SMOOTH_FACTOR] = 1.0f; p[PG_P_SMOOTH_STEP] = 2; break;
    case PG_MOD_SIMPLIFY: p[PG_P_SIMPLIFY_STEP] = 1; break;
    case PG_MOD_SUBDIV: p[PG_P_SUBDIV_LEVEL] = 1; break;
    case PG_MOD_OFFSET: p[PG_P_OFFSET_MODE] = 0; p[PG_P_OFFSET_RND_OFFSET] = 30; p[PG_P_OFFSET_RND_OFFSET + 1] = 30;
                        p[PG_P_OFFSET_RND_ROT + 2] = 0.3f; p[PG_P_OFFSET_SEED] = 7; break;
    case PG_MOD_NOISE: p[PG_P_NOISE_FACTOR] = 1.0f; p[PG_P_NOISE_SEED] = 3; break;
  }
  return e;
}

static void test_type_info()
{
  for (int t = PG_MOD_THICKNESS; t <= PG_MOD_TYPE_LAST; t++) {
    CHECK(pg_mod_valid_type(t));
    CHECK(pg_mod_param_count(t) > 0 && pg_mod_param_count(t) <= PG_MOD_MAX_PARAMS);
    CHECK(pg_mod_name(t) != nullptr && pg_mod_name(t)[0] != 0);
  }
  CHECK(!pg_mod_valid_type(0) && !pg_mod_valid_type(PG_MOD_TYPE_LAST + 1) && !pg_mod_valid_type(-1));
  float p[PG_MOD_MAX_PARAMS];
  pg_mod_defaults(PG_MOD_NOISE, p);
  CHECK(near(p[PG_P_NOISE_FACTOR], 0.5f) && p[PG_P_NOISE_STEP] == 4 && p[PG_P_NOISE_SEED] == 1);
  p[PG_P_NOISE_FACTOR] = NAN;
  p[PG_P_NOISE_MODE] = 99;
  pg_mod_sanitize(PG_MOD_NOISE, p);
  CHECK(std::isfinite(p[PG_P_NOISE_FACTOR]) && (p[PG_P_NOISE_MODE] == 0 || p[PG_P_NOISE_MODE] == 1));
}

static void test_originals_untouched_and_changed()
{
  for (int t = PG_MOD_THICKNESS; t <= PG_MOD_TYPE_LAST; t++) {
    Doc d = make_doc();
    const auto before = snapshot(d.f1);
    const PGModEntry e = tuned(t);
    const auto live = eval(d, &e, 1, 1);
    CHECK(same(snapshot(d.f1), before)); /* originals untouched */
    if (same(live, before)) {
      printf("type %d did not change the strokes\n", t);
    }
    CHECK(!same(live, before));
    free_doc(d);
  }
}

static void test_disabled_and_empty()
{
  Doc d = make_doc();
  const auto before = snapshot(d.f1);
  PGModEntry e = tuned(PG_MOD_OFFSET);
  e.enabled = 0;
  CHECK(same(eval(d, &e, 1, 1), before));
  CHECK(same(eval(d, nullptr, 0, 1), before)); /* empty stack = copy of the originals */
  e.enabled = 1;
  CHECK(!same(eval(d, &e, 1, 1), before));
  free_doc(d);
}

static void test_order()
{
  Doc d = make_doc();
  PGModEntry sub = tuned(PG_MOD_SUBDIV);
  PGModEntry simp = tuned(PG_MOD_SIMPLIFY);
  PGModEntry ab[2] = {sub, simp};
  PGModEntry ba[2] = {simp, sub};
  const auto r_ab = eval(d, ab, 2, 1);
  const auto r_ba = eval(d, ba, 2, 1);
  CHECK(!same(r_ab, r_ba));
  /* Evaluating [A, B] equals evaluating [A] and then B on the result: baking A, then evaluating B. */
  const PGModEntry only_sub[1] = {sub};
  pg_mod_apply(d.gpd, d.gpl, &only_sub[0], 1);
  const PGModEntry only_simp[1] = {simp};
  CHECK(same(eval(d, only_simp, 1, 1), r_ab));
  free_doc(d);
}

static void test_apply_equals_live()
{
  for (int t = PG_MOD_THICKNESS; t <= PG_MOD_TYPE_LAST; t++) {
    Doc d = make_doc();
    const PGModEntry e = tuned(t);
    const auto live = eval(d, &e, 1, 1);
    CHECK(pg_mod_apply(d.gpd, d.gpl, &e, 1) == 1);
    const auto baked = snapshot(d.f1);
    if (!same(live, baked)) {
      printf("apply != live for type %d\n", t);
    }
    CHECK(same(live, baked));
    /* "Apply" bakes every frame of the layer. */
    CHECK(d.f5 != nullptr);
    free_doc(d);
  }
}

static void test_apply_all_frames()
{
  Doc d = make_doc();
  const PGModEntry e = tuned(PG_MOD_OFFSET);
  const auto f5_before = snapshot(d.f5);
  pg_mod_apply(d.gpd, d.gpl, &e, 1);
  CHECK(!same(snapshot(d.f5), f5_before));
  free_doc(d);
}

/* Thickness with a vertex group: weights live on the strokes (dvert), so the evaluated copy and
 * Apply on the original agree, and points outside the group are skipped. */
static Doc make_pair_doc(int strokes);
static void test_thickness_vertex_group()
{
  Doc d = make_pair_doc(1);
  bGPDstroke *s = static_cast<bGPDstroke *>(d.f1->strokes.first);
  s->dvert = static_cast<MDeformVert *>(MEM_callocN(sizeof(MDeformVert) * 2, "test dvert"));
  for (int i = 0; i < 2; i++) {
    s->points[i].pressure = 1.0f;
  }
  s->dvert[0].dw = static_cast<MDeformWeight *>(MEM_callocN(sizeof(MDeformWeight), "test dw"));
  s->dvert[0].totweight = 1;
  s->dvert[0].dw[0].def_nr = 2;
  s->dvert[0].dw[0].weight = 0.5f; /* point 1 has no weight in group 2 */
  PGModEntry e = entry(PG_MOD_THICKNESS);
  e.params[PG_P_THICK_NORMALIZE] = 0;
  e.params[PG_P_THICK_FACTOR] = 3.0f;
  e.params[PG_P_THICK_USE_VGROUP] = 1;
  e.params[PG_P_THICK_VGROUP] = 2;
  auto live = eval(d, &e, 1, 1);
  CHECK(near(live[0][0].v[3], 2.0f)); /* interp(3, 1, 0.5) */
  CHECK(near(live[0][1].v[3], 1.0f)); /* outside the group: untouched */
  e.params[PG_P_THICK_INVERT_VGROUP] = 1;
  auto inv = eval(d, &e, 1, 1);
  CHECK(near(inv[0][0].v[3], 1.0f) && near(inv[0][1].v[3], 3.0f)); /* inverted: the other points */
  e.params[PG_P_THICK_INVERT_VGROUP] = 0;
  e.params[PG_P_THICK_VGROUP] = 7; /* a group nobody is in: nothing changes */
  auto none = eval(d, &e, 1, 1);
  CHECK(near(none[0][0].v[3], 1.0f) && near(none[0][1].v[3], 1.0f));
  e.params[PG_P_THICK_VGROUP] = 2;
  e.params[PG_P_THICK_USE_VGROUP] = 0; /* group off: every point */
  auto all = eval(d, &e, 1, 1);
  CHECK(near(all[0][0].v[3], 3.0f) && near(all[0][1].v[3], 3.0f));
  e.params[PG_P_THICK_USE_VGROUP] = 1;
  const auto before = eval(d, &e, 1, 1);
  CHECK(pg_mod_apply(d.gpd, d.gpl, &e, 1) == 1);
  CHECK(same(before, snapshot(d.f1)));
  /* the weights are freed with the stroke by free_doc (BKE_gpencil_free_stroke) */
  free_doc(d);
}

static void test_offset_noise_determinism()
{
  Doc d = make_doc();
  const PGModEntry off = tuned(PG_MOD_OFFSET);
  CHECK(same(eval(d, &off, 1, 1), eval(d, &off, 1, 1)));
  PGModEntry off2 = off;
  off2.params[PG_P_OFFSET_SEED] = 8;
  CHECK(!same(eval(d, &off, 1, 1), eval(d, &off2, 1, 1)));

  const PGModEntry noise = tuned(PG_MOD_NOISE);
  CHECK(same(eval(d, &noise, 1, 4), eval(d, &noise, 1, 4)));
  PGModEntry n2 = noise;
  n2.params[PG_P_NOISE_SEED] = 4;
  CHECK(!same(eval(d, &noise, 1, 4), eval(d, &n2, 1, 4)));
  /* Noise depends on time: another frame number (past the step) gives other values. */
  CHECK(!same(eval(d, &noise, 1, 4), eval(d, &noise, 1, 20)));
  /* Within one step (step 4) the frame number does not change the noise... */
  PGModEntry stepped = noise;
  stepped.params[PG_P_NOISE_USE_RANDOM] = 1;
  stepped.params[PG_P_NOISE_STEP] = 4;
  CHECK(same(eval(d, &stepped, 1, 8), eval(d, &stepped, 1, 8)));
  /* factor 0 leaves the strokes as they are. */
  PGModEntry zero = noise;
  zero.params[PG_P_NOISE_FACTOR] = 0.0f;
  zero.params[PG_P_NOISE_STRENGTH] = 0.0f;
  zero.params[PG_P_NOISE_THICKNESS] = 0.0f;
  zero.params[PG_P_NOISE_UVS] = 0.0f;
  CHECK(same(eval(d, &zero, 1, 4), snapshot(d.f1), 1e-3f));
  free_doc(d);
}

static void test_offset_mapping()
{
  Doc d = make_doc();
  PGModEntry off = entry(PG_MOD_OFFSET);
  off.params[PG_P_OFFSET_MODE] = 3; /* GP_OFFSET_STROKE, not random */
  off.params[PG_P_OFFSET_LOC] = 10.0f;
  off.params[PG_P_OFFSET_LOC + 1] = 5.0f;
  const auto before = snapshot(d.f1);
  const auto after = eval(d, &off, 1, 1);
  /* Canvas pixels: +X to the right, +Y up on screen = smaller canvas y. */
  CHECK(near(after[0][0].v[0], before[0][0].v[0] + 10.0f, 1e-3f));
  CHECK(near(after[0][0].v[1], before[0][0].v[1] - 5.0f, 1e-3f));
  free_doc(d);
}


/* Golden values from tests/gen_modifier_golden.py (independent reference written from the pinned
 * MOD_gpencil_legacy_offset.c / MOD_gpencil_legacy_noise.c and the documented canvas mapping). */
static Doc make_pair_doc(int strokes)
{
  Doc d;
  d.gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "test gpd"));
  d.gpl = BKE_gpencil_layer_addnew(d.gpd, "Layer", true, false);
  d.f1 = BKE_gpencil_frame_addnew(d.gpl, 1);
  d.f5 = nullptr;
  for (int k = 0; k < strokes; k++) {
    bGPDstroke *gps = BKE_gpencil_stroke_add(d.f1, 0, 2, 10, false);
    gps->points[0].x = 100.0f; gps->points[0].y = 200.0f;
    gps->points[1].x = 160.0f; gps->points[1].y = 260.0f;
    for (int i = 0; i < 2; i++) {
      gps->points[i].z = 0.0f; gps->points[i].pressure = 1.0f; gps->points[i].strength = 1.0f;
    }
  }
  return d;
}

static void test_golden_offset_noise()
{
  Doc d = make_pair_doc(3);
  PGModEntry off = entry(PG_MOD_OFFSET);
  off.params[PG_P_OFFSET_MODE] = 0;
  off.params[PG_P_OFFSET_RND_OFFSET] = 30; off.params[PG_P_OFFSET_RND_OFFSET + 1] = 30;
  off.params[PG_P_OFFSET_SEED] = 7;
  auto r = eval(d, &off, 1, 1);
  const float ex[3][2] = {{105.5078125f, 194.4921875f}, {127.83203125f, 200.5859375f},
                          {122.6171875f, 185.64453125f}};
  for (int k = 0; k < 3; k++) {
    CHECK(near(r[k][0].v[0], ex[k][0], 1e-3f) && near(r[k][0].v[1], ex[k][1], 1e-3f));
  }

  Doc n = make_pair_doc(2);
  PGModEntry noise = entry(PG_MOD_NOISE);
  float *q = noise.params;
  q[PG_P_NOISE_FACTOR] = 1.0f; q[PG_P_NOISE_STRENGTH] = 0; q[PG_P_NOISE_THICKNESS] = 0;
  q[PG_P_NOISE_UVS] = 0; q[PG_P_NOISE_SCALE] = 1.0f; q[PG_P_NOISE_OFFSET] = 0;
  q[PG_P_NOISE_SEED] = 3; q[PG_P_NOISE_STEP] = 4; q[PG_P_NOISE_USE_RANDOM] = 1;
  q[PG_P_NOISE_MODE] = 0;
  auto a = eval(n, &noise, 1, 4);
  CHECK(near(a[0][0].v[0], 100.05317890518579f, 1e-3f) && near(a[0][0].v[1], 199.9468210948142f, 1e-3f));
  CHECK(near(a[0][1].v[0], 156.05677661884022f, 1e-3f) && near(a[0][1].v[1], 263.9432233811598f, 1e-3f));
  CHECK(near(a[0][0].v[2], -0.10635781037158197f, 1e-3f) && near(a[0][1].v[2], 7.886446762319532f, 1e-3f));
  CHECK(near(a[1][0].v[0], 97.83772983024063f, 1e-3f) && near(a[1][0].v[1], 202.16227016975935f, 1e-3f));
  CHECK(near(a[1][1].v[0], 162.49617754235462f, 1e-3f) && near(a[1][1].v[1], 257.5038224576454f, 1e-3f));
  /* Time dependence: cfra 20 is another step (20/4 = 5 vs 4/4 = 1). */
  auto b = eval(n, &noise, 1, 20);
  CHECK(near(b[0][0].v[0], 99.97062046894686f, 1e-3f) && near(b[0][0].v[1], 200.02937953105314f, 1e-3f));
  CHECK(near(b[0][1].v[0], 163.2336605418309f, 1e-3f) && near(b[0][1].v[1], 256.7663394581691f, 1e-3f));
  /* Same step, other frame (5/4 == 4/4): identical to cfra 4. */
  CHECK(same(eval(n, &noise, 1, 7), a));
  free_doc(d);
  free_doc(n);
}

int main()
{
  test_type_info();
  test_originals_untouched_and_changed();
  test_disabled_and_empty();
  test_order();
  test_apply_equals_live();
  test_apply_all_frames();
  test_thickness_vertex_group();
  test_offset_noise_determinism();
  test_offset_mapping();
  test_golden_offset_noise();
  if (failures) {
    printf("%d FAILURES\n", failures);
    return 1;
  }
  printf("modifier stack tests passed\n");
  return 0;
}
