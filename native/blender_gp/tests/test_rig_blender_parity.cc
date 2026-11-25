/* The rig against a real Blender 3.6.23 run (tools/rig_reference/blender_rig_reference.py).
 *
 * The identical rig is built through the app's own API (Backend + the PG_ARM_CMD_* bridge): a legacy
 * GP drawing (layer "Body" with three strokes, layer "Mouth" with three drawings under a Time Offset
 * modifier), 15 bones (a chain, a branch, connected / unconnected bones, roll), Parent with Automatic
 * Weights, Bezier and Linear pose keys at frames 1 and 10, IK (chain 2, pole), Copy Rotation, Copy
 * Location, Damped Track, Limit Rotation, Child Of (Set Inverse) and drivers (a bone rotation driving
 * a bone, a "Smile" property driving a bone and the Time Offset). The constants are duplicated in the
 * Python script; the JSON's bone names are checked against these.
 *
 * RIG_BLENDER_RUN=<file> compares with Blender's JSON at frames 1, 5, 10: pose bone matrices (1e-5),
 * head / tail (0.01 canvas px), the evaluated stroke points after the Armature + Time Offset modifiers
 * (0.01 px, pressure / strength 1e-4) and the vertex group weights after bind (1e-4). It fails when
 * nothing was compared. Without the variable the same rig runs a self-consistency check (constraint
 * and key results worked out by hand, the weights sane, evaluation repeatable). */
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "BLI_listbase.h"
#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "project_grease_armature.h"
#include "project_grease_constraint.h"
#include "project_grease_driver.h"
#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"
#include "project_grease_modifier_stack.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;
using project_grease::gp::StrokePoint;

int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name);
int pg_arm_bridge_query(Backend &backend, int what, int bone, float x, float *out, int cap);

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) do { checks++; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (e))) { \
    printf("FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); failures++; } } while (0)

/* ---- a minimal JSON reader (numbers, strings, arrays, objects) ---- */
struct Json {
  char kind = 0; /* n s a o */
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
    if (*p++ != close) return false;
    return true;
  }
  if (*p == '"') {
    j.kind = 's';
    p++;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; j.s += *p++; }
    return *p++ == '"';
  }
  char *end = nullptr;
  j.kind = 'n';
  j.n = std::strtod(p, &end); /* also NaN / Infinity as Python writes them */
  if (end == p) return false;
  p = end;
  return true;
}

