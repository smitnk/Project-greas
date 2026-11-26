/* SPDX-License-Identifier: GPL-2.0-or-later
 * .blend open / save (project_grease_blend_io.cc) against real Blender 3.6.23
 * (tools/blend_io_reference/blender_blend_io_reference.py, tools/run_native_blend_io_tests.sh):
 *   import REF.blend REF.json OUTDIR   Blender's file -> project JSON, compared with Blender's own dump of it
 *                                      (strokes, points, weights, materials, layers, modifiers, bones, pose,
 *                                      F-Curves); the rig also through the app's pg_arm_from_json. Then the
 *                                      JSON is saved as OUTDIR/app.blend (and OUTDIR/app.project.json).
 *   compare REF.json APP.json          Blender's dump of the app's file against the original dump.
 *   self OUTDIR                        no Blender: a project saved, read back and compared.
 * The test includes the implementation to reach its JSON parser. */
#include "../project_grease_blend_io.cc"

#include <cfloat>
#include <fstream>
#include <sstream>

static int g_fail = 0, g_checks = 0;
static double g_max_co = 0, g_max_mat = 0;
#define CHECK(c, ...)                                         \
  do {                                                        \
    g_checks++;                                               \
    if (!(c)) {                                               \
      g_fail++;                                               \
      if (g_fail < 40) {                                      \
        std::fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
        std::fprintf(stderr, __VA_ARGS__);                    \
        std::fprintf(stderr, "\n");                           \
      }                                                       \
    }                                                         \
  } while (0)

