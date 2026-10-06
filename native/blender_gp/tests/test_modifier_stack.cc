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
#include "DNA_object_types.h"
#include "MEM_guardedalloc.h"
#include "project_grease_blender_interp.h"
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
  float v[13]; /* ..., thickness, uv_fac, uv_rot */
};
static std::vector<std::vector<Pt>> snapshot(const bGPDframe *gpf)
{
  std::vector<std::vector<Pt>> out;
  for (const bGPDstroke *gps = (const bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) {
    std::vector<Pt> s;
    for (int i = 0; i < gps->totpoints; i++) {
      const bGPDspoint *p = &gps->points[i];
      Pt q = {{p->x, p->y, p->z, p->pressure, p->strength, p->time, p->vert_color[0],
               p->vert_color[1], p->vert_color[2], p->vert_color[3], (float)gps->thickness, p->uv_fac, p->uv_rot}};
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
    case PG_MOD_HOOK: p[PG_P_HOOK_CX] = 50; p[PG_P_HOOK_CY] = 80; p[PG_P_HOOK_DX] = 20; p[PG_P_HOOK_DY] = -10;
                      p[PG_P_HOOK_STRENGTH] = 1.0f; break;
    case PG_MOD_LATTICE: p[PG_P_LATTICE_X0] = 0; p[PG_P_LATTICE_Y0] = 0; p[PG_P_LATTICE_X1] = 200;
                         p[PG_P_LATTICE_Y1] = 200; p[PG_P_LATTICE_OFFSETS + 8] = 15; break; /* centre node of 3x3 */
    case PG_MOD_TEXTURE: p[PG_P_TEXTURE_UV_SCALE] = 2.0f; p[PG_P_TEXTURE_ALIGN_ROT] = 0.3f; break;
  }
  return e;
}
/* Types whose effect is not a change of the evaluated points at cfra 1 (checked on their own). */
static bool point_changing(int t)
{
  /* Texture Mapping only changes uv data (test_texture_modifier) */
  return t != PG_MOD_TIME && t != PG_MOD_WEIGHT_PROX && t != PG_MOD_WEIGHT_ANGLE && t != PG_MOD_TEXTURE;
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
    if (!point_changing(t)) continue;
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
    if (!point_changing(t)) continue;
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

/* ---- batch 21 ---- */
static float group_weight(const bGPDstroke *s, int i, int g)
{
  if (!s->dvert) return -1.0f;
  for (int k = 0; k < s->dvert[i].totweight; k++)
    if ((int)s->dvert[i].dw[k].def_nr == g) return s->dvert[i].dw[k].weight;
  return -1.0f;
}

static void test_batch21()
{
  /* Time Offset: normal mode offset 1, range 1..5 loop; ping-pong; fixed */
  PGModEntry t = entry(PG_MOD_TIME);
  CHECK(pg_mod_time_frame(&t, 1, 3) == 4);
  t.params[PG_P_TIME_USE_RANGE] = 1; t.params[PG_P_TIME_SFRA] = 1; t.params[PG_P_TIME_EFRA] = 5;
  CHECK(pg_mod_time_frame(&t, 1, 5) == 1); /* 6 loops to 1 */
  t.params[PG_P_TIME_MODE] = 2; t.params[PG_P_TIME_OFFSET] = 7;
  CHECK(pg_mod_time_frame(&t, 1, 3) == 7);
  t.enabled = 0;
  CHECK(pg_mod_time_frame(&t, 1, 3) == 3);

  /* Build: at the keyframe nothing is visible, half way about half, after the length all */
  {
    Doc d = make_doc();
    PGModEntry b = entry(PG_MOD_BUILD);
    b.params[PG_P_BUILD_LENGTH] = 10;
    auto none = eval(d, &b, 1, 1);
    CHECK(none.empty());
    /* Blender ends the build at the next key (frame 5) when it comes before start + length */
    auto half = eval(d, &b, 1, 3);
    size_t pts = 0; for (auto &s : half) pts += s.size();
    CHECK(pts == 9); /* last_visible round(0.5 * 16) = 8 covers stroke 0 (indices 0..8), stroke 1 starts at 9 */
    CHECK(same(eval(d, &b, 1, 11), snapshot(d.f1)));
    b.params[PG_P_BUILD_MODE] = 1; /* concurrent: both strokes grow */
    auto conc = eval(d, &b, 1, 3);
    /* start alignment: round(0.5 * 9) = 5 and round(0.5 * 9 / 7 * 7) = 5 */
    CHECK(conc.size() == 2 && conc[0].size() == 5 && conc[1].size() == 5);
    free_doc(d);
  }
  /* Vertex weight proximity / angle write the target group on the copies only */
  {
    Doc d = make_pair_doc(1);
    bDeformGroup *g = static_cast<bDeformGroup *>(MEM_callocN(sizeof(bDeformGroup), "g"));
    BLI_addtail(&d.gpd->vertex_group_names, g);
    PGModEntry w = entry(PG_MOD_WEIGHT_PROX);
    w.params[PG_P_WPROX_X] = 100; w.params[PG_P_WPROX_Y] = 200;
    w.params[PG_P_WPROX_DIST_START] = 0; w.params[PG_P_WPROX_DIST_END] = 100;
    bGPDframe ev;
    pg_mod_eval_frame(d.gpd, d.gpl, d.f1, &w, 1, 1, &ev);
    const bGPDstroke *s = (const bGPDstroke *)ev.strokes.first;
    CHECK(near(group_weight(s, 0, 0), 0.0f) && near(group_weight(s, 1, 0), 84.8528f / 100.0f, 1e-3f));
    pg_mod_eval_free(&ev);
    CHECK(((bGPDstroke *)d.f1->strokes.first)->dvert == nullptr); /* original untouched */
    PGModEntry a = entry(PG_MOD_WEIGHT_ANGLE);
    a.params[PG_P_WANGLE_ANGLE] = -0.78539816f; /* the 45 deg segment (canvas y down) */
    pg_mod_eval_frame(d.gpd, d.gpl, d.f1, &a, 1, 1, &ev);
    s = (const bGPDstroke *)ev.strokes.first;
    CHECK(near(group_weight(s, 1, 0), 1.0f, 1e-3f));
    pg_mod_eval_free(&ev);
    /* a later Thickness entry with the group sees the written weights */
    PGModEntry stack[2] = {w, entry(PG_MOD_THICKNESS)};
    stack[1].params[PG_P_THICK_FACTOR] = 3; stack[1].params[PG_P_THICK_USE_VGROUP] = 1; stack[1].params[PG_P_THICK_VGROUP] = 0;
    auto r = eval(d, stack, 2, 1);
    CHECK(near(r[0][0].v[3], 1.0f) && r[0][1].v[3] > 2.0f);
    BLI_freelistN(&d.gpd->vertex_group_names);
    free_doc(d);
  }
  /* Filters: a material filter that matches nothing leaves the strokes; invert selects all */
  {
    Doc d = make_doc();
    PGModEntry h = tuned(PG_MOD_HOOK);
    h.params[PG_P_FILTER_BASE + PG_P_FILTER_MATERIAL] = 3; /* slot 2 */
    CHECK(same(eval(d, &h, 1, 1), snapshot(d.f1)));
    h.params[PG_P_FILTER_BASE + PG_P_FILTER_INVERT_MATERIAL] = 1;
    const PGModEntry plain = tuned(PG_MOD_HOOK);
    CHECK(same(eval(d, &h, 1, 1), eval(d, &plain, 1, 1)));
    /* custom curve 0 -> 0: the first point keeps its place, the last moves fully */
    PGModEntry o = entry(PG_MOD_OFFSET);
    o.params[PG_P_OFFSET_MODE] = 3; o.params[PG_P_OFFSET_LOC] = 10.0f;
    float *c = &o.params[PG_P_CURVE_BASE];
    c[PG_P_CURVE_USE] = 1; c[PG_P_CURVE_COUNT_PTS] = 2;
    c[PG_P_CURVE_XY] = 0; c[PG_P_CURVE_XY + 1] = 0; c[PG_P_CURVE_XY + 2] = 1; c[PG_P_CURVE_XY + 3] = 1;
    auto before = snapshot(d.f1);
    auto cur = eval(d, &o, 1, 1);
    CHECK(near(cur[0][0].v[0], before[0][0].v[0], 1e-3f));
    CHECK(near(cur[0].back().v[0], before[0].back().v[0] + 10.0f, 1e-3f));
    free_doc(d);
  }
  /* Generators add strokes on the copy */
  {
    Doc d = make_doc();
    const short flag0 = ((bGPDstroke *)d.f1->strokes.first)->flag;
    PGModEntry arr = entry(PG_MOD_ARRAY); arr.params[PG_P_ARRAY_COUNT_N] = 3;
    CHECK(eval(d, &arr, 1, 1).size() == 6);
    PGModEntry mir = entry(PG_MOD_MIRROR); mir.params[PG_P_MIRROR_Y] = 1;
    CHECK(eval(d, &mir, 1, 1).size() == 2 + 2 * 3);
    PGModEntry mul = entry(PG_MOD_MULTIPLY);
    CHECK(eval(d, &mul, 1, 1).size() == 2 * 4);
    PGModEntry env = entry(PG_MOD_ENVELOPE); env.params[PG_P_ENVELOPE_SPREAD] = 2;
    CHECK(eval(d, &env, 1, 1).size() > 2);
    PGModEntry dash = entry(PG_MOD_DASH);
    CHECK(eval(d, &dash, 1, 1).size() > 2);
    PGModEntry out = entry(PG_MOD_OUTLINE);
    auto o = eval(d, &out, 1, 1);
    CHECK(o.size() == 2 && o[0].size() > 9);
    CHECK(snapshot(d.f1).size() == 2 && ((bGPDstroke *)d.f1->strokes.first)->flag == flag0); /* originals as they were */
    free_doc(d);
  }
  /* old 24-float stacks: zero curve/filter blocks are no-ops and survive sanitize */
  float p[PG_MOD_MAX_PARAMS];
  pg_mod_defaults(PG_MOD_HOOK, p);
  pg_mod_sanitize(PG_MOD_HOOK, p);
  CHECK(p[PG_P_CURVE_BASE] == 0 && p[PG_P_FILTER_BASE] == 0 && pg_mod_own_param_count(PG_MOD_HOOK) == PG_P_HOOK_COUNT);
}

/* Build ported from MOD_gpencil_legacy_build.c: Additive keeps the previous key's strokes, Fade. */
static bGPDstroke *line_stroke(bGPDframe *f, int n)
{
  bGPDstroke *gps = BKE_gpencil_stroke_add(f, 0, n, 10, false);
  for (int i = 0; i < n; i++) {
    gps->points[i].x = 10.0f * i; gps->points[i].y = 0.0f; gps->points[i].z = 0.0f;
    gps->points[i].pressure = 1.0f; gps->points[i].strength = 1.0f;
  }
  return gps;
}
static void test_build_blender()
{
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "L", true, false);
  bGPDframe *f1 = BKE_gpencil_frame_addnew(gpl, 1);
  bGPDframe *f11 = BKE_gpencil_frame_addnew(gpl, 11);
  line_stroke(f1, 4);
  for (int k = 0; k < 3; k++) line_stroke(f11, 4);
  PGModEntry b = entry(PG_MOD_BUILD);
  b.params[PG_P_BUILD_LENGTH] = 10;
  auto run = [&](int cfra) { bGPDframe ev; pg_mod_eval_frame(gpd, gpl, f11, &b, 1, cfra, &ev); auto r = snapshot(&ev); pg_mod_eval_free(&ev); return r; };
  /* sequential at fac 0.5: 12 points, last_visible 6 -> 4 + 2 points, third stroke cleared */
  auto seq = run(16);
  CHECK(seq.size() == 2 && seq[0].size() == 4 && seq[1].size() == 2);
  /* additive: the first stroke (already on frame 1) stays, the 2 new ones build over 8 points */
  b.params[PG_P_BUILD_MODE] = 2;
  auto add = run(16);
  CHECK(add.size() == 2 && add[0].size() == 4 && add[1].size() == 4);
  auto add0 = run(11);
  CHECK(add0.size() == 1); /* fac 0: only the previous key's stroke */
  /* vanish removes from the start */
  b.params[PG_P_BUILD_MODE] = 0; b.params[PG_P_BUILD_TRANSITION] = 2;
  auto van = run(16);
  /* Blender keeps end_idx - first_visible points (7 - 6 = 1) of the partly hidden stroke */
  CHECK(van.size() == 2 && van[0].size() == 1 && van[1].size() == 4 && near(van[0][0].v[0], 30.0f));
  /* fade: one 10 point stroke at fac 0.5, fade_fac 0.5 -> 8 visible, opacity ramps 1 .. 0.2 over 3..7 */
  bGPDframe *f21 = BKE_gpencil_frame_addnew(gpl, 21);
  line_stroke(f21, 10);
  PGModEntry fd = entry(PG_MOD_BUILD);
  fd.params[PG_P_BUILD_LENGTH] = 10;
  fd.params[PG_P_BUILD_USE_FADE] = 1; fd.params[PG_P_BUILD_FADE_FAC] = 0.5f; fd.params[PG_P_BUILD_FADE_OPACITY] = 1.0f;
  bGPDframe ev;
  pg_mod_eval_frame(gpd, gpl, f21, &fd, 1, 26, &ev);
  const bGPDstroke *s = static_cast<const bGPDstroke *>(ev.strokes.first);
  CHECK(s && s->totpoints == 8 && near(s->points[0].strength, 1.0f) && near(s->points[3].strength, 1.0f) &&
        near(s->points[7].strength, 0.2f, 1e-4f) && near(s->points[5].strength, 0.6f, 1e-4f));
  pg_mod_eval_free(&ev);
  BKE_gpencil_free_layers(&gpd->layers);
  MEM_freeN(gpd);
}

/* Interpolation (gpencil_interpolate.c): pairing by position, unpaired strokes skipped, flip modes,
 * point counts matched, sequence in-betweens are breakdown keys. */
static void no_cache(bGPdata *) {}
static void test_interpolate_blender()
{
  BKE_gpencil_batch_cache_dirty_tag_cb = no_cache;
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "L", true, false);
  bGPDframe *f1 = BKE_gpencil_frame_addnew(gpl, 1);
  bGPDframe *f5 = BKE_gpencil_frame_addnew(gpl, 5);
  gpl->actframe = f1;
  line_stroke(f1, 4);                 /* x 0..30 left to right */
  bGPDstroke *b = line_stroke(f5, 4); /* same, drawn right to left and moved down 40 */
  for (int i = 0; i < 4; i++) { b->points[i].x = 30.0f - 10.0f * i; b->points[i].y = 40.0f; }
  line_stroke(f5, 6);                 /* unpaired: no partner on frame 1 */
  PGInterpSettings st;
  memset(&st, 0, sizeof(st));
  st.step = 1; st.flipmode = PG_INTERP_FLIPAUTO; st.smooth_steps = 1; st.single = 0; st.factor = -1;
  CHECK(pg_interp_need_flip(static_cast<bGPDstroke *>(f1->strokes.first), b) == 1);
  CHECK(pg_gp_interpolate_run(gpd, gpl, 2, &st) == 3); /* frames 2, 3, 4, one stroke each */
  bGPDframe *f3 = BKE_gpencil_layer_frame_find(gpl, 3);
  CHECK(f3 && f3->key_type == BEZT_KEYTYPE_BREAKDOWN && BLI_listbase_count(&f3->strokes) == 1);
  const bGPDstroke *m = static_cast<const bGPDstroke *>(f3->strokes.first);
  /* auto flip: the start follows the start, so x stays 0 at the first point and y is half way */
  CHECK(near(m->points[0].x, 0.0f) && near(m->points[0].y, 20.0f) && near(m->points[3].x, 30.0f));
  /* no flip: the first point travels to the other end */
  /* drop the in-betweens */
  for (int fn = 2; fn <= 4; fn++) {
    bGPDframe *x = BKE_gpencil_layer_frame_find(gpl, fn);
    if (x) BKE_gpencil_layer_frame_delete(gpl, x);
  }
  st.flipmode = PG_INTERP_NOFLIP; st.single = 1; st.factor = 0.5f;
  CHECK(pg_gp_interpolate_run(gpd, gpl, 3, &st) == 1);
  f3 = BKE_gpencil_layer_frame_find(gpl, 3);
  m = static_cast<const bGPDstroke *>(f3->strokes.first);
  CHECK(near(m->points[0].x, 15.0f) && near(m->points[3].x, 15.0f));
  /* different point counts are resampled to the larger count */
  bGPDframe *f9 = BKE_gpencil_frame_addnew(gpl, 9);
  line_stroke(f9, 8);
  st.single = 1; st.factor = 0.5f; st.flipmode = PG_INTERP_NOFLIP;
  gpl->actframe = f5; /* Blender takes the active frame as the previous key when it is before cfra */
  CHECK(pg_gp_interpolate_run(gpd, gpl, 7, &st) == 1); /* the second stroke of frame 5 has no partner: skipped */
  bGPDframe *f7 = BKE_gpencil_layer_frame_find(gpl, 7);
  CHECK(f7 && static_cast<const bGPDstroke *>(f7->strokes.first)->totpoints == 8);
  BKE_gpencil_free_layers(&gpd->layers);
  MEM_freeN(gpd);
}

/* Texture Mapping (deformStroke): fit stroke divides uv_fac by the length, then scale, offset,
 * alignment rotation; fill mode moves the stroke's fill uv transform. */
static void test_texture_modifier()
{
  Doc d = make_pair_doc(1); /* one 2-point stroke, length hypot(60, 60) */
  PGModEntry t = entry(PG_MOD_TEXTURE);
  t.params[PG_P_TEXTURE_MODE] = 2;
  t.params[PG_P_TEXTURE_FIT] = 0; /* GP_TEX_FIT_STROKE */
  t.params[PG_P_TEXTURE_UV_SCALE] = 2.0f;
  t.params[PG_P_TEXTURE_UV_OFFSET] = 0.25f;
  t.params[PG_P_TEXTURE_ALIGN_ROT] = 0.5f;
  t.params[PG_P_TEXTURE_FILL_ROT] = 0.3f;
  t.params[PG_P_TEXTURE_FILL_OFFSET_X] = 0.1f;
  t.params[PG_P_TEXTURE_FILL_SCALE] = 3.0f;
  bGPDframe ev;
  pg_mod_eval_frame(d.gpd, d.gpl, d.f1, &t, 1, 1, &ev);
  const bGPDstroke *s = static_cast<const bGPDstroke *>(ev.strokes.first);
  /* geometry update sets uv_fac to the running length: 0 and 84.85 */
  const float len = hypotf(60.0f, 60.0f);
  CHECK(s && near(s->points[0].uv_fac, 0.25f) && near(s->points[1].uv_fac, len / len * 2.0f + 0.25f, 1e-3f) &&
        near(s->points[1].uv_rot, 0.5f) && near(s->uv_rotation, 0.3f) && near(s->uv_translation[0], 0.1f) &&
        near(s->uv_scale, 3.0f));
  pg_mod_eval_free(&ev);
  free_doc(d);
}

/* Length random (applyLength): rand_start_fac / rand_end_fac add a per-stroke offset in [0, 2) *
 * factor; the same seed gives the same result, another seed a different one. */
static void test_length_random()
{
  Doc d = make_doc();
  PGModEntry e = entry(PG_MOD_LENGTH);
  e.params[PG_P_LENGTH_START] = 0.0f; e.params[PG_P_LENGTH_END] = 0.0f;
  auto base = eval(d, &e, 1, 1);
  e.params[PG_P_LENGTH_RAND_START] = 0.3f; e.params[PG_P_LENGTH_RAND_END] = 0.3f; e.params[PG_P_LENGTH_SEED] = 5;
  auto r1 = eval(d, &e, 1, 1), r2 = eval(d, &e, 1, 1);
  CHECK(same(r1, r2));      /* deterministic */
  CHECK(!same(r1, base));   /* the random offsets lengthen the strokes */
  e.params[PG_P_LENGTH_SEED] = 9;
  CHECK(!same(eval(d, &e, 1, 1), r1));
  /* GP_LENGTH_USE_RANDOM: changes every `step` frames */
  e.params[PG_P_LENGTH_USE_RANDOM] = 1; e.params[PG_P_LENGTH_STEP] = 4;
  CHECK(same(eval(d, &e, 1, 1), eval(d, &e, 1, 2)) && !same(eval(d, &e, 1, 1), eval(d, &e, 1, 4)));
  free_doc(d);
}

int main()
{
  test_length_random();
  test_texture_modifier();
  test_interpolate_blender();
  test_build_blender();
  test_type_info();
  test_batch21();
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
