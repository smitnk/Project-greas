/* SPDX-License-Identifier: GPL-2.0-or-later
 * Project document JSON of the rig (see pg_arm_to_json in project_grease_armature.h):
 * {"version":1,"mode":0,"active":-1,"drawtype":0,"in_front":0,
 *  "bones":[{"name","parent","head":[3],"tail":[3],"roll","flag","inherit","dist","weight",
 *            "radHead","radTail","segments","layer"}],           (EditBone, list order)
 *  "pose":[{"name","loc":[3],"quat":[4],"scale":[3]}],
 *  "fcurves":[{"path","index","extend","smoothing","flag",
 *              "keys":[[x,y,ipo,h1,h2,lx,ly,rx,ry,easing,keytype], ...]}]} */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_action.h"
#include "BKE_armature.h"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"

#include "project_grease_armature.h"
#include "project_grease_armature_bke.h"
#include "project_grease_armature_intern.h"

namespace {

void put_num(std::string &s, double v)
{
  if (!std::isfinite(v)) v = 0.0;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.9g", v);
  s += buf;
}

void put_str(std::string &s, const char *v)
{
  s += '"';
  for (const char *p = v; *p; p++) {
    const unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') { s += '\\'; s += (char)c; }
    else if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); s += buf; }
    else s += (char)c;
  }
  s += '"';
}

void put_vec(std::string &s, const float *v, int n)
{
  s += '[';
  for (int i = 0; i < n; i++) { if (i) s += ','; put_num(s, v[i]); }
  s += ']';
}

/* Minimal JSON reader: objects, arrays, numbers, strings, true/false/null. */
struct JValue {
  enum Kind { NUL, NUM, STR, ARR, OBJ } kind = NUL;
  double num = 0;
  std::string str;
  std::vector<JValue> arr;
  std::map<std::string, JValue> obj;
  const JValue *get(const char *k) const
  {
    auto it = obj.find(k);
    return kind == OBJ && it != obj.end() ? &it->second : nullptr;
  }
  double n(const char *k, double def) const
  {
    const JValue *v = get(k);
    return v && v->kind == NUM ? v->num : def;
  }
};

struct JParser {
  const char *p;
  int depth = 0;
  void ws() { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; }
  bool parse(JValue &out)
  {
    ws();
    if (++depth > 64) return false;
    bool ok = parse_inner(out);
    depth--;
    return ok;
  }
  bool parse_inner(JValue &out)
  {
    if (*p == '{') {
      out.kind = JValue::OBJ;
      p++; ws();
      if (*p == '}') { p++; return true; }
      for (;;) {
        ws();
        JValue key;
        if (*p != '"' || !parse_string(key.str)) return false;
        ws();
        if (*p++ != ':') return false;
        JValue v;
        if (!parse(v)) return false;
        out.obj[key.str] = std::move(v);
        ws();
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; return true; }
        return false;
      }
    }
    if (*p == '[') {
      out.kind = JValue::ARR;
      p++; ws();
      if (*p == ']') { p++; return true; }
      for (;;) {
        JValue v;
        if (!parse(v)) return false;
        out.arr.push_back(std::move(v));
        ws();
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; return true; }
        return false;
      }
    }
    if (*p == '"') { out.kind = JValue::STR; return parse_string(out.str); }
    if (!strncmp(p, "true", 4)) { out.kind = JValue::NUM; out.num = 1; p += 4; return true; }
    if (!strncmp(p, "false", 5)) { out.kind = JValue::NUM; out.num = 0; p += 5; return true; }
    if (!strncmp(p, "null", 4)) { out.kind = JValue::NUL; p += 4; return true; }
    char *end = nullptr;
    out.num = std::strtod(p, &end);
    if (end == p) return false;
    out.kind = JValue::NUM;
    p = end;
    return true;
  }
  bool parse_string(std::string &out)
  {
    p++; /* opening quote */
    while (*p && *p != '"') {
      if (*p == '\\') {
        p++;
        switch (*p) {
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': {
            unsigned code = 0;
            for (int i = 1; i <= 4; i++) {
              const char c = p[i];
              if (!c) return false;
              code = code * 16 + (unsigned)(c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0');
            }
            p += 4;
            if (code < 0x80) out += (char)code;
            else if (code < 0x800) { out += (char)(0xC0 | (code >> 6)); out += (char)(0x80 | (code & 0x3F)); }
            else { out += (char)(0xE0 | (code >> 12)); out += (char)(0x80 | ((code >> 6) & 0x3F)); out += (char)(0x80 | (code & 0x3F)); }
            break;
          }
          case 0: return false;
          default: out += *p; break;
        }
        p++;
      }
      else {
        out += *p++;
      }
    }
    if (*p != '"') return false;
    p++;
    return true;
  }
};