/* ---- the rig (duplicated in blender_rig_reference.py) ---- */
struct BoneDef { const char *name; float h[2], t[2]; float roll; int parent; bool conn; };
static const BoneDef BONES[] = {
    {"Root", {500, 800}, {500, 600}, 0.0f, -1, false},
    {"Spine", {500, 600}, {500, 400}, 0.0f, 0, true},
    {"Arm.U", {500, 450}, {700, 450}, 0.3f, 1, false},
    {"Arm.L", {700, 450}, {850, 380}, 0.0f, 2, true},
    {"Head", {500, 400}, {500, 250}, 0.0f, 1, true},
    {"IK.T", {900, 300}, {900, 250}, 0.0f, -1, false},
    {"Pole", {700, 300}, {700, 250}, 0.0f, -1, false},
    {"Src", {200, 500}, {300, 500}, 0.0f, -1, false},
    {"Follow", {200, 600}, {300, 600}, 0.0f, -1, false},
    {"RotCopy", {200, 700}, {300, 700}, 0.0f, -1, false},
    {"Aim", {300, 300}, {400, 300}, 0.0f, -1, false},
    {"Lim", {100, 400}, {100, 300}, 0.0f, -1, false},
    {"Kid", {200, 200}, {250, 200}, 0.0f, -1, false},
    {"Smile", {400, 100}, {450, 100}, 0.0f, -1, false},
    {"DrvBy", {500, 100}, {550, 100}, 0.0f, -1, false},
};
static const int NBONES = (int)(sizeof(BONES) / sizeof(BONES[0]));
static int bone_index(const char *name)
{
  for (int i = 0; i < NBONES; i++) if (!std::strcmp(BONES[i].name, name)) return i;
  return -1;
}
struct Pose { float loc[3], q[4], scale[3]; };
static Pose pose_z(float x, float y, float z, float a) { return {{x, y, z}, {std::cos(a / 2), 0, 0, std::sin(a / 2)}, {1, 1, 1}}; }
static Pose pose_axis(const float loc[3], const float ax[3], float a)
{
  const float n = std::sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]), s = std::sin(a / 2) / n;
  return {{loc[0], loc[1], loc[2]}, {std::cos(a / 2), ax[0] * s, ax[1] * s, ax[2] * s}, {1, 1, 1}};
}
struct KeyDef { const char *bone; int ipo; Pose f1, f10; };
static const float SRC10_LOC[3] = {0.08f, -0.03f, 0.01f}, SRC10_AXIS[3] = {0.3f, 0.1f, 0.9f};
static const KeyDef KEYS[] = {
    {"Spine", 2, pose_z(0, 0, 0, 0.0f), pose_z(0, 0, 0, 0.35f)},
    {"Arm.U", 1, pose_z(0, 0, 0, -0.2f), pose_z(0.02f, 0.03f, 0, 0.5f)},
    {"Head", 2, pose_z(0, 0, 0, 0.0f), pose_z(0, 0, 0, -0.3f)},
    {"Src", 2, pose_z(0.01f, 0.02f, 0, 0.2f), pose_axis(SRC10_LOC, SRC10_AXIS, 0.9f)},
    {"IK.T", 1, pose_z(0, 0, 0, 0.0f), pose_z(-0.05f, 0.08f, 0, 0.0f)},
    {"Lim", 2, pose_z(0, 0, 0, 0.0f), pose_z(0, 0, 0, 1.2f)},
};
static std::vector<std::vector<std::pair<float, float>>> body_strokes()
{
  std::vector<std::vector<std::pair<float, float>>> s(3);
  for (int i = 0; i < 10; i++) s[0].push_back({500.0f + 6 * i, 780.0f - 40 * i});
  for (int i = 0; i < 12; i++) s[1].push_back({480.0f + 20 * i, 440.0f + 3 * (i % 3)});
  for (int i = 0; i < 8; i++) s[2].push_back({860.0f - 5 * i, 380.0f + 12 * i});
  return s;
}
static std::vector<std::pair<float, float>> mouth_stroke(int frame)
{
  std::vector<std::pair<float, float>> s;
  for (int i = 0; i < 6; i++) {
    const float x = 30.0f + 10 * i;
    s.push_back({x, frame == 1 ? 40.0f + 5 * i : frame == 2 ? 60.0f - 4 * i : 80.0f + 3 * (i % 2)});
  }
  return s;
}
static const float SMILE[3] = {0.0f, 0.5f, 1.0f};
static const int FRAMES[3] = {1, 5, 10};

static int cmd(Backend &b, int c, int bone, std::initializer_list<float> args, const char *name = nullptr)
{
  return pg_arm_bridge_command(b, c, bone, args.size() ? args.begin() : nullptr, (int)args.size(), name);
}
static bool add_stroke(Backend &b, const std::vector<std::pair<float, float>> &pts)
{
  std::vector<StrokePoint> sp(pts.size());
  for (size_t i = 0; i < pts.size(); i++) {
    sp[i] = StrokePoint();
    sp[i].x = pts[i].first;
    sp[i].y = pts[i].second;
    sp[i].pressure = sp[i].strength = 1.0f;
  }
  PGStrokeInfo info = {};
  info.thickness = 10;
  return b.add_stroke(sp.data(), (int)sp.size(), info);
}
/* a constraint's settings patched through the bridge: the query returns type + the pg_arm_con_set values */
static void con_patch(Backend &b, int bone, int idx, std::initializer_list<std::pair<int, float>> edits)
{
  float q[PG_CON_MAX_VALUES + 2];
  const int n = pg_arm_bridge_query(b, PG_ARM_Q_CONSTRAINT, bone, (float)idx, q, PG_CON_MAX_VALUES + 2);
  CHECK(n > 5);
  std::vector<float> v(q + 1, q + n);
  for (const auto &e : edits) v[(size_t)e.first] = e.second;
  std::vector<float> args(v.size() + 1);
  args[0] = (float)idx;
  std::copy(v.begin(), v.end(), args.begin() + 1);
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_CON_SET, bone, args.data(), (int)args.size(), nullptr) == 1);
}

