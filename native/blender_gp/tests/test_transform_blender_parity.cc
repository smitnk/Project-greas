/* Stroke transform against a real Blender 3.6.23 run (tools/transform_reference/blender_transform_reference.py).
 *
 * The same legacy GP drawing (one layer, frames 1, 5 (active) and 9, partly / fully / not selected
 * strokes) goes through Backend::transform_begin / update / end (project_grease_transform.c) for every
 * case of the script: translate / rotate / resize / mirror x each pivot, each proportional falloff,
 * Connected Only, proportional with each pivot, multi-frame editing and multi-frame falloff. The
 * fixture and the case table are duplicated in the Python script; the JSON's case names are checked
 * against these.
 *
 * TRANSFORM_BLENDER_RUN=<file> compares every point of every frame with Blender's (1e-3 canvas units)
 * and fails when nothing was compared. TRANSFORM_BLENDER_REPORT=<file> also writes the per-case lines.
 * Without the variable it runs the self-consistency checks (worked out by hand), which always run. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_scene_types.h"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "project_grease_gp_backend.h"
#include "project_grease_transform.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) do { checks++; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (e))) { \
    printf("FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); failures++; } } while (0)

/* ---- a minimal JSON reader (as test_rig_blender_parity.cc) ---- */
struct Json {
  char kind = 0;
  double n = 0;
  std::string s;
  std::vector<Json> a;
  std::vector<std::pair<std::string, Json>> o;
  const Json *get(const char *k) const
  {
    for (const auto &p : o) if (p.first == k) return &p.second;
    return nullptr;
  }
};
static void skip_ws(const char *&p) { while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') p++; }
static bool parse_json(const char *&p, Json &j)
{
  skip_ws(p);
  if (*p == '{' || *p == '[') {
    const char close = *p == '{' ? '}' : ']';
    j.kind = *p == '{' ? 'o' : 'a';
    p++;
    skip_ws(p);
    while (*p && *p != close) {
      std::string key;
      if (j.kind == 'o') {
        Json k;
        if (!parse_json(p, k) || k.kind != 's') return false;
        key = k.s;
        skip_ws(p);
        if (*p++ != ':') return false;
      }
      Json v;
      if (!parse_json(p, v)) return false;
      if (j.kind == 'o') j.o.emplace_back(key, std::move(v)); else j.a.push_back(std::move(v));
      skip_ws(p);
      if (*p == ',') p++;
      skip_ws(p);
    }
    return *p++ == close;
  }
  if (*p == '"') {
    j.kind = 's';
    p++;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; j.s += *p++; }
    return *p++ == '"';
  }
  char *end = nullptr;
  j.kind = 'n';
  j.n = std::strtod(p, &end);
  if (end == p) return false;
  p = end;
  return true;
}

/* ---- the fixture (duplicated in blender_transform_reference.py) ---- */
struct Pt { float x, y; bool sel; };
using Stroke = std::vector<Pt>;
template <class F> static Stroke line(float x0, float y0, float dx, float dy, int n, F sel)
{
  Stroke s;
  for (int i = 0; i < n; i++) s.push_back({x0 + dx * i, y0 + dy * i, sel(i)});
  return s;
}
static const int FRAMES[3] = {1, 5, 9};
static const int ACTIVE = 5;
static const float CURSOR[2] = {13.0f, -7.0f};
static std::vector<Stroke> fixture(int f)
{
  if (f == 1) return {line(0, 0, 10, 2, 12, [](int i) { return i >= 4 && i <= 6; }),
                      line(40, 60, 6, -3, 8, [](int) { return true; }),
                      line(-30, 20, 5, 9, 7, [](int) { return false; })};
  if (f == 5) return {line(0, 0, 11, 1, 10, [](int i) { return i == 2 || i == 3; }),
                      line(-20, -40, 7, 5, 6, [](int) { return false; })};
  return {line(5, 5, 9, -2, 10, [](int i) { return i >= 7; })};
}