static std::string slurp(const std::string &p)
{
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
static JV parse(const std::string &text)
{
  JV v;
  JP p{text.c_str()};
  if (!p.parse(v)) { std::fprintf(stderr, "bad json\n"); std::exit(2); }
  return v;
}
static float F(const JV *v, size_t i = 0) { return v ? (float)(v->kind == JV::ARR ? v->at(i, NAN) : v->num) : NAN; }
static float F(const JV &v, const char *k) { return F(v.get(k)); }
static bool B(const JV &v, const char *k) { const JV *x = v.get(k); return x && x->num != 0; }
static int enum_index(const std::string &s, std::initializer_list<const char *> names)
{
  int i = 0;
  for (const char *n : names) { if (s == n) return i; i++; }
  return -1;
}

/* project JSON (app) vs Blender's dump of the same file: exact, the px conversion done in float */
static void compare_import(const JV &app, const JV &ref)
{
  CHECK(app.n("fps", 0) == ref.n("fps", -1), "fps");
  CHECK(app.n("frameEnd", 0) == ref.n("frame_end", -1), "frame end");
  const auto &mats = app.list("materials");
  const auto &rmats = ref.list("materials");
  CHECK(mats.size() == rmats.size(), "material count %zu %zu", mats.size(), rmats.size());
  for (size_t i = 0; i < MIN2(mats.size(), rmats.size()); i++) {
    const JV &m = mats[i], &r = rmats[i];
    CHECK(m.s("name") == r.s("name"), "material name");
    for (int c = 0; c < 4; c++) {
      CHECK(F(m.get("stroke"), c) == F(r.get("color"), c), "material %zu stroke %d", i, c);
      CHECK(F(m.get("fill"), c) == F(r.get("fill_color"), c), "material %zu fill %d", i, c);
    }
    CHECK(B(m, "strokeEnabled") == B(r, "show_stroke") && B(m, "fillEnabled") == B(r, "show_fill"), "material show");
    CHECK(B(m, "visible") == !B(r, "hide") && B(m, "locked") == B(r, "lock"), "material hide/lock");
    CHECK(m.n("mode", -1) == enum_index(r.s("mode"), {"LINE", "DOTS", "BOX"}), "material mode");
    CHECK(m.n("pass", -1) == r.n("pass_index", -2), "material pass");
  }
  std::vector<std::string> groups;
  for (const JV &g : app.list("vertexGroups")) groups.push_back(g.str);
  std::vector<std::string> rgroups;
  for (const JV &g : ref.list("vertex_groups")) rgroups.push_back(g.str);
  CHECK(groups == rgroups, "vertex groups");
  const auto &layers = app.list("layers");
  const auto &rlayers = ref.list("layers");
  CHECK(layers.size() == rlayers.size(), "layer count");
  int strokes = 0, points = 0, weights = 0;
  for (size_t li = 0; li < MIN2(layers.size(), rlayers.size()); li++) {
    const JV &l = layers[li], &r = rlayers[li];
    CHECK(l.s("name") == r.s("info"), "layer name");
    CHECK(F(l, "opacity") == F(r, "opacity"), "layer opacity");
    CHECK(l.n("blend", -1) == enum_index(r.s("blend_mode"), {"REGULAR", "HARDLIGHT", "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE"}), "blend");
    for (int c = 0; c < 4; c++) CHECK(F(l.get("tint"), c) == F(r.get("tint"), c), "tint %d", c);
    CHECK(l.n("lineChange", 0) == r.n("line_change", 1) && l.n("pass", 0) == r.n("pass_index", 1), "line change / pass");
    CHECK(B(l, "visible") == !B(r, "hide") && B(l, "locked") == B(r, "lock"), "layer hide/lock");
    CHECK(B(l, "useMask") == B(r, "use_mask"), "use mask");
    const auto &masks = l.list("masks");
    CHECK(masks.size() == r.list("masks").size(), "mask count");
    for (size_t k = 0; k < MIN2(masks.size(), r.list("masks").size()); k++)
      CHECK(masks[k].s("name") == r.list("masks")[k].str, "mask name");
    const auto &frames = l.list("frames");
    const auto &rframes = r.list("frames");
    CHECK(frames.size() == rframes.size(), "frame count");
    for (size_t fi = 0; fi < MIN2(frames.size(), rframes.size()); fi++) {
      const JV &f = frames[fi], &rf = rframes[fi];
      CHECK(f.n("number", -1) == rf.n("number", -2), "frame number");
      CHECK(f.n("keyType", -1) == enum_index(rf.s("key"), {"KEYFRAME", "EXTREME", "BREAKDOWN", "JITTER", "MOVING_HOLD"}), "key type");
      const auto &ss = f.list("strokes");
      const auto &rs = rf.list("strokes");
      CHECK(ss.size() == rs.size(), "stroke count");
      for (size_t si = 0; si < MIN2(ss.size(), rs.size()); si++) {
        const JV &s = ss[si], &q = rs[si];
        strokes++;
        CHECK(s.n("thickness", -1) == q.n("line_width", -2), "line width");
        CHECK(s.n("material", -1) == q.n("material_index", -2), "material index");
        CHECK(B(s, "cyclic") == B(q, "cyclic"), "cyclic");
        CHECK(F(s, "hardness") == F(q, "hardness"), "hardness");
        for (int c = 0; c < 2; c++)
          CHECK(F(s.get("caps"), c) == enum_index(q.get("caps")->arr[c].str, {"ROUND", "FLAT"}), "caps");
        for (int c = 0; c < 4; c++) CHECK(F(s.get("fillColor"), c) == F(q.get("fill"), c), "fill vertex color");
        std::map<int, std::map<int, float>> w;
        for (const JV &row : s.list("weights"))
          for (size_t k = 1; k + 1 < row.arr.size(); k += 2) w[(int)row.at(0, -1)][(int)row.at(k, -1)] = (float)row.at(k + 1, NAN);
        const auto &ps = s.list("points");
        const auto &rp = q.list("points");
        CHECK(ps.size() == rp.size(), "point count");
        for (size_t pi = 0; pi < MIN2(ps.size(), rp.size()); pi++) {
          const JV &p = ps[pi], &rq = rp[pi];
          points++;
          const JV *co = rq.get("co");
          CHECK(F(&p, 0) == F(co, 0) * PPU && F(&p, 1) == -F(co, 1) * PPU && F(&p, 2) == F(co, 2) * PPU,
                "point %zu co %g %g", pi, F(&p, 0), F(co, 0) * PPU);
          CHECK(F(&p, 3) == F(rq, "pressure") && F(&p, 4) == F(rq, "strength"), "pressure / strength");
          for (int c = 0; c < 4; c++) CHECK(F(&p, 6 + c) == F(rq.get("vertex_color"), c), "vertex color");
          const JV *rw = rq.get("weights");
          size_t n = 0;
          for (const auto &kv : rw ? rw->obj : std::map<std::string, JV>()) {
            const int g = vgroup_index(groups, kv.first.c_str());
            CHECK(w[(int)pi].count(g) && w[(int)pi][g] == (float)kv.second.num, "weight %s", kv.first.c_str());
            n++;
            weights++;
          }
          CHECK(n == w[(int)pi].size(), "weight count");
        }
      }
    }
    /* modifiers: Blender's per object (layer filter), the app's per layer */
    std::vector<const JV *> want;
    for (const JV &m : ref.list("modifiers"))
      if (m.s("layer").empty() || m.s("layer") == l.s("name")) want.push_back(&m);
    const auto &mods = l.list("modifiers");
    CHECK(mods.size() == want.size(), "layer %zu modifier count %zu %zu", li, mods.size(), want.size());
    for (size_t k = 0; k < MIN2(mods.size(), want.size()); k++) {
      const JV &m = mods[k], &r2 = *want[k];
      const JV *p = m.get("params");
      CHECK(B(m, "enabled") == B(r2, "show"), "modifier enabled");
      if (r2.s("type") == "GP_THICK") {
        CHECK(m.n("type", 0) == PG_MOD_THICKNESS && F(p, PG_P_THICK_FACTOR) == F(r2, "factor") &&
              F(p, PG_P_THICK_THICKNESS) == F(r2, "thickness") && (F(p, PG_P_THICK_NORMALIZE) != 0) == B(r2, "normalize"), "thickness");
      }
      else if (r2.s("type") == "GP_TINT") {
        CHECK(m.n("type", 0) == PG_MOD_TINT && F(p, PG_P_TINT_FACTOR) == F(r2, "factor") &&
              F(p, PG_P_TINT_MODE) == enum_index(r2.s("mode"), {"STROKE", "FILL", "BOTH"}), "tint");
        for (int c = 0; c < 3; c++) CHECK(F(p, PG_P_TINT_R + c) == F(r2.get("color"), c), "tint color");
      }
      else if (r2.s("type") == "GP_ARMATURE") {
        CHECK(m.n("type", 0) == PG_MOD_ARMATURE && (F(p, PG_P_ARMATURE_USE_VGROUPS) != 0) == B(r2, "vgroups") &&
              (F(p, PG_P_ARMATURE_USE_ENVELOPE) != 0) == B(r2, "envelope") &&
              (F(p, PG_P_ARMATURE_PRESERVE_VOLUME) != 0) == B(r2, "volume"), "armature modifier");
      }
    }
  }
  std::printf("BLEND-IO import: %d strokes, %d points, %d weights compared exactly\n", strokes, points, weights);

  /* rig: the JSON, then the app's own reading of it */
  const JV *arm = app.get("armature");
  const JV *rarm = ref.get("armature");
  CHECK((arm != nullptr) == (rarm != nullptr), "armature presence");
  if (!arm || !rarm) return;
  const auto &bones = arm->list("bones");
  const auto &rb = rarm->list("bones");
  CHECK(bones.size() == rb.size(), "bone count");
  for (size_t i = 0; i < MIN2(bones.size(), rb.size()); i++) {
    const JV &b = bones[i], &r = rb[i];
    CHECK(b.s("name") == r.s("name"), "bone name");
    const int parent = (int)b.n("parent", -1);
    CHECK((parent < 0 ? std::string() : bones[parent].s("name")) == r.s("parent"), "bone parent");
    for (int c = 0; c < 3; c++)
      CHECK(F(b.get("head"), c) == F(r.get("head"), c) && F(b.get("tail"), c) == F(r.get("tail"), c), "bone %s head/tail", r.s("name").c_str());
  }
  const auto &pose = arm->list("pose");
  const auto &rpose = rarm->list("pose");
  CHECK(pose.size() == rpose.size(), "pose count");
  for (size_t i = 0; i < MIN2(pose.size(), rpose.size()); i++) {
    CHECK(pose[i].s("name") == rpose[i].s("name"), "pose name");
    for (int c = 0; c < 4; c++) CHECK(F(pose[i].get("quat"), c) == F(rpose[i].get("quat"), c), "pose quat");
    for (int c = 0; c < 3; c++)
      CHECK(F(pose[i].get("loc"), c) == F(rpose[i].get("loc"), c) && F(pose[i].get("scale"), c) == F(rpose[i].get("scale"), c), "pose loc/scale");
  }
  const auto &fc = arm->list("fcurves");
  const auto &rfc = rarm->list("fcurves");
  CHECK(fc.size() == rfc.size(), "fcurve count %zu %zu", fc.size(), rfc.size());
  int keys = 0;
  for (size_t i = 0; i < MIN2(fc.size(), rfc.size()); i++) {
    CHECK(fc[i].s("path") == rfc[i].s("path") && fc[i].n("index", -1) == rfc[i].n("index", -2), "fcurve path");
    const auto &k = fc[i].list("keys");
    const auto &rk = rfc[i].list("keys");
    CHECK(k.size() == rk.size(), "key count");
    for (size_t j = 0; j < MIN2(k.size(), rk.size()); j++) {
      keys++;
      /* app [x, y, ipo, h1, h2, lx, ly, rx, ry, ...], Blender [x, y, lx, ly, rx, ry, ipo] */
      const int map[6] = {0, 1, 5, 6, 7, 8};
      for (int c = 0; c < 6; c++) CHECK(F(&k[j], map[c]) == F(&rk[j], c), "key %zu value %d", j, c);
      CHECK(F(&k[j], 2) == enum_index(rk[j].arr[6].str, {"CONSTANT", "LINEAR", "BEZIER"}), "key ipo");
    }
  }
  /* pg_arm_from_json: the rig the app builds from this JSON (rest bones through ED_armature_from_edit) */
  std::string sub;
  {
    std::function<void(const JV &)> dump = [&](const JV &v) {
      switch (v.kind) {
        case JV::NUL: sub += "null"; break;
        case JV::BOOL: sub += v.num ? "true" : "false"; break;
        case JV::NUM: put_num(sub, v.num); break;
        case JV::STR: put_str(sub, v.str.c_str()); break;
        case JV::ARR: sub += '['; for (size_t i = 0; i < v.arr.size(); i++) { if (i) sub += ','; dump(v.arr[i]); } sub += ']'; break;
        case JV::OBJ: { sub += '{'; bool f = true; for (const auto &kv : v.obj) { if (!f) sub += ','; f = false; put_str(sub, kv.first.c_str()); sub += ':'; dump(kv.second); } sub += '}'; }
      }
    };
    dump(*arm);
  }
  PGArmature *a = pg_arm_from_json(sub.c_str());
  CHECK(a != nullptr, "pg_arm_from_json");
  if (!a) return;
  CHECK(pg_arm_bone_count(a) == (int)rb.size(), "app bone count");
  for (size_t i = 0; i < rb.size(); i++) {
    const int bi = pg_arm_bone_find(a, rb[i].s("name").c_str());
    float info[PG_ARM_BONE_INFO];
    CHECK(bi >= 0 && pg_arm_bone_info(a, bi, info), "app bone %s", rb[i].s("name").c_str());
    if (bi < 0) continue;
    for (int c = 0; c < 3; c++) CHECK(info[c] == F(rb[i].get("head"), c) && info[3 + c] == F(rb[i].get("tail"), c), "app bone head/tail");
    const auto &rows = rb[i].get("matrix")->arr;
    for (int r = 0; r < 4; r++)
      for (int c = 0; c < 4; c++) {
        const double d = std::fabs((double)info[10 + c * 4 + r] - F(&rows[r], c)); /* arm_mat[col][row] */
        g_max_mat = MAX2(g_max_mat, d);
        CHECK(d <= 1e-6, "app bone %s arm_mat[%d][%d] %g", rb[i].s("name").c_str(), c, r, d);
      }
  }
  pg_arm_free(a);
  std::printf("BLEND-IO import: %zu bones (head/tail exact, arm_mat max |d| %.3g), %zu pose bones, %zu F-Curves / %d keys exact\n",
              rb.size(), g_max_mat, rpose.size(), rfc.size(), keys);
}

/* Blender's dump of the app's file vs the original dump. Exact except the GP point coordinates, which
 * went through canvas px: float(x * 1000) / 1000 is within one float step of x. */
static bool close_ulp(float a, float b) { return a == b || std::nextafter(a, b) == b; }
static void compare_dumps(const JV &a, const JV &b, const std::string &path, bool co)
{
  if (a.kind != b.kind) { CHECK(false, "%s: kind", path.c_str()); return; }
  switch (a.kind) {
    case JV::NUM:
      if (co) {
        g_max_co = MAX2(g_max_co, std::fabs(a.num - b.num));
        CHECK(close_ulp((float)a.num, (float)b.num), "%s: %.9g vs %.9g", path.c_str(), a.num, b.num);
      }
      else CHECK((float)a.num == (float)b.num, "%s: %.9g vs %.9g", path.c_str(), a.num, b.num);
      break;
    case JV::STR: CHECK(a.str == b.str, "%s: '%s' vs '%s'", path.c_str(), a.str.c_str(), b.str.c_str()); break;
    case JV::BOOL: CHECK(a.num == b.num, "%s", path.c_str()); break;
    case JV::ARR:
      CHECK(a.arr.size() == b.arr.size(), "%s: size %zu vs %zu", path.c_str(), a.arr.size(), b.arr.size());
      for (size_t i = 0; i < MIN2(a.arr.size(), b.arr.size()); i++)
        compare_dumps(a.arr[i], b.arr[i], path + "[" + std::to_string(i) + "]", co);
      break;
    case JV::OBJ:
      for (const auto &kv : a.obj) {
        if (kv.first == "blender") continue;
        const JV *o = b.get(kv.first.c_str());
        CHECK(o != nullptr, "%s.%s missing", path.c_str(), kv.first.c_str());
        if (!o) continue;
        const bool is_co = co || kv.first == "co";
        if (kv.first == "matrix") { /* arm_mat: recomputed by the app's ED_armature_from_edit */
          for (size_t r = 0; r < kv.second.arr.size(); r++)
            for (size_t c = 0; c < kv.second.arr[r].arr.size(); c++) {
              const double d = std::fabs(kv.second.arr[r].arr[c].num - o->arr[r].arr[c].num);
              g_max_mat = MAX2(g_max_mat, d);
              CHECK(d <= 1e-6, "%s.matrix %g", path.c_str(), d);
            }
          continue;
        }
        compare_dumps(kv.second, *o, path + "." + kv.first, is_co);
      }
      CHECK(a.obj.size() == b.obj.size(), "%s: key count", path.c_str());
      break;
    default: break;
  }
}

static bool write_file(const std::string &p, const void *d, size_t n)
{
  FILE *f = std::fopen(p.c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(d, 1, n, f) == n;
  return std::fclose(f) == 0 && ok;
}

static std::string blend_to_json(const std::string &bytes)
{
  char err[256] = "";
  char *j = pg_blend_to_project_json(bytes.data(), bytes.size(), err, sizeof(err));
  CHECK(j != nullptr, "import: %s", err);
  std::string out = j ? j : "";
  std::free(j);
  return out;
}

static std::string json_to_blend(const std::string &json)
{
  char err[256] = "";
  size_t n = 0;
  void *b = pg_project_json_to_blend(json.c_str(), &n, err, sizeof(err));
  CHECK(b != nullptr, "export: %s", err);
  std::string out(static_cast<char *>(b ? b : (void *)""), b ? n : 0);
  std::free(b);
  return out;
}

/* without Blender: save a project, open the file, the same project comes back (and bad input fails) */
static void self_test(const std::string &dir)
{
  const std::string project =
      "{\"version\":6,\"fps\":24,\"frame\":1,\"frameEnd\":30,"
      "\"materials\":[{\"stroke\":[0,0,0,1],\"fill\":[1,0,0,1],\"visible\":true,\"fillEnabled\":true,\"name\":\"Ink\"}],"
      "\"vertexGroups\":[\"Root\",\"Tip\"],\"activeVertexGroup\":0,"
      "\"layers\":[{\"index\":0,\"name\":\"L\",\"visible\":true,\"locked\":false,\"opacity\":0.5,"
      "\"frames\":[{\"number\":1,\"strokes\":[{\"points\":[[100,200,0,1,1,0,0,0,0,0],[300,250,0,0.5,0.8,0,1,0,0,0.5]],"
      "\"material\":0,\"thickness\":12,\"cyclic\":false,\"fillOpacity\":1,\"fillColor\":[0,0,0,0],"
      "\"weights\":[[0,0,1],[1,0,0.25,1,0.75]]}]},{\"number\":4,\"keyType\":1,\"strokes\":[]}],"
      "\"modifiers\":[{\"type\":24,\"enabled\":true,\"params\":[1,0,0,0,0]},{\"type\":1,\"enabled\":false,\"params\":[0,30,2]}]}],"
      "\"armature\":{\"version\":1,\"mode\":0,\"active\":-1,\"bones\":[{\"name\":\"Root\",\"parent\":-1,\"head\":[0.1,-0.2,0],"
      "\"tail\":[0.1,-0.1,0],\"roll\":0},{\"name\":\"Tip\",\"parent\":0,\"head\":[0.1,-0.1,0],\"tail\":[0.3,-0.1,0],\"roll\":0.2,\"flag\":16}],"
      "\"pose\":[{\"name\":\"Tip\",\"loc\":[0,0.01,0],\"quat\":[0.9,0,0,0.43588989],\"scale\":[1,1,1]}],"
      "\"fcurves\":[{\"path\":\"pose.bones[\\\"Tip\\\"].location\",\"index\":1,\"keys\":[[1,0,2,5,5,0,0,2,0],[9,0.01,1,5,5,8,0.01,10,0.01]]}]}}";
  const std::string blend = json_to_blend(project);
  CHECK(blend.size() > 1000 && blend.compare(0, 12, "BLENDER-v306") == 0, "header");
  write_file(dir + "/self.blend", blend.data(), blend.size());
  const JV back = parse(blend_to_json(blend));
  const JV in = parse(project);
  CHECK(back.n("fps", 0) == 24 && back.n("frameEnd", 0) == 30, "scene");
  CHECK(back.list("materials").size() == 1 && back.list("materials")[0].s("name") == "Ink" &&
        B(back.list("materials")[0], "fillEnabled"), "material");
  const JV &l = back.list("layers")[0];
  CHECK(l.s("name") == "L" && F(l, "opacity") == 0.5f && l.list("frames").size() == 2, "layer");
  CHECK(l.list("frames")[1].n("keyType", 0) == 1, "key type");
  const JV &s = l.list("frames")[0].list("strokes")[0];
  const JV &si = in.list("layers")[0].list("frames")[0].list("strokes")[0];
  for (size_t p = 0; p < 2; p++)
    for (int c = 0; c < 10; c++) {
      const float want = F(&si.list("points")[p], c);
      CHECK(close_ulp(F(&s.list("points")[p], c), want), "point %zu %d %g %g", p, c, F(&s.list("points")[p], c), want);
    }
  CHECK(s.n("thickness", 0) == 12 && s.list("weights").size() == 2 && F(&s.list("weights")[1], 4) == 0.75f, "stroke");
  const auto &mods = l.list("modifiers");
  CHECK(mods.size() == 2 && mods[0].n("type", 0) == 24 && mods[1].n("type", 0) == 1 && !B(mods[1], "enabled") &&
        F(mods[1].get("params"), 2) == 2.0f, "modifiers");
  const JV *arm = back.get("armature");
  CHECK(arm && arm->list("bones").size() == 2 && arm->list("bones")[1].n("parent", -1) == 0 &&
        F(arm->list("bones")[1].get("tail"), 0) == 0.3f && F(arm->list("bones")[1], "roll") == 0.2f, "bones");
  CHECK(arm && arm->list("pose").size() == 2 && F(arm->list("pose")[1].get("quat"), 3) == 0.43588989f, "pose");
  CHECK(arm && arm->list("fcurves").size() == 1 && arm->list("fcurves")[0].list("keys").size() == 2 &&
        F(&arm->list("fcurves")[0].list("keys")[1], 1) == 0.01f, "fcurves");
  /* trust boundary: garbage, truncation and a project without layers */
  char err[256];
  CHECK(!pg_blend_to_project_json("nope", 4, err, sizeof(err)), "garbage rejected");
  CHECK(!pg_blend_to_project_json(blend.data(), 40, err, sizeof(err)), "truncated rejected");
  size_t n = 0;
  CHECK(!pg_project_json_to_blend("[1,2", &n, err, sizeof(err)), "bad json rejected");
  void *empty = pg_project_json_to_blend("{}", &n, err, sizeof(err));
  CHECK(empty != nullptr, "empty project saves");
  std::free(empty);
}

int main(int argc, char **argv)
{
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "import" && argc == 5) {
    const std::string bytes = slurp(argv[2]);
    const std::string json = blend_to_json(bytes);
    compare_import(parse(json), parse(slurp(argv[3])));
    const std::string out = argv[4];
    write_file(out + "/app.project.json", json.data(), json.size());
    const std::string blend = json_to_blend(json);
    CHECK(write_file(out + "/app.blend", blend.data(), blend.size()), "write app.blend");
  }
  else if (mode == "compare" && argc == 4) {
    compare_dumps(parse(slurp(argv[2])), parse(slurp(argv[3])), "", false);
    std::printf("BLEND-IO export: Blender's dump of the app's file vs the original: GP co max |d| %.3g (<= 1 float step), arm_mat max |d| %.3g, everything else exact\n",
                g_max_co, g_max_mat);
  }
  else if (mode == "save" && argc == 4) { /* tool: project JSON -> .blend */
    const std::string blend = json_to_blend(slurp(argv[2]));
    CHECK(write_file(argv[3], blend.data(), blend.size()), "write");
  }
  else if (mode == "open" && argc == 4) { /* tool: .blend -> project JSON */
    const std::string json = blend_to_json(slurp(argv[2]));
    CHECK(write_file(argv[3], json.data(), json.size()), "write");
  }
  else if (mode == "self" && argc == 3) {
    self_test(argv[2]);
  }
  else {
    std::fprintf(stderr, "usage: import REF.blend REF.json OUTDIR | compare REF.json APP.json | self OUTDIR\n");
    return 2;
  }
  std::printf("BLEND-IO %s: %d checks, %d failed\n", mode.c_str(), g_checks, g_fail);
  return g_fail ? 1 : 0;
}