static bGPDlayer *layer_at(Backend &b, int i) { return (bGPDlayer *)BLI_findlink(&b.document_data()->layers, i); }
static const bGPDframe *frame_at(const bGPDlayer *l, int f)
{
  const bGPDframe *found = nullptr;
  for (const bGPDframe *g = (const bGPDframe *)l->frames.first; g; g = g->next) if (g->framenum <= f) found = g;
  return found;
}
struct PgBone { float head[3], tail[3], m[16]; };
static bool pg_bone(Backend &b, int i, PgBone *r)
{
  float q[PG_ARM_BONE_INFO];
  if (pg_arm_bridge_query(b, PG_ARM_Q_BONE, i, 0, q, PG_ARM_BONE_INFO) != PG_ARM_BONE_INFO) return false;
  std::memcpy(r->head, q + 26, sizeof(r->head));
  std::memcpy(r->tail, q + 29, sizeof(r->tail));
  std::memcpy(r->m, q + 32, sizeof(r->m));
  return true;
}
/* armature space (Blender units) -> canvas px, as pg_arm_arm_to_canvas */
static void to_px(const double u[3], double px[3]) { px[0] = u[0] * 1000.0; px[1] = -u[1] * 1000.0; px[2] = u[2] * 1000.0; }

static int compared = 0, run_failures = 0;
static std::ofstream *report = nullptr;
static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...)
{
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  std::fputs(buf, stdout);
  if (report) *report << buf;
}