struct Case { std::string name; int mode; float v[2]; int pivot; bool prop; int falloff; float size; bool connected; bool multi, mf; };
static std::vector<Case> cases()
{
  const char *pivots[] = {"MEDIAN_POINT", "BOUNDING_BOX_CENTER", "INDIVIDUAL_ORIGINS", "CURSOR"};
  const int pivot_ids[] = {PG_TFM_PIVOT_MEDIAN, PG_TFM_PIVOT_BOUNDS, PG_TFM_PIVOT_INDIVIDUAL, PG_TFM_PIVOT_CURSOR};
  const char *falloffs[] = {"SMOOTH", "SPHERE", "ROOT", "INVERSE_SQUARE", "SHARP", "LINEAR", "CONSTANT"};
  const int falloff_ids[] = {PROP_SMOOTH, PROP_SPHERE, PROP_ROOT, PROP_INVSQUARE, PROP_SHARP, PROP_LIN, PROP_CONST};
  struct Op { const char *name; int mode; float v[2]; } ops[] = {
      {"translate", PG_TFM_TRANSLATION, {14.5f, -9.25f}}, {"rotate", PG_TFM_ROTATION, {0.7f, 0}},
      {"resize", PG_TFM_RESIZE, {1.6f, 0.55f}}, {"mirror", PG_TFM_MIRROR, {0, 0}}};
  std::vector<Case> out;
  for (const Op &op : ops) {
    const std::string n = op.name;
    for (int p = 0; p < 4; p++) out.push_back({n + "_" + pivots[p], op.mode, {op.v[0], op.v[1]}, pivot_ids[p], false, 0, 0, false, false, false});
    out.push_back({n + "_multiframe", op.mode, {op.v[0], op.v[1]}, 0, false, 0, 0, false, true, false});
    if (op.mode == PG_TFM_MIRROR) continue;
    for (int f = 0; f < 7; f++) out.push_back({n + "_prop_" + falloffs[f], op.mode, {op.v[0], op.v[1]}, 0, true, falloff_ids[f], 35, false, false, false});
    out.push_back({n + "_prop_connected", op.mode, {op.v[0], op.v[1]}, 0, true, PROP_SMOOTH, 35, true, false, false});
    for (int p = 1; p < 4; p++) out.push_back({n + "_prop_" + pivots[p], op.mode, {op.v[0], op.v[1]}, pivot_ids[p], true, PROP_LIN, 50, false, false, false});
    out.push_back({n + "_multiframe_falloff_prop", op.mode, {op.v[0], op.v[1]}, 0, true, PROP_SMOOTH, 35, false, true, true});
  }
  return out;
}

static bGPDframe *frame_of(bGPDlayer *gpl, int fn)
{
  LISTBASE_FOREACH (bGPDframe *, f, &gpl->frames) if (f->framenum == fn) return f;
  return nullptr;
}

/* the drawing as the script resets it before each case */
static void reset(bGPdata *gpd, bool multi)
{
  bGPDlayer *gpl = static_cast<bGPDlayer *>(gpd->layers.first);
  for (int fn : FRAMES) {
    bGPDframe *f = frame_of(gpl, fn);
    if (multi) f->flag |= GP_FRAME_SELECT; else f->flag &= ~GP_FRAME_SELECT;
    const auto strokes = fixture(fn);
    bGPDstroke *gps = static_cast<bGPDstroke *>(f->strokes.first);
    for (const Stroke &s : strokes) {
      bool any = false;
      for (size_t i = 0; i < s.size(); i++) {
        bGPDspoint &pt = gps->points[i];
        pt.x = s[i].x; pt.y = s[i].y; pt.z = 0;
        if (s[i].sel) pt.flag |= GP_SPOINT_SELECT; else pt.flag &= ~GP_SPOINT_SELECT;
        any |= s[i].sel;
      }
      if (any) gps->flag |= GP_STROKE_SELECT; else gps->flag &= ~GP_STROKE_SELECT;
      gps = gps->next;
    }
  }
  if (multi) gpd->flag |= GP_DATA_STROKE_MULTIEDIT; else gpd->flag &= ~GP_DATA_STROKE_MULTIEDIT;
}