void read_vec(const JValue *v, float *dst, int n)
{
  if (!v || v->kind != JValue::ARR) return;
  for (int i = 0; i < n && i < (int)v->arr.size(); i++) {
    if (v->arr[i].kind == JValue::NUM) dst[i] = (float)v->arr[i].num;
  }
}

std::string read_str(const JValue *v) { return v && v->kind == JValue::STR ? v->str : std::string(); }

}  // namespace

char *pg_arm_to_json(const PGArmature *a)
{
  if (!a) return nullptr;
  std::string s = "{\"version\":1,\"mode\":";
  put_num(s, a->mode);
  s += ",\"active\":";
  put_num(s, pg_arm_active_bone(a));
  s += ",\"drawtype\":";
  put_num(s, a->arm.drawtype);
  s += ",\"in_front\":";
  put_num(s, a->show_in_front);
  s += ",\"bones\":[";
  bool first = true;
  LISTBASE_FOREACH (const EditBone *, eb, &a->edbo) {
    if (!first) s += ',';
    first = false;
    s += "{\"name\":"; put_str(s, eb->name);
    s += ",\"parent\":"; put_num(s, eb->parent ? BLI_findindex(&a->edbo, eb->parent) : -1);
    s += ",\"head\":"; put_vec(s, eb->head, 3);
    s += ",\"tail\":"; put_vec(s, eb->tail, 3);
    s += ",\"roll\":"; put_num(s, eb->roll);
    s += ",\"flag\":"; put_num(s, eb->flag);
    s += ",\"inherit\":"; put_num(s, eb->inherit_scale_mode);
    s += ",\"dist\":"; put_num(s, eb->dist);
    s += ",\"weight\":"; put_num(s, eb->weight);
    s += ",\"radHead\":"; put_num(s, eb->rad_head);
    s += ",\"radTail\":"; put_num(s, eb->rad_tail);
    s += ",\"segments\":"; put_num(s, eb->segments);
    s += ",\"layer\":"; put_num(s, eb->layer);
    s += '}';
  }
  s += "],\"pose\":[";
  first = true;
  LISTBASE_FOREACH (const bPoseChannel *, pc, &a->pose.chanbase) {
    if (!first) s += ',';
    first = false;
    s += "{\"name\":"; put_str(s, pc->name);
    s += ",\"loc\":"; put_vec(s, pc->loc, 3);
    s += ",\"quat\":"; put_vec(s, pc->quat, 4);
    s += ",\"scale\":"; put_vec(s, pc->size, 3);
    s += '}';
  }
  s += "],\"fcurves\":[";
  first = true;
  LISTBASE_FOREACH (const FCurve *, fcu, &a->curves) {
    if (!first) s += ',';
    first = false;
    s += "{\"path\":"; put_str(s, fcu->rna_path ? fcu->rna_path : "");
    s += ",\"index\":"; put_num(s, fcu->array_index);
    s += ",\"extend\":"; put_num(s, fcu->extend);
    s += ",\"smoothing\":"; put_num(s, fcu->auto_smoothing);
    s += ",\"flag\":"; put_num(s, fcu->flag);
    s += ",\"keys\":[";
    for (int i = 0; i < fcu->totvert; i++) {
      const BezTriple &b = fcu->bezt[i];
      const float k[11] = {b.vec[1][0], b.vec[1][1], (float)b.ipo, (float)b.h1, (float)b.h2,
                           b.vec[0][0], b.vec[0][1], b.vec[2][0], b.vec[2][1], (float)b.easing,
                           (float)b.hide};
      if (i) s += ',';
      put_vec(s, k, 11);
    }
    s += "]}";
  }
  s += "]}";
  char *out = static_cast<char *>(std::malloc(s.size() + 1));
  if (out) std::memcpy(out, s.c_str(), s.size() + 1);
  return out;
}