int main()
{
  const char *run_path = std::getenv("RIG_BLENDER_RUN");
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
  CHECK(b.initialize() && b.create_document());
  CHECK(b.create_layer("Body") && b.create_frame(1));
  const auto body = body_strokes();
  for (const auto &s : body) CHECK(add_stroke(b, s));
  CHECK(b.create_layer("Mouth"));
  for (int f = 1; f <= 3; f++) CHECK(b.create_frame(f) && add_stroke(b, mouth_stroke(f)));
  /* Time Offset, Fixed Frame, before the Armature modifier that Bind appends */
  CHECK(b.modifier_add(1, PG_MOD_TIME) == 0);
  {
    int type = 0, en = 0;
    float p[PG_MOD_MAX_PARAMS];
    b.modifier_get(1, 0, &type, &en, p, PG_MOD_MAX_PARAMS);
    p[PG_P_TIME_MODE] = GP_TIME_MODE_FIX;
    CHECK(b.modifier_set_params(1, 0, p, PG_MOD_MAX_PARAMS));
  }

  /* bones */
  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {1}) == 1);
  for (int i = 0; i < NBONES; i++) {
    const BoneDef &d = BONES[i];
    CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {d.h[0], d.h[1], d.t[0], d.t[1]}, d.name) == i);
    if (d.roll != 0.0f) CHECK(cmd(b, PG_ARM_CMD_BONE_SET, i, {d.h[0], d.h[1], d.t[0], d.t[1], d.roll}) == 1);
    if (d.parent >= 0) {
      cmd(b, PG_ARM_CMD_EDIT_SELECT, i, {1, 0});
      cmd(b, PG_ARM_CMD_EDIT_SELECT, d.parent, {1, 1});
      CHECK(cmd(b, PG_ARM_CMD_PARENT_SET, -1, {d.conn ? 1.0f : 2.0f}) == 1);
    }
  }
  /* Parent with Automatic Weights, at frame 1 on each layer's active frame */
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {0}) == 1);
  CHECK(b.select_layer(1) && b.select_frame_or_hold(1) && b.select_layer(0) && b.select_frame_or_hold(1));
  CHECK(cmd(b, PG_ARM_CMD_BIND, -1, {2}) >= NBONES);

  /* the vertex group weights after bind, per "Layer/frame/stroke" -> group -> points */
  std::map<std::string, std::map<std::string, std::vector<double>>> pgw;
  const char *layer_names[2] = {"Body", "Mouth"};
  const int layer_frames[2] = {1, 3};
  double wmax = 0;
  for (int li = 0; li < 2; li++) {
    CHECK(b.select_layer(li));
    for (int fr = 1; fr <= layer_frames[li]; fr++) {
      CHECK(b.select_frame(fr));
      for (int si = 0; si < b.stroke_count(); si++) {
        char key[64];
        std::snprintf(key, sizeof(key), "%s/%d/%d", layer_names[li], fr, si);
        for (int g = 0; g < b.vertex_group_count(); g++) {
          char gn[64];
          CHECK(b.vertex_group_name(g, gn, sizeof(gn)));
          std::vector<double> w;
          StrokePoint sp;
          for (int pi = 0; b.get_point(si, pi, &sp); pi++) {
            float v = 0;
            if (!b.get_point_group_weight(si, pi, g, &v)) v = 0; /* no weight = 0, as Blender's -1 */
            w.push_back(v);
            wmax = std::max<double>(wmax, v);
          }
          pgw[key][gn] = w;
        }
      }
    }
  }
  CHECK(b.select_layer(0) && b.select_frame_or_hold(1));
  CHECK(b.vertex_group_count() == NBONES);
  CHECK(wmax > 0.5 && wmax <= 1.0);
  { /* the drawing is what the Python script built */
    StrokePoint a, z;
    CHECK(b.get_point(0, 0, &a) && b.get_point(2, 7, &z));
    NEAR(a.x, body[0][0].first, 1e-3); NEAR(a.y, body[0][0].second, 1e-3);
    NEAR(z.x, body[2][7].first, 1e-3); NEAR(z.y, body[2][7].second, 1e-3);
  }

  /* pose keys */
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {2}) == 1);
  for (int fi = 0; fi < 2; fi++) {
    const int frame = fi == 0 ? 1 : 10;
    bool first = true;
    for (const KeyDef &k : KEYS) {
      const int i = bone_index(k.bone);
      const Pose &p = fi == 0 ? k.f1 : k.f10;
      CHECK(cmd(b, PG_ARM_CMD_POSE_SET, i, {p.loc[0], p.loc[1], p.loc[2], p.q[0], p.q[1], p.q[2], p.q[3],
                                            p.scale[0], p.scale[1], p.scale[2]}) == 1);
      cmd(b, PG_ARM_CMD_POSE_SELECT, i, {first ? 0.0f : 1.0f});
      first = false;
    }
    CHECK(cmd(b, PG_ARM_CMD_KEY_INSERT, -1, {(float)frame}) > 0);
  }
  for (const KeyDef &k : KEYS) {
    const int i = bone_index(k.bone);
    CHECK(cmd(b, PG_ARM_CMD_KEY_INTERPOLATION, i, {1, (float)k.ipo}) == PG_ARM_CHANNELS);
    CHECK(cmd(b, PG_ARM_CMD_KEY_INTERPOLATION, i, {10, (float)k.ipo}) == PG_ARM_CHANNELS);
  }

  /* constraints */
  const int ik = cmd(b, PG_ARM_CMD_CON_ADD, bone_index("Arm.L"), {CONSTRAINT_TYPE_KINEMATIC});
  CHECK(ik == 0);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, bone_index("Arm.L"), {0, 0, 0}, "IK.T") == 1);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, bone_index("Arm.L"), {0, 1, 0}, "Pole") == 1);
  con_patch(b, bone_index("Arm.L"), 0, {{5 + 1, 2.0f}, {5 + 3, 0.4f}}); /* chain 2, pole angle */
  CHECK(cmd(b, PG_ARM_CMD_CON_ADD, bone_index("RotCopy"), {CONSTRAINT_TYPE_ROTLIKE}) == 0);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, bone_index("RotCopy"), {0, 0, 0}, "Src") == 1);
  con_patch(b, bone_index("RotCopy"), 0, {{1, CONSTRAINT_SPACE_WORLD}, {2, CONSTRAINT_SPACE_WORLD}});
  CHECK(cmd(b, PG_ARM_CMD_CON_ADD, bone_index("Follow"), {CONSTRAINT_TYPE_LOCLIKE}) == 0);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, bone_index("Follow"), {0, 0, 0}, "Src") == 1);
  con_patch(b, bone_index("Follow"), 0, {{1, CONSTRAINT_SPACE_POSE}, {2, CONSTRAINT_SPACE_POSE}});
  CHECK(cmd(b, PG_ARM_CMD_CON_ADD, bone_index("Aim"), {CONSTRAINT_TYPE_DAMPTRACK}) == 0);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, bone_index("Aim"), {0, 0, 0}, "IK.T") == 1);
  con_patch(b, bone_index("Aim"), 0, {{2, CONSTRAINT_SPACE_WORLD}, {5, TRACK_Y}});
  CHECK(cmd(b, PG_ARM_CMD_CON_ADD, bone_index("Lim"), {CONSTRAINT_TYPE_ROTLIMIT}) == 0);
  con_patch(b, bone_index("Lim"), 0, {{1, CONSTRAINT_SPACE_LOCAL}, {2, CONSTRAINT_SPACE_LOCAL}, {5, LIMIT_ZROT}, {5 + 6, -0.3f}, {5 + 7, 0.5f}});
  CHECK(cmd(b, PG_ARM_CMD_CON_ADD, bone_index("Kid"), {CONSTRAINT_TYPE_CHILDOF}) == 0);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, bone_index("Kid"), {0, 0, 0}, "Src") == 1);
  CHECK(b.select_frame_or_hold(1));
  CHECK(cmd(b, PG_ARM_CMD_CON_CHILDOF_INVERSE, bone_index("Kid"), {0, 1}) == 1); /* Set Inverse at frame 1 */

  /* drivers */
  CHECK(cmd(b, PG_ARM_CMD_PROP_ADD, -1, {0, 0, 1, 0}, "Smile") == 0);
  struct Drv { const char *path; int index; const char *expr; } drv[] = {
      {"pose.bones[\"Smile\"].location", 0, "var*0.2"},
      {"layers[\"Mouth\"].modifiers[0].offset", 0, "1+var*2.99"},
  };
  for (const Drv &d : drv) {
    const int i = cmd(b, PG_ARM_CMD_DRIVER_ADD, -1, {(float)d.index, DRIVER_TYPE_PYTHON, 0}, d.path);
    CHECK(i >= 0);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_ADD, -1, {(float)i, DVAR_TYPE_SINGLE_PROP}, "var") == 0);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_PATH, -1, {(float)i, 0}, "[\"Smile\"]") == 1);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_EXPRESSION, -1, {(float)i}, d.expr) == 1);
  }
  { /* a bone rotation driving another bone: Src's local Z rotation -> DrvBy.location[1] */
    const int i = cmd(b, PG_ARM_CMD_DRIVER_ADD, -1, {1, DRIVER_TYPE_PYTHON, 0}, "pose.bones[\"DrvBy\"].location");
    CHECK(i >= 0);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_ADD, -1, {(float)i, DVAR_TYPE_TRANSFORM_CHAN}, "var") == 0);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_TRANSFORM, -1,
              {(float)i, 0, DTAR_TRANSCHAN_ROTZ, DTAR_ROTMODE_AUTO, (float)(DTAR_FLAG_LOCALSPACE | DTAR_FLAG_LOCAL_CONSTS)}, "Src") == 1);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_EXPRESSION, -1, {(float)i}, "var*0.1") == 1);
  }

  /* evaluate at each frame: PG and the optional Blender result */
  const PGArmature *arm = b.armature(false);
  CHECK(arm != nullptr);
  struct FrameResult { std::vector<PgBone> bones; std::vector<std::vector<std::vector<StrokePoint>>> strokes; std::vector<int> frame_no; };
  FrameResult res[3];
  for (int fi = 0; fi < 3; fi++) {
    const int f = FRAMES[fi];
    CHECK(b.select_layer(0) && b.select_frame_or_hold(f));
    CHECK(cmd(b, PG_ARM_CMD_PROP_SET, -1, {0, SMILE[fi]}) == 1);
    b.armature_evaluate((float)f);
    for (int i = 0; i < NBONES; i++) {
      PgBone pb;
      CHECK(pg_bone(b, i, &pb));
      res[fi].bones.push_back(pb);
    }
    for (int li = 0; li < 2; li++) {
      bGPDlayer *gpl = layer_at(b, li);
      const bGPDframe *cur = frame_at(gpl, f);
      const bGPDframe *ev = cur ? b.evaluated_frame(gpl, cur, f) : nullptr;
      res[fi].frame_no.push_back(ev ? ev->framenum : -1);
      res[fi].strokes.emplace_back();
      if (!ev) continue;
      for (const bGPDstroke *s = (const bGPDstroke *)ev->strokes.first; s; s = s->next) {
        std::vector<StrokePoint> pts(s->totpoints);
        for (int k = 0; k < s->totpoints; k++) {
          pts[k] = StrokePoint();
          pts[k].x = s->points[k].x; pts[k].y = s->points[k].y; pts[k].z = s->points[k].z;
          pts[k].pressure = s->points[k].pressure; pts[k].strength = s->points[k].strength;
        }
        res[fi].strokes.back().push_back(std::move(pts));
      }
    }
  }

  if (!run_path) {
    /* ---- self-consistency: results worked out by hand ---- */
    const int root = bone_index("Root"), spine = bone_index("Spine"), src = bone_index("Src"), follow = bone_index("Follow");
    const int aim = bone_index("Aim"), ikt = bone_index("IK.T"), lim = bone_index("Lim"), rc = bone_index("RotCopy");
    for (int fi = 0; fi < 3; fi++) {
      for (int i = 0; i < NBONES; i++) for (float v : res[fi].bones[i].m) CHECK(std::isfinite(v));
      /* unkeyed Root stays at its rest: head (500,800), tail (500,600) */
      NEAR(res[fi].bones[root].head[0], 500, 0.01); NEAR(res[fi].bones[root].head[1], 800, 0.01);
      NEAR(res[fi].bones[root].tail[1], 600, 0.01);
      /* Copy Location in Pose space onto Src: the head lands on Src's head */
      for (int k = 0; k < 3; k++) NEAR(res[fi].bones[follow].head[k], res[fi].bones[src].head[k], 0.01);
      /* Copy Rotation in World space: Follow-like check on the rotation of RotCopy == Src */
      for (int k = 0; k < 16; k++) if (k % 4 != 3 && k < 11) NEAR(res[fi].bones[rc].m[k], res[fi].bones[src].m[k], 1e-5);
      /* Damped Track Y at IK.T's head */
      double d[3], e[3];
      for (int k = 0; k < 3; k++) {
        d[k] = res[fi].bones[aim].tail[k] - res[fi].bones[aim].head[k];
        e[k] = res[fi].bones[ikt].head[k] - res[fi].bones[aim].head[k];
      }
      const double dn = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]), en = std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
      NEAR((d[0] * e[0] + d[1] * e[1] + d[2] * e[2]) / (dn * en), 1.0, 1e-4);
      /* Limit Rotation Z: key 0 at frame 1, 1.2 at frame 10 clamped to 0.5 (the bone points +y: tail = head + L (-sin, cos)) */
      const double th = FRAMES[fi] == 1 ? 0.0 : FRAMES[fi] == 10 ? 0.5 : -1.0;
      if (th >= 0) {
        NEAR(res[fi].bones[lim].tail[0] - res[fi].bones[lim].head[0], 100.0 * -std::sin(th), 0.02);
        NEAR(-(res[fi].bones[lim].tail[1] - res[fi].bones[lim].head[1]), 100.0 * std::cos(th), 0.02);
      }
      else {
        const double dx = res[fi].bones[lim].tail[0] - res[fi].bones[lim].head[0];
        CHECK(dx < 0 && dx > -100.0 * std::sin(0.5) - 0.02);
      }
    }
    /* Spine key 0.35 at frame 10 (the pivot is its head): tail = head + 200 (-sin, cos) in arm space */
    NEAR(res[2].bones[spine].tail[0] - res[2].bones[spine].head[0], 200.0 * -std::sin(0.35), 0.02);
    NEAR(-(res[2].bones[spine].tail[1] - res[2].bones[spine].head[1]), 200.0 * std::cos(0.35), 0.02);
    /* the Armature modifier moved the drawing (frame 1 pose is not the rest pose) and the Time Offset swapped the mouth */
    CHECK(res[0].frame_no[1] == 1 && res[1].frame_no[1] == 2 && res[2].frame_no[1] == 3);
    CHECK(res[2].strokes[0].size() == 3 && res[2].strokes[0][0].size() == 10);
    const StrokePoint &moved = res[2].strokes[0][0][9];
    CHECK(std::fabs(moved.x - body[0][9].first) > 1.0 || std::fabs(moved.y - body[0][9].second) > 1.0);
    /* repeatable: evaluating frame 10 again changes nothing */
    PgBone again;
    CHECK(b.select_frame_or_hold(10));
    b.armature_evaluate(10.0f);
    CHECK(pg_bone(b, bone_index("Arm.L"), &again));
    for (int k = 0; k < 16; k++) NEAR(again.m[k], res[2].bones[bone_index("Arm.L")].m[k], 1e-6);
    std::printf("rig self-check: %d checks, %d failures (set RIG_BLENDER_RUN=<file> to compare with Blender)\n", checks, failures);
    b.shutdown();
    return failures ? 1 : 0;
  }

  /* ---- comparison with Blender ---- */
  std::ofstream rep_file;
  if (const char *rp = std::getenv("RIG_BLENDER_REPORT")) { rep_file.open(rp); report = &rep_file; }
  const Json *rig = ref.get("rig");
  const Json *names = rig ? rig->get("bones") : nullptr;
  CHECK(names && (int)names->a.size() == NBONES);
  if (names) for (int i = 0; i < NBONES && i < (int)names->a.size(); i++) CHECK(names->a[i].s == BONES[i].name);

  /* weights */
  {
    const Json *w = ref.get("weights");
    double worst = 0;
    std::string worst_at;
    bool structure = w != nullptr;
    int n = 0;
    for (const auto &kv : w ? w->o : std::vector<std::pair<std::string, Json>>()) {
      auto it = pgw.find(kv.first);
      if (it == pgw.end()) { structure = false; break; }
      for (const auto &g : kv.second.o) {
        auto gi = it->second.find(g.first);
        if (gi == it->second.end() || gi->second.size() != g.second.a.size()) { structure = false; break; }
        for (size_t i = 0; i < g.second.a.size(); i++) {
          const double d = std::fabs(gi->second[i] - g.second.a[i].n);
          n++;
          if (d > worst) { worst = d; worst_at = kv.first + " " + g.first + " point " + std::to_string(i); }
        }
      }
    }
    const bool ok = structure && n > 0 && worst <= 1e-4;
    say("%s BLENDER-RUN weights after bind: %s, %d weights, max diff %.3g (%s)\n", ok ? "  ok" : "FAIL",
        structure ? "same groups / strokes / points" : "STRUCTURE DIFFERS", n, worst, worst_at.c_str());
    compared++;
    if (!ok) run_failures++;
  }

  const Json *frames = ref.get("frames");
  for (int fi = 0; fi < 3; fi++) {
    const Json *fj = frames ? frames->get(std::to_string(FRAMES[fi]).c_str()) : nullptr;
    if (!fj) { say("FAIL BLENDER-RUN frame %d missing in the Blender run\n", FRAMES[fi]); run_failures++; continue; }
    /* pose bones */
    {
      const Json *bj = fj->get("bones");
      double worst_m = 0, worst_p = 0;
      std::string worst_m_at, worst_p_at, bad;
      int n = 0;
      for (int i = 0; i < NBONES; i++) {
        const Json *bone = bj ? bj->get(BONES[i].name) : nullptr;
        if (!bone) { bad += std::string(" ") + BONES[i].name + "(missing)"; continue; }
        const Json *mj = bone->get("matrix"), *hj = bone->get("head"), *tj = bone->get("tail");
        double dm = 0, dp = 0;
        for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) dm = std::max(dm, std::fabs(res[fi].bones[i].m[c * 4 + r] - mj->a[r].a[c].n));
        for (int k = 0; k < 3; k++) {
          double hp[3], tp[3], hv[3] = {hj->a[0].n, hj->a[1].n, hj->a[2].n}, tv[3] = {tj->a[0].n, tj->a[1].n, tj->a[2].n};
          to_px(hv, hp); to_px(tv, tp);
          dp = std::max(dp, std::max(std::fabs(res[fi].bones[i].head[k] - hp[k]), std::fabs(res[fi].bones[i].tail[k] - tp[k])));
        }
        n++;
        if (dm > worst_m) { worst_m = dm; worst_m_at = BONES[i].name; }
        if (dp > worst_p) { worst_p = dp; worst_p_at = BONES[i].name; }
        if (dm > 1e-5 || dp > 0.01) {
          char line[160];
          std::snprintf(line, sizeof(line), " %s(matrix %.3g, head/tail %.3g px)", BONES[i].name, dm, dp);
          bad += line;
        }
      }
      const bool ok = n == NBONES && worst_m <= 1e-5 && worst_p <= 0.01;
      say("%s BLENDER-RUN frame %d pose bones: %d bones, max matrix diff %.3g (%s), max head/tail diff %.3g px (%s)\n",
          ok ? "  ok" : "FAIL", FRAMES[fi], n, worst_m, worst_m_at.c_str(), worst_p, worst_p_at.c_str());
      if (!bad.empty()) say("       differing bones:%s\n", bad.c_str());
      compared++;
      if (!ok) run_failures++;
    }
    /* evaluated strokes */
    {
      const Json *sj = fj->get("strokes");
      double worst_pos = 0, worst_val = 0;
      bool structure = sj != nullptr;
      int n = 0;
      std::string why;
      for (int li = 0; li < 2 && structure; li++) {
        const Json *lj = sj->get(layer_names[li]);
        if (!lj) { structure = false; why = "layer missing"; break; }
        const Json *fnj = lj->get("frame");
        if (!fnj || (int)fnj->n != res[fi].frame_no[li]) {
          structure = false;
          why = std::string(layer_names[li]) + " shows frame " + std::to_string(res[fi].frame_no[li]) + ", Blender " + std::to_string(fnj ? (int)fnj->n : -1);
          break;
        }
        const Json *strokes = lj->get("strokes");
        if (strokes->a.size() != res[fi].strokes[li].size()) { structure = false; why = std::string(layer_names[li]) + " stroke count"; break; }
        for (size_t si = 0; si < strokes->a.size() && structure; si++) {
          const Json &s = strokes->a[si];
          const auto &pg = res[fi].strokes[li][si];
          if (s.a.size() != pg.size()) { structure = false; why = std::string(layer_names[li]) + " point count"; break; }
          for (size_t k = 0; k < pg.size(); k++) {
            const double bv[3] = {s.a[k].a[0].n, s.a[k].a[1].n, s.a[k].a[2].n};
            double bp[3];
            to_px(bv, bp);
            worst_pos = std::max(worst_pos, std::max(std::fabs(pg[k].x - bp[0]), std::max(std::fabs(pg[k].y - bp[1]), std::fabs(pg[k].z - bp[2]))));
            worst_val = std::max(worst_val, std::max(std::fabs(pg[k].pressure - s.a[k].a[3].n), std::fabs(pg[k].strength - s.a[k].a[4].n)));
            n++;
          }
        }
      }
      const bool ok = structure && n > 0 && worst_pos <= 0.01 && worst_val <= 1e-4;
      say("%s BLENDER-RUN frame %d evaluated strokes: %s, %d points, max position diff %.3g px, max pressure/strength diff %.3g\n",
          ok ? "  ok" : "FAIL", FRAMES[fi], structure ? "same layers / frames / strokes / points" : ("STRUCTURE DIFFERS (" + why + ")").c_str(), n,
          worst_pos, worst_val);
      compared++;
      if (!ok) run_failures++;
    }
  }
  say("rig vs Blender: %d cases compared, %d differ\n", compared, run_failures);
  if (compared == 0) { std::printf("FAIL: no case was compared\n"); failures++; }
  failures += run_failures;
  b.shutdown();
  std::printf("rig blender parity: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