static bool run(Backend &b, int mode, const float v[2], const PGTransformOptions &o)
{
  if (!b.transform_begin(mode, &o)) return false;
  const bool ok = b.transform_update(v);
  b.transform_end(false);
  return ok;
}

static PGTransformOptions opts(const Case &c)
{
  PGTransformOptions o{};
  o.pivot = c.pivot; o.cursor[0] = CURSOR[0]; o.cursor[1] = CURSOR[1];
  o.prop_edit = c.prop; o.prop_connected = c.connected; o.prop_mode = c.falloff; o.prop_size = c.size;
  o.multiframe_falloff = c.mf;
  return o;
}

static bGPDspoint *point(bGPdata *gpd, int fn, int s, int i)
{
  bGPDframe *f = frame_of(static_cast<bGPDlayer *>(gpd->layers.first), fn);
  return &static_cast<bGPDstroke *>(BLI_findlink(&f->strokes, s))->points[i];
}

static void self_checks(Backend &b, bGPdata *gpd)
{
  /* falloffs (calculatePropRatio) at half the radius, and the increment snap */
  NEAR(pg_transform_prop_factor(PROP_SMOOTH, 5, 10), 0.5, 1e-6);
  NEAR(pg_transform_prop_factor(PROP_SHARP, 5, 10), 0.25, 1e-6);
  NEAR(pg_transform_prop_factor(PROP_ROOT, 5, 10), std::sqrt(0.5), 1e-6);
  NEAR(pg_transform_prop_factor(PROP_INVSQUARE, 5, 10), 0.75, 1e-6);
  NEAR(pg_transform_prop_factor(PROP_SPHERE, 5, 10), std::sqrt(0.75), 1e-6);
  NEAR(pg_transform_prop_factor(PROP_CONST, 9, 10), 1, 1e-6);
  NEAR(pg_transform_prop_factor(PROP_LIN, 12, 10), 0, 1e-6);
  NEAR(pg_transform_snap_increment(14.6f, 5), 15, 1e-6);
  NEAR(pg_transform_snap_increment(-9.25f, 5), -10, 1e-6);

  Case c{"", PG_TFM_TRANSLATION, {14.5f, -9.25f}, 0, false, 0, 0, false, false, false};
  /* translate, median: only the selected points of the active frame move */
  reset(gpd, false);
  CHECK(run(b, c.mode, c.v, opts(c)));
  NEAR(point(gpd, 5, 0, 2)->x, 22 + 14.5, 1e-5);
  NEAR(point(gpd, 5, 0, 1)->x, 11, 1e-6);
  NEAR(point(gpd, 1, 0, 4)->x, 40, 1e-6);
  /* increment snapping: the total value snaps (not each step) */
  reset(gpd, false);
  PGTransformOptions o = opts(c);
  o.snap = 1; o.snap_translate = 5;
  CHECK(b.transform_begin(c.mode, &o));
  const float step[2] = {3, 0}, total[2] = {14.5f, -9.25f};
  CHECK(b.transform_update(step));
  NEAR(point(gpd, 5, 0, 2)->x, 22 + 5, 1e-5);
  CHECK(b.transform_update(total));
  NEAR(point(gpd, 5, 0, 2)->x, 22 + 15, 1e-5);
  NEAR(point(gpd, 5, 0, 2)->y, 2 - 10, 1e-5);
  /* cancel restores */
  b.transform_end(true);
  NEAR(point(gpd, 5, 0, 2)->x, 22, 1e-6);
  CHECK(!b.transform_active());
  /* rotation snaps to 5 degrees: 0.7 rad -> 40 degrees, clockwise about the median (27.5, 2.5) */
  reset(gpd, false);
  o = opts(c);
  o.snap = 1;
  CHECK(b.transform_begin(PG_TFM_ROTATION, &o));
  const float rot[2] = {0.7f, 0};
  CHECK(b.transform_update(rot));
  b.transform_end(false);
  const double a = -40.0 * M_PI / 180.0;
  NEAR(point(gpd, 5, 0, 2)->x, 27.5 + std::cos(a) * -5.5 - std::sin(a) * -0.5, 1e-4);
  NEAR(point(gpd, 5, 0, 2)->y, 2.5 + std::sin(a) * -5.5 + std::cos(a) * -0.5, 1e-4);
  /* resize snaps to 0.1 steps: 1.63 -> 1.6 */
  reset(gpd, false);
  CHECK(b.transform_begin(PG_TFM_RESIZE, &o));
  const float sc[2] = {1.63f, 1.63f};
  CHECK(b.transform_update(sc));
  b.transform_end(false);
  NEAR(point(gpd, 5, 0, 2)->x, 27.5 - 5.5 * 1.6, 1e-4);
  /* nothing selected: no session */
  reset(gpd, false);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers)
    LISTBASE_FOREACH (bGPDframe *, f, &gpl->frames)
      LISTBASE_FOREACH (bGPDstroke *, gps, &f->strokes) {
        gps->flag &= ~GP_STROKE_SELECT;
        for (int i = 0; i < gps->totpoints; i++) gps->points[i].flag &= ~GP_SPOINT_SELECT;
      }
  o = opts(c);
  o.prop_edit = 1; o.prop_size = 30;
  CHECK(!b.transform_begin(PG_TFM_TRANSLATION, &o));
  /* an edit operator confirms the open transform (modal_confirm), undo-style replacement discards it */
  reset(gpd, false);
  o = opts(c);
  CHECK(b.transform_begin(PG_TFM_TRANSLATION, &o));
  CHECK(b.transform_update(c.v));
  CHECK(b.modal_confirm());
  CHECK(!b.transform_active());
  NEAR(point(gpd, 5, 0, 2)->x, 22 + 14.5, 1e-5);

  /* curve edit mode (createTransGPencil_curves, handles shown for selected curve points): a selected
   * control point moves with its handles; cancel puts them and the handle types back */
  reset(gpd, false);
  bGPDstroke *gps = static_cast<bGPDstroke *>(frame_of(static_cast<bGPDlayer *>(gpd->layers.first), ACTIVE)->strokes.first);
  BKE_gpencil_stroke_editcurve_update(gpd, static_cast<bGPDlayer *>(gpd->layers.first), gps);
  CHECK(gps->editcurve && gps->editcurve->tot_curve_points >= 2);
  if (gps->editcurve) {
    gpd->flag |= GP_DATA_CURVE_EDIT_MODE | GP_DATA_STROKE_EDITMODE; /* GPENCIL_CURVE_EDIT_SESSIONS_ON */
    bGPDcurve_point *cp = &gps->editcurve->curve_points[0];
    for (int i = 0; i < gps->editcurve->tot_curve_points; i++) {
      BezTriple *bz = &gps->editcurve->curve_points[i].bezt;
      bz->f1 = bz->f2 = bz->f3 = 0;
      gps->editcurve->curve_points[i].flag &= ~GP_CURVE_POINT_SELECT;
    }
    cp->bezt.f1 = cp->bezt.f2 = cp->bezt.f3 = SELECT;
    cp->flag |= GP_CURVE_POINT_SELECT;
    const float k0 = cp->bezt.vec[1][0], h0 = cp->bezt.vec[2][0];
    const float k1 = gps->editcurve->curve_points[1].bezt.vec[1][0];
    o = opts(c);
    CHECK(b.transform_begin(PG_TFM_TRANSLATION, &o));
    const float dv[2] = {7, 3};
    CHECK(b.transform_update(dv));
    NEAR(cp->bezt.vec[1][0], k0 + 7, 1e-5);
    NEAR(cp->bezt.vec[2][0], h0 + 7, 1e-5);
    NEAR(gps->editcurve->curve_points[1].bezt.vec[1][0], k1, 1e-6);
    b.transform_end(true);
    NEAR(cp->bezt.vec[1][0], k0, 1e-6);
    gpd->flag &= ~(GP_DATA_CURVE_EDIT_MODE | GP_DATA_STROKE_EDITMODE);
    BKE_gpencil_free_stroke_editcurve(gps);
  }
}