PGArmature *pg_arm_from_json(const char *json)
{
  if (!json) return nullptr;
  JValue root;
  JParser parser{json};
  if (!parser.parse(root) || root.kind != JValue::OBJ) return nullptr;
  const JValue *bones = root.get("bones");
  if (!bones || bones->kind != JValue::ARR) return nullptr;

  PGArmature *a = pg_arm_new();
  std::vector<EditBone *> list;
  for (const JValue &jb : bones->arr) {
    if (jb.kind != JValue::OBJ) continue;
    EditBone *eb = pg_bke_ebone_add(&a->arm, read_str(jb.get("name")).c_str()); /* unique, defaults */
    read_vec(jb.get("head"), eb->head, 3);
    read_vec(jb.get("tail"), eb->tail, 3);
    eb->roll = (float)jb.n("roll", 0);
    eb->flag = (int)jb.n("flag", eb->flag);
    eb->inherit_scale_mode = (char)jb.n("inherit", 0);
    eb->dist = (float)jb.n("dist", eb->dist);
    eb->weight = (float)jb.n("weight", eb->weight);
    eb->rad_head = (float)jb.n("radHead", eb->rad_head);
    eb->rad_tail = (float)jb.n("radTail", eb->rad_tail);
    eb->segments = (short)jb.n("segments", 1);
    eb->layer = (int)jb.n("layer", eb->layer);
    list.push_back(eb);
  }
  for (size_t i = 0; i < list.size(); i++) {
    const int parent = (int)bones->arr[i].n("parent", -1);
    /* only earlier-or-later valid indices that do not make a cycle */
    if (parent >= 0 && parent < (int)list.size() && parent != (int)i) {
      bool cycle = false;
      for (EditBone *p = list[parent]; p; p = p->parent) if (p == list[i]) cycle = true;
      if (!cycle) list[i]->parent = list[parent];
    }
    if (!list[i]->parent) list[i]->flag &= ~BONE_CONNECTED;
  }
  const int active = (int)root.n("active", -1);
  a->arm.act_edbone = active >= 0 && active < (int)list.size() ? list[active] : nullptr;
  pg_arm_set_display(a, (int)root.n("drawtype", ARM_OCTA), root.n("in_front", 0) != 0);
  const int mode = (int)root.n("mode", 0);
  pg_arm_set_mode(a, mode);
  pg_arm_update(a);

  if (const JValue *pose = root.get("pose"); pose && pose->kind == JValue::ARR) {
    for (const JValue &jp : pose->arr) {
      bPoseChannel *pc = BKE_pose_channel_find_name(&a->pose, read_str(jp.get("name")).c_str());
      if (!pc) continue;
      read_vec(jp.get("loc"), pc->loc, 3);
      read_vec(jp.get("quat"), pc->quat, 4);
      read_vec(jp.get("scale"), pc->size, 3);
    }
  }
  if (const JValue *curves = root.get("fcurves"); curves && curves->kind == JValue::ARR) {
    for (const JValue &jc : curves->arr) {
      const std::string path = read_str(jc.get("path"));
      const JValue *keys = jc.get("keys");
      if (path.empty() || !keys || keys->kind != JValue::ARR || keys->arr.empty()) continue;
      FCurve *fcu = static_cast<FCurve *>(MEM_callocN(sizeof(FCurve), "FCurve"));
      fcu->rna_path = BLI_strdup(path.c_str());
      fcu->array_index = (int)jc.n("index", 0);
      fcu->extend = (short)jc.n("extend", 0);
      fcu->auto_smoothing = (char)jc.n("smoothing", FCURVE_SMOOTH_CONT_ACCEL);
      fcu->flag = (short)jc.n("flag", FCURVE_VISIBLE | FCURVE_SELECTED);
      fcu->bezt = static_cast<BezTriple *>(MEM_callocN(sizeof(BezTriple) * keys->arr.size(), "beztriple"));
      for (const JValue &jk : keys->arr) {
        float k[11] = {0, 0, BEZT_IPO_BEZ, HD_AUTO_ANIM, HD_AUTO_ANIM, 0, 0, 0, 0, 0, 0};
        read_vec(&jk, k, 11);
        BezTriple &b = fcu->bezt[fcu->totvert];
        if (fcu->totvert > 0 && k[0] <= fcu->bezt[fcu->totvert - 1].vec[1][0]) continue; /* keys must increase */
        b.vec[1][0] = k[0];
        b.vec[1][1] = k[1];
        b.ipo = (char)CLAMPIS((int)k[2], BEZT_IPO_CONST, BEZT_IPO_SINE);
        b.h1 = (uint8_t)CLAMPIS((int)k[3], HD_FREE, HD_ALIGN_DOUBLESIDE);
        b.h2 = (uint8_t)CLAMPIS((int)k[4], HD_FREE, HD_ALIGN_DOUBLESIDE);
        b.vec[0][0] = k[5];
        b.vec[0][1] = k[6];
        b.vec[2][0] = k[7];
        b.vec[2][1] = k[8];
        b.easing = (char)k[9];
        b.hide = (char)k[10];
        b.f1 = b.f2 = b.f3 = SELECT;
        b.back = 1.70158f;
        b.amplitude = 0.8f;
        b.period = 4.1f;
        fcu->totvert++;
      }
      BLI_addtail(&a->curves, fcu);
      pg_bke_fcurve_handles_recalc(fcu);
    }
  }
  pg_arm_update(a);
  return a;
}