int main()
{
  const char *run_path = std::getenv("TRANSFORM_BLENDER_RUN");
  Json ref;
  if (run_path) {
    std::ifstream in(run_path);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    const char *p = text.c_str();
    if (text.empty() || !parse_json(p, ref) || ref.kind != 'o') {
      std::printf("FAIL: cannot read the Blender run %s\n", run_path);
      return 1;
    }
    std::printf("Blender run %s: %s\n", run_path, ref.get("blender") ? ref.get("blender")->s.c_str() : "?");
  }

  Backend b;
  CHECK(b.initialize() && b.create_document() && b.create_layer("L"));
  bGPdata *gpd = b.document_data();
  bGPDlayer *gpl = static_cast<bGPDlayer *>(gpd->layers.first);
  for (int fn : FRAMES) {
    CHECK(b.create_frame(fn));
    bGPDframe *f = frame_of(gpl, fn);
    for (const Stroke &s : fixture(fn)) {
      bGPDstroke *gps = BKE_gpencil_stroke_new(0, int(s.size()), 10);
      gps->flag |= GP_STROKE_3DSPACE; /* display_mode = "3DSPACE" in the script */
      BLI_addtail(&f->strokes, gps);
    }
  }
  gpl->actframe = frame_of(gpl, ACTIVE);
  b.sync_active_frame();

  self_checks(b, gpd);

  const auto cs = cases();
  int run_failures = 0, compared = 0;
  double worst = 0;
  std::string worst_case;
  std::ofstream rep_file;
  if (const char *rp = std::getenv("TRANSFORM_BLENDER_REPORT")) rep_file.open(rp);
  if (run_path) {
    const Json *names = ref.get("cases"), *results = ref.get("results");
    CHECK(names && results && names->a.size() == cs.size());
    for (size_t k = 0; names && k < cs.size() && k < names->a.size(); k++) CHECK(names->a[k].s == cs[k].name);
    for (size_t k = 0; results && k < cs.size() && k < results->a.size(); k++) {
      const Case &c = cs[k];
      reset(gpd, c.multi);
      const float v[2] = {c.mode == PG_TFM_MIRROR ? 0.0f : c.v[0], c.v[1]};
      const bool ok = run(b, c.mode, v, opts(c));
      const Json *fr = results->a[k].get("frames");
      double maxd = 0;
      int pts = 0;
      for (int fn : FRAMES) {
        const Json *fj = fr ? fr->get(std::to_string(fn).c_str()) : nullptr;
        bGPDframe *f = frame_of(gpl, fn);
        int si = 0;
        LISTBASE_FOREACH (bGPDstroke *, gps, &f->strokes) {
          const Json *sj = fj && si < int(fj->a.size()) ? &fj->a[si] : nullptr;
          for (int i = 0; i < gps->totpoints; i++) {
            if (!sj || i >= int(sj->a.size())) { maxd = 1e9; continue; }
            maxd = std::max({maxd, std::fabs(gps->points[i].x - sj->a[i].a[0].n), std::fabs(gps->points[i].y - sj->a[i].a[1].n)});
            pts++;
          }
          si++;
        }
      }
      const bool pass = ok && maxd <= 1e-3;
      if (!pass) run_failures++;
      compared += pts;
      if (maxd > worst) { worst = maxd; worst_case = c.name; }
      char line[256];
      std::snprintf(line, sizeof(line), "%s BLENDER-RUN %s: %d points, max diff %.3g%s\n", pass ? "  ok" : "FAIL",
                    c.name.c_str(), pts, maxd, ok ? "" : " (transform refused)");
      std::fputs(line, stdout);
      if (rep_file) rep_file << line;
    }
    char line[256];
    std::snprintf(line, sizeof(line), "BLENDER-RUN %zu cases, %d points, max diff %.3g (%s), %d failing\n", cs.size(),
                  compared, worst, worst_case.c_str(), run_failures);
    std::fputs(line, stdout);
    if (rep_file) rep_file << line;
    if (compared == 0) run_failures++;
  }
  failures += run_failures;
  if (failures) std::printf("%d FAILURES (%d checks)\n", failures, checks);
  else std::printf("transform blender parity tests passed (%d checks)\n", checks);
  return failures ? 1 : 0;
}
