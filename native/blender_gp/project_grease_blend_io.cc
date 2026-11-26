/* SPDX-License-Identifier: GPL-2.0-or-later
 * Blender 3.6.23 .blend open / save (see project_grease_blend_io.h).
 *
 * Open is a port of blenloader/intern/readfile.cc: blo_filedata_from_file_descriptor (compression by
 * magic, :1186), decode_blender_header (:943), get_bhead / blo_bhead_next (:691 / :863), read_file_dna
 * (:987, DNA_sdna_from_data + DNA_struct_get_compareflags + DNA_reconstruct_info_create), read_struct
 * (:1800, DNA_struct_reconstruct when the file DNA differs) and the oldnewmap relinking of the
 * blend_read_data callbacks it uses (BKE_gpencil_blend_read_data gpencil_legacy.c:183,
 * armature_blend_read_data armature.c:245, BKE_pose_blend_read_data action.c:1885,
 * BKE_fcurve_blend_read_data, material_blend_read_data, object_blend_read_data), then
 * make_boneList_recursive (editors/armature/armature_utils.c:451) for the rest bones.
 *
 * Save is a port of blenloader/intern/writefile.cc write_file_handle (:1193): the "BLENDER-v306" header,
 * write_global (:1027), writestruct_at_address_nr / writedata (:662 / :698), the ID blend_write
 * callbacks (scene_blend_write scene.cc:953, BKE_view_layer_blend_write layer.cc:2371,
 * BKE_collection_blend_write_nolib collection.c:227, object_blend_write object.cc:542,
 * greasepencil_blend_write gpencil_legacy.c:133, material_blend_write material.cc:171,
 * armature_blend_write / write_bone armature.c:200 / :181, BKE_pose_blend_write action.c:1833,
 * action_blend_write action.c:174, BKE_animdata_blend_write anim_data.c:1454, BKE_fcurve_blend_write
 * fcurve.c:2564, BKE_gpencil_modifier_blend_write gpencil_modifier_legacy.c:842, BKE_defvert_blend_write
 * deform.cc:1587), DNA1 = this build's SDNA (makesdna of the pinned 3.6.23 headers) and ENDB.
 *
 * Coordinates: GP points are canvas px in the app, Blender units in the file:
 * (x, y, z)_blender = (px, -py, pz) / PG_ARM_PIXELS_PER_UNIT, as the rig (project_grease_armature.c).
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BLI_filereader.h"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_defaults.h"
#include "DNA_fileglobal_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_layer_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sdna_types.h"

#include "BKE_blender_version.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLO_blend_defs.h"

#include "project_grease_armature.h"
#include "project_grease_armature_intern.h"
#include "project_grease_blend_io.h"
#include "project_grease_modifier_stack.h"

namespace {

constexpr float PPU = PG_ARM_PIXELS_PER_UNIT;
constexpr size_t SIZEOFBLENDERHEADER = 12; /* readfile.h */
/* BLI_endian_defines.h (it needs the build's __LITTLE_ENDIAN__ define) */
constexpr int L_ENDIAN = 1, B_ENDIAN = 0;
constexpr int ENDIAN_ORDER = __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ ? B_ENDIAN : L_ENDIAN;

void set_err(char *err, int n, const char *msg)
{
  if (err && n > 0) BLI_strncpy(err, msg, (size_t)n);
}

char *dup_out(const std::string &s)
{
  char *out = static_cast<char *>(std::malloc(s.size() + 1));
  if (out) std::memcpy(out, s.c_str(), s.size() + 1);
  return out;
}

/* ---------------------------------------------------------------------------------------- */
/* JSON (the project document)                                                               */

struct JV {
  enum Kind { NUL, NUM, STR, ARR, OBJ, BOOL } kind = NUL;
  double num = 0;
  std::string str;
  std::vector<JV> arr;
  std::map<std::string, JV> obj;
  const JV *get(const char *k) const
  {
    if (kind != OBJ) return nullptr;
    auto it = obj.find(k);
    return it != obj.end() ? &it->second : nullptr;
  }
  double n(const char *k, double def) const
  {
    const JV *v = get(k);
    return v && (v->kind == NUM || v->kind == BOOL) ? v->num : def;
  }
  double at(size_t i, double def) const
  {
    return kind == ARR && i < arr.size() && arr[i].kind == NUM ? arr[i].num : def;
  }
  std::string s(const char *k) const
  {
    const JV *v = get(k);
    return v && v->kind == STR ? v->str : std::string();
  }
  const std::vector<JV> &list(const char *k) const
  {
    static const std::vector<JV> empty;
    const JV *v = get(k);
    return v && v->kind == ARR ? v->arr : empty;
  }
};

struct JP {
  const char *p;
  int depth = 0;
  void ws() { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; }
  bool lit(const char *w)
  {
    const size_t l = std::strlen(w);
    if (std::strncmp(p, w, l) != 0) return false;
    p += l;
    return true;
  }
  bool str(std::string &out)
  {
    if (*p != '"') return false;
    p++;
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
            unsigned c = 0;
            for (int i = 1; i <= 4; i++) {
              const char h = p[i];
              if (!h) return false;
              c = c * 16 + (unsigned)(h <= '9' ? h - '0' : (h | 32) - 'a' + 10);
            }
            p += 4;
            /* UTF-8 of the BMP code point (names only; surrogate pairs come out as two) */
            if (c < 0x80) out += (char)c;
            else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 63)); }
            else { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 63)); out += (char)(0x80 | (c & 63)); }
            break;
          }
          case 0: return false;
          default: out += *p;
        }
        p++;
      }
      else out += *p++;
    }
    if (*p != '"') return false;
    p++;
    return true;
  }
  bool parse(JV &v)
  {
    ws();
    if (++depth > 64) return false;
    bool ok = true;
    if (*p == '{') {
      v.kind = JV::OBJ;
      p++;
      ws();
      if (*p == '}') p++;
      else for (;;) {
        ws();
        std::string k;
        if (!str(k)) { ok = false; break; }
        ws();
        if (*p++ != ':') { ok = false; break; }
        if (!parse(v.obj[k])) { ok = false; break; }
        ws();
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; break; }
        ok = false;
        break;
      }
    }
    else if (*p == '[') {
      v.kind = JV::ARR;
      p++;
      ws();
      if (*p == ']') p++;
      else for (;;) {
        v.arr.emplace_back();
        if (!parse(v.arr.back())) { ok = false; break; }
        ws();
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; break; }
        ok = false;
        break;
      }
    }
    else if (*p == '"') { v.kind = JV::STR; ok = str(v.str); }
    else if (lit("true")) { v.kind = JV::BOOL; v.num = 1; }
    else if (lit("false")) { v.kind = JV::BOOL; v.num = 0; }
    else if (lit("null")) v.kind = JV::NUL;
    else {
      char *end = nullptr;
      v.num = std::strtod(p, &end);
      if (end == p) ok = false;
      v.kind = JV::NUM;
      p = end ? end : p;
    }
    depth--;
    return ok;
  }
};

void put_num(std::string &s, double v)
{
  char b[40];
  if (!std::isfinite(v)) v = 0;
  if (v == std::floor(v) && std::fabs(v) < 1e15) std::snprintf(b, sizeof(b), "%.0f", v);
  else std::snprintf(b, sizeof(b), "%.9g", v); /* every float round-trips */
  s += b;
}
void put_str(std::string &s, const char *v)
{
  s += '"';
  for (const unsigned char *c = (const unsigned char *)v; *c; c++) {
    if (*c == '"' || *c == '\\') { s += '\\'; s += (char)*c; }
    else if (*c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", *c); s += b; }
    else s += (char)*c;
  }
  s += '"';
}
void put_vec(std::string &s, const float *v, int n)
{
  s += '[';
  for (int i = 0; i < n; i++) { if (i) s += ','; put_num(s, v[i]); }
  s += ']';
}
void key(std::string &s, const char *k)
{
  if (s.back() != '{' && s.back() != '[') s += ',';
  put_str(s, k);
  s += ':';
}

/* ---------------------------------------------------------------------------------------- */
/* Open: readfile.cc                                                                         */

enum { FD_FLAGS_SWITCH_ENDIAN = 1 << 0, FD_FLAGS_FILE_POINTSIZE_IS_4 = 1 << 1, FD_FLAGS_FILE_OK = 1 << 3 };

struct BlendFileData {
  std::vector<char> buf;
  int flags = 0;
  int fileversion = 0;
  SDNA *filesdna = nullptr;
  const SDNA *memsdna = nullptr;
  const char *compflags = nullptr;
  DNA_ReconstructInfo *reconstruct_info = nullptr;
  std::unordered_map<uintptr_t, void *> datamap; /* oldnewmap: old address -> read memory */
  std::vector<ID *> ids;
  std::vector<void *> owned;
  ~BlendFileData()
  {
    for (void *p : owned) MEM_freeN(p);
    if (reconstruct_info) DNA_reconstruct_info_free(reconstruct_info);
    if (compflags) MEM_freeN((void *)compflags);
    if (filesdna) DNA_sdna_free(filesdna);
  }
  /* newdataadr / BLO_read_data_address: an address nothing was read at is NULL */
  template<class T> T *R(T *old) const
  {
    auto it = datamap.find((uintptr_t)old);
    return it == datamap.end() ? nullptr : static_cast<T *>(it->second);
  }
  template<class T> T *first(const ListBase &lb) const { return R(static_cast<T *>(lb.first)); }
  template<class T> T *next(const T *l) const { return R(l->next); }
};

#define FOREACH_READ(T, it, fd, lb) for (T *it = (fd).first<T>(lb); it; it = (fd).next(it))

struct BHeadRef {
  BHead bh;
  const char *data;
};

/* blo_filedata_from_file_descriptor + FileReader: the whole (decompressed) file */
bool read_all(BlendFileData &fd, const void *data, size_t len, char *err, int en)
{
  if (len < 7) { set_err(err, en, "Not a blend file (too short)"); return false; }
  const char *h = static_cast<const char *>(data);
  if (std::memcmp(h, "BLENDER", 7) == 0) {
    fd.buf.assign(h, h + len);
    return true;
  }
  FileReader *raw = BLI_filereader_new_memory(data, len);
  FileReader *file = nullptr;
  if (BLI_file_magic_is_gzip(h)) file = BLI_filereader_new_gzip(raw);
  else if (BLI_file_magic_is_zstd(h)) file = BLI_filereader_new_zstd(raw);
  if (!file) {
    raw->close(raw);
    set_err(err, en, "Unrecognized file format");
    return false;
  }
  char chunk[65536];
  for (;;) {
    const int64_t got = file->read(file, chunk, sizeof(chunk));
    if (got <= 0) break;
    fd.buf.insert(fd.buf.end(), chunk, chunk + got);
  }
  file->close(file);
  return true;
}

/* decode_blender_header */
bool decode_blender_header(BlendFileData &fd)
{
  const char *header = fd.buf.data();
  if (fd.buf.size() >= SIZEOFBLENDERHEADER && STREQLEN(header, "BLENDER", 7) && ELEM(header[7], '_', '-') &&
      ELEM(header[8], 'v', 'V') && isdigit(header[9]) && isdigit(header[10]) && isdigit(header[11]))
  {
    fd.flags |= FD_FLAGS_FILE_OK;
    if (header[7] == '_') fd.flags |= FD_FLAGS_FILE_POINTSIZE_IS_4;
    if (((header[8] == 'v') ? L_ENDIAN : B_ENDIAN) != ENDIAN_ORDER) fd.flags |= FD_FLAGS_SWITCH_ENDIAN;
    char num[4];
    memcpy(num, header + 9, 3);
    num[3] = 0;
    fd.fileversion = atoi(num);
    return true;
  }
  return false;
}

/* get_bhead over the in-memory file (BHead8 of this platform) */
std::vector<BHeadRef> bheads(const BlendFileData &fd)
{
  std::vector<BHeadRef> out;
  size_t pos = SIZEOFBLENDERHEADER;
  while (pos + sizeof(BHead) <= fd.buf.size()) {
    BHeadRef r;
    std::memcpy(&r.bh, fd.buf.data() + pos, sizeof(BHead));
    if (r.bh.code == BLO_CODE_ENDB) break;
    /* make sure people are not trying to pass bad blend files */
    if (r.bh.len < 0 || pos + sizeof(BHead) + (size_t)r.bh.len > fd.buf.size()) break;
    r.data = fd.buf.data() + pos + sizeof(BHead);
    out.push_back(r);
    pos += sizeof(BHead) + (size_t)r.bh.len;
  }
  return out;
}

/* read_file_dna */
bool read_file_dna(BlendFileData &fd, const std::vector<BHeadRef> &blocks, const char **r_error)
{
  for (const BHeadRef &b : blocks) {
    if (b.bh.code == BLO_CODE_DNA1) {
      fd.filesdna = DNA_sdna_from_data(b.data, b.bh.len, false, true, r_error);
      if (!fd.filesdna) return false;
      fd.compflags = DNA_struct_get_compareflags(fd.filesdna, fd.memsdna);
      fd.reconstruct_info = DNA_reconstruct_info_create(fd.filesdna, fd.memsdna, fd.compflags);
      return true;
    }
  }
  *r_error = "Missing DNA block";
  return false;
}

/* read_struct */
void *read_struct(BlendFileData &fd, const BHeadRef &b)
{
  if (b.bh.len <= 0 || b.bh.SDNAnr < 0 || b.bh.SDNAnr >= fd.filesdna->structs_len) return nullptr;
  if (fd.compflags[b.bh.SDNAnr] == SDNA_CMP_REMOVED) return nullptr;
  if (fd.compflags[b.bh.SDNAnr] == SDNA_CMP_NOT_EQUAL) {
    return DNA_struct_reconstruct(fd.reconstruct_info, b.bh.SDNAnr, b.bh.nr, b.data);
  }
  void *temp = MEM_mallocN((size_t)b.bh.len, "read_struct");
  std::memcpy(temp, b.data, (size_t)b.bh.len);
  return temp;
}

const SDNA *mem_sdna()
{
  static bool init = false;
  if (!init) {
    DNA_sdna_current_init();
    init = true;
  }
  return DNA_sdna_current_get();
}

bool open_blend(BlendFileData &fd, const void *data, size_t len, char *err, int en)
{
  if (!read_all(fd, data, len, err, en)) return false;
  if (!decode_blender_header(fd)) { set_err(err, en, "Not a blend file"); return false; }
  /* shortcut: 64-bit little-endian files only (every Blender 3.x desktop / arm64 build); bh4/endian
   * conversion of get_bhead is not ported, upgrade if a 32-bit or big-endian file must open */
  if (fd.flags & (FD_FLAGS_SWITCH_ENDIAN | FD_FLAGS_FILE_POINTSIZE_IS_4) || sizeof(void *) != 8) {
    set_err(err, en, "Only 64-bit little-endian .blend files are supported");
    return false;
  }
  fd.memsdna = mem_sdna();
  const std::vector<BHeadRef> blocks = bheads(fd);
  const char *error = nullptr;
  if (!read_file_dna(fd, blocks, &error)) { set_err(err, en, error ? error : "Bad DNA"); return false; }
  for (const BHeadRef &b : blocks) {
    if (ELEM(b.bh.code, BLO_CODE_DNA1, BLO_CODE_GLOB, BLO_CODE_REND, BLO_CODE_TEST, BLO_CODE_USER)) continue;
    void *p = read_struct(fd, b);
    if (!p) continue;
    fd.owned.push_back(p);
    fd.datamap[(uintptr_t)b.bh.old] = p;
    if (b.bh.code != BLO_CODE_DATA) fd.ids.push_back(static_cast<ID *>(p));
  }
  return true;
}

/* ---- import: DNA to the project JSON */

std::vector<std::string> import_vgroups(const BlendFileData &fd, const bGPdata *gpd)
{
  std::vector<std::string> out;
  FOREACH_READ (const bDeformGroup, dg, fd, gpd->vertex_group_names) out.push_back(dg->name);
  return out;
}

int vgroup_index(const std::vector<std::string> &groups, const char *name)
{
  for (size_t i = 0; i < groups.size(); i++) if (groups[i] == name) return (int)i;
  return -1;
}

void import_stroke(std::string &s, const BlendFileData &fd, const bGPDstroke *gps)
{
  const bGPDspoint *pts = fd.R(gps->points);
  const MDeformVert *dvert = fd.R(gps->dvert);
  s += '{';
  key(s, "points");
  s += '[';
  for (int i = 0; pts && i < gps->totpoints; i++) {
    const bGPDspoint &p = pts[i];
    const float v[10] = {p.x * PPU, -p.y * PPU, p.z * PPU, p.pressure, p.strength, p.time,
                         p.vert_color[0], p.vert_color[1], p.vert_color[2], p.vert_color[3]};
    if (i) s += ',';
    put_vec(s, v, 10);
  }
  s += ']';
  key(s, "material"); put_num(s, gps->mat_nr);
  key(s, "thickness"); put_num(s, gps->thickness);
  key(s, "cyclic"); s += (gps->flag & GP_STROKE_CYCLIC) ? "true" : "false";
  key(s, "fillOpacity"); put_num(s, gps->fill_opacity_fac);
  key(s, "fillColor"); put_vec(s, gps->vert_color_fill, 4);
  key(s, "caps"); s += '['; put_num(s, gps->caps[0]); s += ','; put_num(s, gps->caps[1]); s += ']';
  key(s, "hardness"); put_num(s, gps->hardeness);
  if (dvert) {
    key(s, "weights");
    s += '[';
    bool first = true;
    for (int i = 0; i < gps->totpoints; i++) {
      const MDeformWeight *dw = fd.R(dvert[i].dw);
      if (!dw || dvert[i].totweight <= 0) continue;
      if (!first) s += ',';
      first = false;
      s += '[';
      put_num(s, i);
      for (int w = 0; w < dvert[i].totweight; w++) {
        s += ',';
        put_num(s, dw[w].def_nr);
        s += ',';
        put_num(s, dw[w].weight);
      }
      s += ']';
    }
    s += ']';
  }
  s += '}';
}

/* Blender GP modifiers the app has (Thickness, Tint, Armature) as the app's per-layer entries:
 * a modifier with no layer filter goes on every layer. */
struct ImportedMod {
  std::string layer;
  int type;
  bool enabled;
  std::vector<float> params;
};

std::vector<ImportedMod> import_modifiers(const BlendFileData &fd, const Object *ob,
                                          const std::vector<std::string> &groups)
{
  std::vector<ImportedMod> out;
  FOREACH_READ (const GpencilModifierData, md, fd, ob->greasepencil_modifiers) {
    ImportedMod m;
    m.enabled = (md->mode & eGpencilModifierMode_Realtime) != 0;
    if (md->type == eGpencilModifierType_Thick) {
      const auto *t = reinterpret_cast<const ThickGpencilModifierData *>(md);
      const int vg = vgroup_index(groups, t->vgname);
      m.type = PG_MOD_THICKNESS;
      m.layer = t->layername;
      m.params = {(t->flag & GP_THICK_NORMALIZE) ? 1.0f : 0.0f, (float)t->thickness, t->thickness_fac,
                  vg >= 0 ? 1.0f : 0.0f, (float)MAX2(vg, 0), (t->flag & GP_THICK_INVERT_VGROUP) ? 1.0f : 0.0f};
    }
    else if (md->type == eGpencilModifierType_Tint) {
      const auto *t = reinterpret_cast<const TintGpencilModifierData *>(md);
      m.type = PG_MOD_TINT;
      m.layer = t->layername;
      m.params = {(float)t->mode, t->factor, t->rgb[0], t->rgb[1], t->rgb[2]};
    }
    else if (md->type == eGpencilModifierType_Armature) {
      const auto *a = reinterpret_cast<const ArmatureGpencilModifierData *>(md);
      const int vg = vgroup_index(groups, a->vgname);
      m.type = PG_MOD_ARMATURE;
      m.params = {(a->deformflag & ARM_DEF_VGROUP) ? 1.0f : 0.0f, (a->deformflag & ARM_DEF_ENVELOPE) ? 1.0f : 0.0f,
                  (a->deformflag & ARM_DEF_QUATERNION) ? 1.0f : 0.0f, (float)(vg + 1),
                  (a->deformflag & ARM_DEF_INVERT_VGROUP) ? 1.0f : 0.0f};
    }
    else continue; /* not in the app */
    out.push_back(m);
  }
  return out;
}

void import_material(std::string &s, const BlendFileData &fd, const Material *ma)
{
  const MaterialGPencilStyle *st = ma ? fd.R(ma->gp_style) : nullptr;
  MaterialGPencilStyle def{};
  if (!st) { /* not a GP material: the BKE_gpencil_material_attr_init defaults */
    def.stroke_rgba[3] = def.fill_rgba[3] = 1.0f;
    def.flag = GP_MATERIAL_STROKE_SHOW;
    st = &def;
  }
  s += '{';
  key(s, "stroke"); put_vec(s, st->stroke_rgba, 4);
  key(s, "fill"); put_vec(s, st->fill_rgba, 4);
  key(s, "visible"); s += (st->flag & GP_MATERIAL_HIDE) ? "false" : "true";
  key(s, "fillEnabled"); s += (st->flag & GP_MATERIAL_FILL_SHOW) ? "true" : "false";
  key(s, "strokeEnabled"); s += (st->flag & GP_MATERIAL_STROKE_SHOW) ? "true" : "false";
  key(s, "name"); put_str(s, ma ? ma->id.name + 2 : "Material");
  key(s, "locked"); s += (st->flag & GP_MATERIAL_LOCKED) ? "true" : "false";
  key(s, "mode"); put_num(s, st->mode);
  key(s, "alignment"); put_num(s, st->alignment_mode);
  key(s, "rotation"); put_num(s, st->alignment_rotation);
  key(s, "pass"); put_num(s, st->index);
  if (st->fill_style == GP_MATERIAL_FILL_STYLE_GRADIENT) {
    const float g[12] = {(float)st->gradient_type, st->mix_rgba[0], st->mix_rgba[1], st->mix_rgba[2],
                         st->mix_rgba[3], st->mix_factor, st->texture_angle, st->texture_scale[0],
                         st->texture_scale[1], st->texture_offset[0], st->texture_offset[1],
                         (st->flag & GP_MATERIAL_FLIP_FILL) ? 1.0f : 0.0f};
    key(s, "gradient"); put_vec(s, g, 12);
  }
  key(s, "strokeHoldout"); s += (st->flag & GP_MATERIAL_IS_STROKE_HOLDOUT) ? "true" : "false";
  key(s, "fillHoldout"); s += (st->flag & GP_MATERIAL_IS_FILL_HOLDOUT) ? "true" : "false";
  key(s, "selfOverlap"); s += (st->flag & GP_MATERIAL_DISABLE_STENCIL) ? "true" : "false";
  s += '}';
}

/* make_boneList_recursive: depth first, a bone before its children (the edit-bone list order) */
void collect_bones(const BlendFileData &fd, const ListBase &lb, int parent, std::vector<std::pair<const Bone *, int>> &out)
{
  FOREACH_READ (const Bone, b, fd, lb) {
    const int index = (int)out.size();
    out.emplace_back(b, parent);
    collect_bones(fd, b->childbase, index, out);
  }
}

void import_armature(std::string &s, const BlendFileData &fd, const Object *ob)
{
  const bArmature *arm = fd.R(static_cast<bArmature *>(ob->data));
  if (!arm) return;
  std::vector<std::pair<const Bone *, int>> bones;
  collect_bones(fd, arm->bonebase, -1, bones);
  key(s, "armature");
  s += "{\"version\":1,\"mode\":0";
  int active = -1;
  for (size_t i = 0; i < bones.size(); i++) if (fd.R(arm->act_bone) == bones[i].first) active = (int)i;
  key(s, "active"); put_num(s, active);
  key(s, "drawtype"); put_num(s, arm->drawtype);
  key(s, "in_front"); put_num(s, (ob->dtx & OB_DRAW_IN_FRONT) ? 1 : 0);
  key(s, "mirror_x"); put_num(s, (arm->flag & ARM_MIRROR_EDIT) ? 1 : 0);
  key(s, "bones");
  s += '[';
  for (size_t i = 0; i < bones.size(); i++) {
    const Bone *b = bones[i].first;
    if (i) s += ',';
    s += '{';
    key(s, "name"); put_str(s, b->name);
    key(s, "parent"); put_num(s, bones[i].second);
    key(s, "head"); put_vec(s, b->arm_head, 3);
    key(s, "tail"); put_vec(s, b->arm_tail, 3);
    key(s, "roll"); put_num(s, b->arm_roll);
    key(s, "flag"); put_num(s, b->flag & ~(BONE_DRAW_ACTIVE | BONE_DRAW_LOCKED_WEIGHT));
    key(s, "inherit"); put_num(s, b->inherit_scale_mode);
    key(s, "dist"); put_num(s, b->dist);
    key(s, "weight"); put_num(s, b->weight);
    key(s, "radHead"); put_num(s, b->rad_head);
    key(s, "radTail"); put_num(s, b->rad_tail);
    key(s, "segments"); put_num(s, b->segments);
    key(s, "layer"); put_num(s, b->layer ? b->layer : 1);
    s += '}';
  }
  s += ']';
  key(s, "pose");
  s += '[';
  const bPose *pose = fd.R(ob->pose);
  bool first = true;
  if (pose) FOREACH_READ (const bPoseChannel, pc, fd, pose->chanbase) {
    float quat[4];
    if (pc->rotmode == ROT_MODE_QUAT) copy_qt_qt(quat, pc->quat);
    else if (pc->rotmode == ROT_MODE_AXISANGLE) axis_angle_to_quat(quat, pc->rotAxis, pc->rotAngle);
    else eulO_to_quat(quat, pc->eul, pc->rotmode); /* BKE_pchan_to_mat4's rotation, as a quaternion */
    if (!first) s += ',';
    first = false;
    s += '{';
    key(s, "name"); put_str(s, pc->name);
    key(s, "loc"); put_vec(s, pc->loc, 3);
    key(s, "quat"); put_vec(s, quat, 4);
    key(s, "scale"); put_vec(s, pc->size, 3);
    s += '}';
  }
  s += ']';
  key(s, "fcurves");
  s += '[';
  const AnimData *adt = fd.R(ob->adt);
  const bAction *act = adt ? fd.R(adt->action) : nullptr;
  first = true;
  if (act) FOREACH_READ (const FCurve, fcu, fd, act->curves) {
    const char *path = fd.R(fcu->rna_path);
    const BezTriple *bezt = fd.R(fcu->bezt);
    if (!path || !bezt || fcu->totvert <= 0) continue;
    if (!first) s += ',';
    first = false;
    s += '{';
    key(s, "path"); put_str(s, path);
    key(s, "index"); put_num(s, fcu->array_index);
    key(s, "extend"); put_num(s, fcu->extend);
    key(s, "smoothing"); put_num(s, fcu->auto_smoothing);
    key(s, "flag"); put_num(s, fcu->flag);
    key(s, "keys");
    s += '[';
    for (int i = 0; i < fcu->totvert; i++) {
      const BezTriple &b = bezt[i];
      const float k[11] = {b.vec[1][0], b.vec[1][1], (float)b.ipo, (float)b.h1, (float)b.h2, b.vec[0][0],
                           b.vec[0][1], b.vec[2][0], b.vec[2][1], (float)b.easing, (float)b.hide};
      if (i) s += ',';
      put_vec(s, k, 11);
    }
    s += "]}";
  }
  s += "]}";
}

std::string import_document(const BlendFileData &fd)
{
  const Object *gp = nullptr;
  const Scene *scene = nullptr;
  for (const ID *id : fd.ids) {
    if (GS(id->name) == ID_OB && !gp && reinterpret_cast<const Object *>(id)->type == OB_GPENCIL_LEGACY)
      gp = reinterpret_cast<const Object *>(id);
    if (GS(id->name) == ID_SCE && !scene) scene = reinterpret_cast<const Scene *>(id);
  }
  const bGPdata *gpd = gp ? fd.R(static_cast<bGPdata *>(gp->data)) : nullptr;
  if (!gpd) return std::string();
  /* the rig: the Armature modifier's object, else the parent, else the first armature */
  const Object *rig = nullptr;
  FOREACH_READ (const GpencilModifierData, md, fd, gp->greasepencil_modifiers) {
    if (md->type == eGpencilModifierType_Armature && !rig)
      rig = fd.R(reinterpret_cast<const ArmatureGpencilModifierData *>(md)->object);
  }
  if (!rig && fd.R(gp->parent) && fd.R(gp->parent)->type == OB_ARMATURE) rig = fd.R(gp->parent);
  for (const ID *id : fd.ids)
    if (!rig && GS(id->name) == ID_OB && reinterpret_cast<const Object *>(id)->type == OB_ARMATURE)
      rig = reinterpret_cast<const Object *>(id);

  std::string s = "{\"version\":6";
  if (scene) {
    key(s, "fps"); put_num(s, scene->r.frs_sec);
    key(s, "frame"); put_num(s, MAX2(scene->r.cfra, 1));
    key(s, "frameEnd"); put_num(s, MAX2(scene->r.efra, 1));
  }
  key(s, "materials");
  s += '[';
  Material **mat = fd.R(gpd->mat);
  for (int i = 0; i < gpd->totcol; i++) {
    if (i) s += ',';
    import_material(s, fd, mat ? fd.R(mat[i]) : nullptr);
  }
  s += ']';
  const std::vector<std::string> groups = import_vgroups(fd, gpd);
  if (!groups.empty()) {
    key(s, "vertexGroups");
    s += '[';
    for (size_t i = 0; i < groups.size(); i++) { if (i) s += ','; put_str(s, groups[i].c_str()); }
    s += ']';
    key(s, "activeVertexGroup"); put_num(s, gpd->vertex_group_active_index - 1);
  }
  const std::vector<ImportedMod> mods = import_modifiers(fd, gp, groups);
  key(s, "layers");
  s += '[';
  int li = 0;
  FOREACH_READ (const bGPDlayer, gpl, fd, gpd->layers) {
    if (li) s += ',';
    s += '{';
    key(s, "index"); put_num(s, li++);
    key(s, "name"); put_str(s, gpl->info);
    key(s, "visible"); s += (gpl->flag & GP_LAYER_HIDE) ? "false" : "true";
    key(s, "locked"); s += (gpl->flag & GP_LAYER_LOCKED) ? "true" : "false";
    key(s, "opacity"); put_num(s, gpl->opacity);
    key(s, "blend"); put_num(s, gpl->blend_mode);
    key(s, "tint"); put_vec(s, gpl->tintcolor, 4);
    key(s, "lineChange"); put_num(s, gpl->line_change);
    key(s, "pass"); put_num(s, gpl->pass_index);
    key(s, "frames");
    s += '[';
    bool ff = true;
    FOREACH_READ (const bGPDframe, gpf, fd, gpl->frames) {
      if (!ff) s += ',';
      ff = false;
      s += '{';
      key(s, "number"); put_num(s, gpf->framenum);
      key(s, "keyType"); put_num(s, gpf->key_type);
      key(s, "strokes");
      s += '[';
      bool fs = true;
      FOREACH_READ (const bGPDstroke, gps, fd, gpf->strokes) {
        if (gps->totpoints <= 0 || !fd.R(gps->points)) continue;
        if (!fs) s += ',';
        fs = false;
        import_stroke(s, fd, gps);
      }
      s += "]}";
    }
    s += ']';
    key(s, "modifiers");
    s += '[';
    bool fm = true;
    for (const ImportedMod &m : mods) {
      if (!m.layer.empty() && m.layer != gpl->info) continue;
      if (!fm) s += ',';
      fm = false;
      s += "{\"type\":";
      put_num(s, m.type);
      s += m.enabled ? ",\"enabled\":true" : ",\"enabled\":false";
      key(s, "params");
      put_vec(s, m.params.data(), (int)m.params.size());
      s += '}';
    }
    s += ']';
    key(s, "useMask"); s += (gpl->flag & GP_LAYER_USE_MASK) ? "true" : "false";
    key(s, "masks");
    s += '[';
    bool fk = true;
    FOREACH_READ (const bGPDlayer_Mask, mask, fd, gpl->mask_layers) {
      if (!fk) s += ',';
      fk = false;
      s += "{\"name\":";
      put_str(s, mask->name);
      s += (mask->flag & GP_MASK_HIDE) ? ",\"hidden\":true" : ",\"hidden\":false";
      s += (mask->flag & GP_MASK_INVERT) ? ",\"invert\":true}" : ",\"invert\":false}";
    }
    s += "]}";
  }
  s += ']';
  if (rig) import_armature(s, fd, rig);
  s += '}';
  return s;
}

/* ---------------------------------------------------------------------------------------- */
/* Save: writefile.cc                                                                        */

struct WriteData {
  std::string out;
  const SDNA *sdna;
  void mywrite(const void *p, size_t n) { out.append(static_cast<const char *>(p), n); }

  /* writestruct_at_address_nr */
  void writestruct_at_address_nr(int filecode, int nr_struct, int nr, const void *adr, const void *data)
  {
    if (adr == nullptr || data == nullptr || nr == 0 || nr_struct <= 0) return;
    BHead bh;
    bh.code = filecode;
    bh.old = adr;
    bh.nr = nr;
    bh.SDNAnr = nr_struct;
    const SDNA_Struct *struct_info = sdna->structs[bh.SDNAnr];
    bh.len = nr * sdna->types_size[struct_info->type];
    if (bh.len == 0) return;
    mywrite(&bh, sizeof(BHead));
    mywrite(data, size_t(bh.len));
  }
  /* BLO_write_struct: the struct number from dna_type_offsets.h (SDNA_TYPE_FROM_STRUCT), as Blender */
  template<class T> void writestruct(int filecode, int n, int nr, const T *adr, const T *data = nullptr)
  {
    /* the in-memory struct is the one the DNA1 block describes */
    BLI_assert(n > 0 && sdna->types_size[sdna->structs[n]->type] == (int)sizeof(T));
    writestruct_at_address_nr(filecode, n, nr, adr, data ? data : adr);
  }
  /* writedata: raw data (strings, pointer arrays), length padded to 4 */
  void writedata(int filecode, size_t len, const void *adr)
  {
    if (adr == nullptr || len == 0) return;
    const size_t padded = (len + 3) & ~size_t(3);
    BHead bh;
    bh.code = filecode;
    bh.old = adr;
    bh.nr = 1;
    bh.SDNAnr = 0;
    bh.len = int(padded);
    mywrite(&bh, sizeof(BHead));
    mywrite(adr, len);
    static const char zero[4] = {0, 0, 0, 0};
    mywrite(zero, padded - len);
  }
};

#define WS(wd, T, nr, adr) (wd).writestruct(BLO_CODE_DATA, SDNA_TYPE_FROM_STRUCT(T), (nr), static_cast<const T *>(adr))
#define WS_ID(wd, T, adr) \
  (wd).writestruct(GS(static_cast<const T *>(adr)->id.name), SDNA_TYPE_FROM_STRUCT(T), 1, static_cast<const T *>(adr))

/* Everything the save allocates, freed at the end (the pointers are also the file's "old" addresses). */
struct Arena {
  std::vector<void *> blocks;
  ~Arena() { for (void *p : blocks) MEM_freeN(p); }
  template<class T> T *calloc(int n = 1)
  {
    T *p = static_cast<T *>(MEM_callocN(sizeof(T) * size_t(MAX2(n, 1)), "pg_blend_write"));
    blocks.push_back(p);
    return p;
  }
  template<class T> T *dna_default(const T *def)
  {
    T *p = calloc<T>();
    std::memcpy(p, def, sizeof(T));
    return p;
  }
  char *str(const std::string &s)
  {
    char *p = static_cast<char *>(MEM_callocN(s.size() + 1, "pg_blend_str"));
    std::memcpy(p, s.c_str(), s.size());
    blocks.push_back(p);
    return p;
  }
};

void id_init(ID *id, short code, const char *name)
{
  *reinterpret_cast<short *>(id->name) = code;
  BLI_strncpy(id->name + 2, name, sizeof(id->name) - 2);
  id->us = 1;
}

/* A modifier the app has (Thickness, Tint, Armature) as Blender's, filtered to its layer. */
GpencilModifierData *export_modifier(Arena &mem, const JV &jm, const char *layer,
                                     const std::vector<std::string> &groups, Object *rig, int *r_struct)
{
  const int type = (int)jm.n("type", 0);
  const JV *pv = jm.get("params");
  JV none;
  const JV &p = pv ? *pv : none;
  auto group = [&](int i) { return i >= 0 && i < (int)groups.size() ? groups[i].c_str() : ""; };
  GpencilModifierData *md = nullptr;
  if (type == PG_MOD_THICKNESS) {
    auto *t = mem.dna_default(DNA_struct_default_get(ThickGpencilModifierData));
    t->modifier.type = eGpencilModifierType_Thick;
    BLI_strncpy(t->modifier.name, "Thickness", sizeof(t->modifier.name));
    SET_FLAG_FROM_TEST(t->flag, p.at(PG_P_THICK_NORMALIZE, 0) != 0, GP_THICK_NORMALIZE);
    t->thickness = (int)p.at(PG_P_THICK_THICKNESS, 30);
    t->thickness_fac = (float)p.at(PG_P_THICK_FACTOR, 1);
    if (p.at(PG_P_THICK_USE_VGROUP, 0) != 0)
      BLI_strncpy(t->vgname, group((int)p.at(PG_P_THICK_VGROUP, 0)), sizeof(t->vgname));
    SET_FLAG_FROM_TEST(t->flag, p.at(PG_P_THICK_INVERT_VGROUP, 0) != 0, GP_THICK_INVERT_VGROUP);
    BLI_strncpy(t->layername, layer, sizeof(t->layername));
    t->curve_thickness = nullptr; /* GP_THICK_CUSTOM_CURVE is off */
    md = &t->modifier;
    *r_struct = SDNA_TYPE_FROM_STRUCT(ThickGpencilModifierData);
  }
  else if (type == PG_MOD_TINT) {
    auto *t = mem.dna_default(DNA_struct_default_get(TintGpencilModifierData));
    t->modifier.type = eGpencilModifierType_Tint;
    BLI_strncpy(t->modifier.name, "Tint", sizeof(t->modifier.name));
    t->mode = (int)p.at(PG_P_TINT_MODE, GPPAINT_MODE_BOTH);
    t->factor = (float)p.at(PG_P_TINT_FACTOR, 0.5);
    for (int c = 0; c < 3; c++) t->rgb[c] = (float)p.at(PG_P_TINT_R + c, 1);
    BLI_strncpy(t->layername, layer, sizeof(t->layername));
    t->curve_intensity = nullptr;
    t->colorband = nullptr;
    md = &t->modifier;
    *r_struct = SDNA_TYPE_FROM_STRUCT(TintGpencilModifierData);
  }
  else if (type == PG_MOD_ARMATURE) {
    auto *a = mem.dna_default(DNA_struct_default_get(ArmatureGpencilModifierData));
    a->modifier.type = eGpencilModifierType_Armature;
    BLI_strncpy(a->modifier.name, "Armature", sizeof(a->modifier.name));
    a->deformflag = 0;
    if (p.at(PG_P_ARMATURE_USE_VGROUPS, 1) != 0) a->deformflag |= ARM_DEF_VGROUP;
    if (p.at(PG_P_ARMATURE_USE_ENVELOPE, 0) != 0) a->deformflag |= ARM_DEF_ENVELOPE;
    if (p.at(PG_P_ARMATURE_PRESERVE_VOLUME, 0) != 0) a->deformflag |= ARM_DEF_QUATERNION;
    if (p.at(PG_P_ARMATURE_INVERT_VGROUP, 0) != 0) a->deformflag |= ARM_DEF_INVERT_VGROUP;
    BLI_strncpy(a->vgname, group((int)p.at(PG_P_ARMATURE_VGROUP, 0) - 1), sizeof(a->vgname));
    a->object = rig;
    md = &a->modifier;
    *r_struct = SDNA_TYPE_FROM_STRUCT(ArmatureGpencilModifierData);
  }
  if (md) {
    md->mode = eGpencilModifierMode_Realtime | eGpencilModifierMode_Render;
    if (!jm.n("enabled", 1)) md->mode = 0;
    md->flag = eGpencilModifierFlag_OverrideLibrary_Local;
    md->ui_expand_flag = 1;
  }
  return md;
}

struct ExportedMod {
  GpencilModifierData *md;
  int struct_nr; /* SDNA_TYPE_FROM_STRUCT of its type (BLO_write_struct_by_name) */
};

bool export_document(const JV &root, std::string &out, char *err, int en)
{
  Arena mem;
  WriteData wd;
  wd.sdna = mem_sdna();

  /* ---- data (as BKE_*_add / init_data would make it) */
  Scene *sce = mem.dna_default(DNA_struct_default_get(Scene));
  id_init(&sce->id, ID_SCE, "Scene");
  sce->r.frs_sec = (short)CLAMPIS((int)root.n("fps", sce->r.frs_sec), 1, 120);
  sce->r.cfra = MAX2((int)root.n("frame", 1), 1);
  if (root.n("frameEnd", 0) > 0) sce->r.efra = (int)root.n("frameEnd", sce->r.efra);
  sce->toolsettings = mem.dna_default(DNA_struct_default_get(ToolSettings));
  Collection *master = mem.dna_default(DNA_struct_default_get(Collection));
  id_init(&master->id, ID_GR, "Scene Collection");
  master->id.flag |= LIB_EMBEDDED_DATA;
  sce->master_collection = master;
  ViewLayer *vl = mem.calloc<ViewLayer>(); /* view_layer_add */
  vl->flag = VIEW_LAYER_RENDER | VIEW_LAYER_FREESTYLE;
  BLI_strncpy(vl->name, "ViewLayer", sizeof(vl->name));
  vl->layflag = SCE_LAY_FLAG_DEFAULT;
  vl->passflag = SCE_PASS_COMBINED;
  vl->pass_alpha_threshold = 0.5f;
  vl->cryptomatte_levels = 6;
  vl->cryptomatte_flag = VIEW_LAYER_CRYPTOMATTE_ACCURATE;
  vl->freestyle_config.mode = FREESTYLE_CONTROL_EDITOR_MODE; /* BKE_freestyle_config_init */
  vl->freestyle_config.sphere_radius = 0.1f;
  vl->freestyle_config.dkr_epsilon = 0.0f;
  vl->freestyle_config.crease_angle = DEG2RADF(134.43f);
  BLI_addtail(&sce->view_layers, vl);
  LayerCollection *lc = mem.calloc<LayerCollection>();
  lc->collection = master;
  BLI_addtail(&vl->layer_collections, lc);

  /* GP object */
  bGPdata *gpd = mem.calloc<bGPdata>(); /* BKE_gpencil_data_addnew */
  id_init(&gpd->id, ID_GD_LEGACY, "Drawing");
  gpd->flag = GP_DATA_DISPINFO | GP_DATA_EXPAND | GP_DATA_VIEWALIGN | GP_DATA_SHOW_ONIONSKINS |
              GP_DATA_CURVE_ADAPTIVE_RESOLUTION;
  ARRAY_SET_ITEMS(gpd->line_color, 0.6f, 0.6f, 0.6f, 0.5f);
  gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
  gpd->curve_edit_resolution = GP_DEFAULT_CURVE_RESOLUTION;
  gpd->curve_edit_threshold = GP_DEFAULT_CURVE_ERROR;
  gpd->curve_edit_corner_angle = GP_DEFAULT_CURVE_EDIT_CORNER_ANGLE;
  gpd->zdepth_offset = 0.150f;
  ARRAY_SET_ITEMS(gpd->grid.color, 0.5f, 0.5f, 0.5f);
  ARRAY_SET_ITEMS(gpd->grid.scale, 1.0f, 1.0f);
  gpd->grid.lines = GP_DEFAULT_GRID_LINES;
  gpd->onion_keytype = -1;
  gpd->onion_flag = GP_ONION_GHOST_PREVCOL | GP_ONION_GHOST_NEXTCOL | GP_ONION_FADE;
  gpd->onion_mode = GP_ONION_MODE_RELATIVE;
  gpd->onion_factor = 0.5f;
  ARRAY_SET_ITEMS(gpd->gcolor_prev, 0.145098f, 0.419608f, 0.137255f);
  ARRAY_SET_ITEMS(gpd->gcolor_next, 0.125490f, 0.082353f, 0.529412f);
  gpd->gstep = 1;
  gpd->gstep_next = 1;

  Object *ob = mem.dna_default(DNA_struct_default_get(Object));
  id_init(&ob->id, ID_OB, "Drawing");
  ob->type = OB_GPENCIL_LEGACY;
  ob->data = gpd;

  /* materials */
  std::vector<Material *> mats;
  for (const JV &jm : root.list("materials")) {
    Material *ma = mem.dna_default(DNA_struct_default_get(Material));
    std::string name = jm.s("name");
    if (name.empty()) name = "Material";
    for (int k = 1; [&] { for (Material *o : mats) if (name == o->id.name + 2) return true; return false; }(); k++) {
      char b[80];
      std::snprintf(b, sizeof(b), "%.58s.%03d", jm.s("name").empty() ? "Material" : jm.s("name").c_str(), k);
      name = b; /* BKE_id_new_name_validate: names are unique in Main */
    }
    id_init(&ma->id, ID_MA, name.c_str());
    MaterialGPencilStyle *st = mem.calloc<MaterialGPencilStyle>(); /* BKE_gpencil_material_attr_init */
    ARRAY_SET_ITEMS(st->mix_rgba, 1.0f, 1.0f, 1.0f, 1.0f);
    ARRAY_SET_ITEMS(st->texture_scale, 1.0f, 1.0f);
    st->texture_offset[0] = -0.5f;
    st->texture_pixsize = 100.0f;
    st->mix_factor = 0.5f;
    for (int c = 0; c < 4; c++) {
      st->stroke_rgba[c] = (float)jm.get("stroke")->at(c, c == 3 ? 1 : 0);
      st->fill_rgba[c] = (float)jm.get("fill")->at(c, c == 3 ? 1 : 0);
    }
    SET_FLAG_FROM_TEST(st->flag, jm.n("strokeEnabled", 1) != 0, GP_MATERIAL_STROKE_SHOW);
    SET_FLAG_FROM_TEST(st->flag, jm.n("fillEnabled", 0) != 0, GP_MATERIAL_FILL_SHOW);
    SET_FLAG_FROM_TEST(st->flag, jm.n("visible", 1) == 0, GP_MATERIAL_HIDE);
    SET_FLAG_FROM_TEST(st->flag, jm.n("locked", 0) != 0, GP_MATERIAL_LOCKED);
    SET_FLAG_FROM_TEST(st->flag, jm.n("strokeHoldout", 0) != 0, GP_MATERIAL_IS_STROKE_HOLDOUT);
    SET_FLAG_FROM_TEST(st->flag, jm.n("fillHoldout", 0) != 0, GP_MATERIAL_IS_FILL_HOLDOUT);
    SET_FLAG_FROM_TEST(st->flag, jm.n("selfOverlap", 0) != 0, GP_MATERIAL_DISABLE_STENCIL);
    st->mode = (int)jm.n("mode", 0);
    st->alignment_mode = (int)jm.n("alignment", 0);
    st->alignment_rotation = (float)jm.n("rotation", 0);
    st->index = (int)jm.n("pass", 0);
    if (const JV *g = jm.get("gradient"); g && g->kind == JV::ARR) {
      st->fill_style = GP_MATERIAL_FILL_STYLE_GRADIENT;
      st->gradient_type = (int)g->at(0, 0);
      for (int c = 0; c < 4; c++) st->mix_rgba[c] = (float)g->at(1 + c, 1);
      st->mix_factor = (float)g->at(5, 0.5);
      st->texture_angle = (float)g->at(6, 0);
      st->texture_scale[0] = (float)g->at(7, 1);
      st->texture_scale[1] = (float)g->at(8, 1);
      st->texture_offset[0] = (float)g->at(9, -0.5);
      st->texture_offset[1] = (float)g->at(10, 0);
      SET_FLAG_FROM_TEST(st->flag, g->at(11, 0) != 0, GP_MATERIAL_FLIP_FILL);
    }
    ma->gp_style = st;
    mats.push_back(ma);
  }
  gpd->totcol = ob->totcol = (int)mats.size();
  if (!mats.empty()) {
    gpd->mat = mem.calloc<Material *>((int)mats.size());
    for (size_t i = 0; i < mats.size(); i++) gpd->mat[i] = mats[i];
    ob->mat = mem.calloc<Material *>((int)mats.size()); /* data-linked slots (matbits 0) */
    ob->matbits = mem.calloc<char>((int)mats.size());
    ob->actcol = 1;
  }

  /* vertex groups */
  std::vector<std::string> groups;
  for (const JV &g : root.list("vertexGroups")) {
    bDeformGroup *dg = mem.calloc<bDeformGroup>();
    BLI_strncpy(dg->name, g.kind == JV::STR && !g.str.empty() ? g.str.c_str() : "Group", sizeof(dg->name));
    BLI_addtail(&gpd->vertex_group_names, dg);
    groups.push_back(dg->name);
  }
  gpd->vertex_group_active_index = groups.empty() ? 0 : (int)root.n("activeVertexGroup", -1) + 1;

  /* armature (pg_arm_from_json: the app's own reading of its rig) */
  PGArmature *a = nullptr;
  Object *rig = nullptr;
  bAction *act = nullptr;
  if (const JV *ja = root.get("armature"); ja && ja->kind == JV::OBJ) {
    std::string text;
    /* the rig's JSON text again: re-serialize the sub-tree */
    std::function<void(const JV &)> dump = [&](const JV &v) {
      switch (v.kind) {
        case JV::NUL: text += "null"; break;
        case JV::BOOL: text += v.num ? "true" : "false"; break;
        case JV::NUM: put_num(text, v.num); break;
        case JV::STR: put_str(text, v.str.c_str()); break;
        case JV::ARR: text += '['; for (size_t i = 0; i < v.arr.size(); i++) { if (i) text += ','; dump(v.arr[i]); } text += ']'; break;
        case JV::OBJ: {
          text += '{';
          bool f = true;
          for (const auto &kv : v.obj) { if (!f) text += ','; f = false; put_str(text, kv.first.c_str()); text += ':'; dump(kv.second); }
          text += '}';
        }
      }
    };
    dump(*ja);
    a = pg_arm_from_json(text.c_str());
    if (!a) { set_err(err, en, "Bad armature in the project"); return false; }
    pg_arm_set_mode(a, PG_ARM_MODE_OBJECT); /* the bone tree (ED_armature_from_edit) is current */
    pg_arm_update(a);
    rig = mem.dna_default(DNA_struct_default_get(Object));
    id_init(&rig->id, ID_OB, "Armature");
    rig->type = OB_ARMATURE;
    id_init(&a->arm.id, ID_AR, "Armature");
    rig->data = &a->arm;
    rig->pose = &a->pose;
    SET_FLAG_FROM_TEST(rig->dtx, a->show_in_front, OB_DRAW_IN_FRONT);
    if (!BLI_listbase_is_empty(&a->curves)) {
      act = mem.calloc<bAction>();
      id_init(&act->id, ID_AC, "ArmatureAction");
      act->idroot = ID_OB;
      act->curves = a->curves;
      rig->adt = mem.calloc<AnimData>();
      rig->adt->action = act;
      rig->adt->act_influence = 1.0f; /* BKE_animdata_ensure_id */
    }
  }

  /* layers / frames / strokes (BKE_gpencil_layer_addnew & co.) */
  std::vector<ExportedMod> mods, mods_after;
  ExportedMod armature_mod{nullptr, 0};
  for (const JV &jl : root.list("layers")) {
    std::string name = jl.s("name");
    char auto_name[32];
    std::snprintf(auto_name, sizeof(auto_name), "Layer %d", (int)BLI_listbase_count(&gpd->layers) + 1);
    bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, name.empty() ? auto_name : name.c_str(), false, false);
    SET_FLAG_FROM_TEST(gpl->flag, jl.n("visible", 1) == 0, GP_LAYER_HIDE);
    SET_FLAG_FROM_TEST(gpl->flag, jl.n("locked", 0) != 0, GP_LAYER_LOCKED);
    gpl->opacity = (float)jl.n("opacity", 1);
    gpl->blend_mode = (short)jl.n("blend", 0);
    if (const JV *t = jl.get("tint")) for (int c = 0; c < 4; c++) gpl->tintcolor[c] = (float)t->at(c, 0);
    gpl->line_change = (short)jl.n("lineChange", 0);
    gpl->pass_index = (int)jl.n("pass", 0);
    for (const JV &jf : jl.list("frames")) {
      bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, MAX2((int)jf.n("number", 1), 1));
      if (!gpf) continue; /* a duplicate frame number */
      gpf->key_type = (short)jf.n("keyType", 0);
      for (const JV &js : jf.list("strokes")) {
        const std::vector<JV> &pts = js.list("points");
        if (pts.empty()) continue;
        bGPDstroke *gps = BKE_gpencil_stroke_new((int)js.n("material", 0), (int)pts.size(),
                                                 (short)js.n("thickness", 1));
        for (size_t i = 0; i < pts.size(); i++) {
          bGPDspoint &p = gps->points[i];
          p.x = (float)pts[i].at(0, 0) / PPU;
          p.y = -(float)pts[i].at(1, 0) / PPU;
          p.z = (float)pts[i].at(2, 0) / PPU;
          p.pressure = (float)pts[i].at(3, 1);
          p.strength = (float)pts[i].at(4, 1);
          p.time = (float)pts[i].at(5, 0);
          for (int c = 0; c < 4; c++) p.vert_color[c] = (float)pts[i].at(6 + c, 0);
        }
        SET_FLAG_FROM_TEST(gps->flag, js.n("cyclic", 0) != 0, GP_STROKE_CYCLIC);
        gps->fill_opacity_fac = (float)js.n("fillOpacity", 1);
        if (const JV *fc = js.get("fillColor")) for (int c = 0; c < 4; c++) gps->vert_color_fill[c] = (float)fc->at(c, 0);
        if (const JV *caps = js.get("caps")) { gps->caps[0] = (short)caps->at(0, 0); gps->caps[1] = (short)caps->at(1, 0); }
        gps->hardeness = (float)js.n("hardness", 1);
        for (const JV &row : js.list("weights")) {
          const int pi = (int)row.at(0, -1);
          if (pi < 0 || pi >= gps->totpoints || row.arr.size() < 3) continue;
          BKE_gpencil_dvert_ensure(gps);
          MDeformVert *dv = &gps->dvert[pi];
          for (size_t k = 1; k + 1 < row.arr.size(); k += 2) {
            const int def_nr = (int)row.at(k, -1);
            if (def_nr < 0 || def_nr >= (int)groups.size()) continue;
            dv->dw = static_cast<MDeformWeight *>(
                MEM_reallocN_id(dv->dw, sizeof(MDeformWeight) * size_t(dv->totweight + 1), "dw"));
            dv->dw[dv->totweight].def_nr = def_nr;
            dv->dw[dv->totweight].weight = (float)row.at(k + 1, 0);
            dv->totweight++;
          }
        }
        BLI_addtail(&gpf->strokes, gps);
        BKE_gpencil_stroke_geometry_update(gpd, gps); /* triangles + uv, as after drawing */
      }
    }
    /* The app's stacks are per layer, Blender's per object (layer filter). One object-level Armature
     * modifier (its deform is per object); a layer's modifiers before / after it stay before / after. */
    bool after_armature = false;
    for (const JV &jm : jl.list("modifiers")) {
      int sn = 0;
      if (GpencilModifierData *md = export_modifier(mem, jm, gpl->info, groups, rig, &sn)) {
        if (md->type == eGpencilModifierType_Armature) {
          after_armature = true;
          if (!armature_mod.md) armature_mod = {md, sn};
          continue;
        }
        (after_armature ? mods_after : mods).push_back({md, sn});
      }
    }
  }
  if (armature_mod.md) mods.push_back(armature_mod);
  mods.insert(mods.end(), mods_after.begin(), mods_after.end());
  for (const ExportedMod &e : mods) BLI_addtail(&ob->greasepencil_modifiers, e.md);
  /* masks name layers: after all of them */
  {
    bGPDlayer *gpl = static_cast<bGPDlayer *>(gpd->layers.first);
    for (const JV &jl : root.list("layers")) {
      if (!gpl) break;
      SET_FLAG_FROM_TEST(gpl->flag, jl.n("useMask", 0) != 0, GP_LAYER_USE_MASK);
      for (const JV &jk : jl.list("masks")) {
        if (!BKE_gpencil_layer_named_get(gpd, jk.s("name").c_str())) continue;
        bGPDlayer_Mask *mask = BKE_gpencil_layer_mask_add(gpl, jk.s("name").c_str());
        SET_FLAG_FROM_TEST(mask->flag, jk.n("hidden", 0) != 0, GP_MASK_HIDE);
        SET_FLAG_FROM_TEST(mask->flag, jk.n("invert", 0) != 0, GP_MASK_INVERT);
      }
      gpl = gpl->next;
    }
  }
  if (gpd->layers.first) BKE_gpencil_layer_active_set(gpd, static_cast<bGPDlayer *>(gpd->layers.first));

  /* scene contents: master collection objects + view layer bases */
  for (Object *o : {ob, rig}) {
    if (!o) continue;
    CollectionObject *cob = mem.calloc<CollectionObject>();
    cob->ob = o;
    BLI_addtail(&master->gobject, cob);
    Base *base = mem.calloc<Base>();
    base->object = o;
    base->flag = BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT | BASE_SELECTABLE;
    BLI_addtail(&vl->object_bases, base);
  }

  /* ---- write_file_handle */
  char buf[16];
  SNPRINTF(buf, "BLENDER%c%c%.3d", (sizeof(void *) == 8) ? '-' : '_', (ENDIAN_ORDER == B_ENDIAN) ? 'V' : 'v',
           BLENDER_FILE_VERSION);
  wd.mywrite(buf, 12);

  /* write_global */
  FileGlobal fg{};
  char subvstr[8];
  fg.curscene = sce;
  fg.cur_view_layer = vl;
  SNPRINTF(subvstr, "%4d", BLENDER_FILE_SUBVERSION);
  memcpy(fg.subvstr, subvstr, 4);
  fg.subversion = BLENDER_FILE_SUBVERSION;
  fg.minversion = BLENDER_FILE_MIN_VERSION;
  fg.minsubversion = BLENDER_FILE_MIN_SUBVERSION;
  STRNCPY(fg.build_hash, "unknown");
  wd.writestruct(BLO_CODE_GLOB, SDNA_TYPE_FROM_STRUCT(FileGlobal), 1, &fg);

  /* scene_blend_write */
  WS_ID(wd, Scene, sce);
  WS(wd, ToolSettings, 1, sce->toolsettings);
  WS(wd, ViewLayer, 1, vl); /* BKE_view_layer_blend_write */
  LISTBASE_FOREACH (Base *, base, &vl->object_bases) WS(wd, Base, 1, base);
  WS(wd, LayerCollection, 1, lc);
  WS(wd, Collection, 1, master); /* BLO_write_struct_at_address + BKE_collection_blend_write_nolib */
  LISTBASE_FOREACH (CollectionObject *, cob, &master->gobject) WS(wd, CollectionObject, 1, cob);

  /* object_blend_write (GP) */
  WS_ID(wd, Object, ob);
  if (ob->totcol) {
    wd.writedata(BLO_CODE_DATA, sizeof(void *) * size_t(ob->totcol), ob->mat); /* BLO_write_pointer_array */
    wd.writedata(BLO_CODE_DATA, size_t(ob->totcol), ob->matbits);
  }
  for (const ExportedMod &e : mods) {
    wd.writestruct_at_address_nr(BLO_CODE_DATA, e.struct_nr, 1, e.md, e.md); /* BLO_write_struct_by_name */
  }

  /* greasepencil_blend_write */
  WS_ID(wd, bGPdata, gpd);
  LISTBASE_FOREACH (bDeformGroup *, dg, &gpd->vertex_group_names) WS(wd, bDeformGroup, 1, dg);
  if (gpd->totcol) wd.writedata(BLO_CODE_DATA, sizeof(void *) * size_t(gpd->totcol), gpd->mat);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    memset(&gpl->runtime, 0, sizeof(gpl->runtime));
    WS(wd, bGPDlayer, 1, gpl);
    LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl->mask_layers) WS(wd, bGPDlayer_Mask, 1, mask);
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      WS(wd, bGPDframe, 1, gpf);
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        memset(&gps->runtime, 0, sizeof(gps->runtime));
        WS(wd, bGPDstroke, 1, gps);
        WS(wd, bGPDspoint, gps->totpoints, gps->points);
        WS(wd, bGPDtriangle, gps->tot_triangles, gps->triangles);
        if (gps->dvert) { /* BKE_defvert_blend_write */
          WS(wd, MDeformVert, gps->totpoints, gps->dvert);
          for (int i = 0; i < gps->totpoints; i++)
            if (gps->dvert[i].dw) WS(wd, MDeformWeight, gps->dvert[i].totweight, gps->dvert[i].dw);
        }
      }
    }
  }

  /* material_blend_write */
  for (Material *ma : mats) {
    WS_ID(wd, Material, ma);
    WS(wd, MaterialGPencilStyle, 1, ma->gp_style);
  }

  if (a) {
    /* object_blend_write (armature) */
    WS_ID(wd, Object, rig);
    if (rig->adt) WS(wd, AnimData, 1, rig->adt);
    /* BKE_pose_blend_write: channels (no constraints / properties, see the header), then the pose */
    std::vector<bPoseChannel> chans;
    LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
      bPoseChannel c = *pc;
      c.selectflag = pc->bone ? (pc->bone->flag & BONE_SELECTED) : 0;
      c.prop = nullptr;
      BLI_listbase_clear(&c.constraints);
      c.mpath = nullptr;
      c.custom = nullptr;
      c.custom_tx = nullptr;
      c.bone = nullptr;
      chans.push_back(c);
    }
    size_t ci = 0;
    LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) wd.writestruct(BLO_CODE_DATA, SDNA_TYPE_FROM_STRUCT(bPoseChannel), 1, pc, &chans[ci++]);
    bPose pose = a->pose;
    pose.chanhash = nullptr;
    pose.chan_array = nullptr;
    pose.ikdata = nullptr;
    pose.ikparam = nullptr;
    BLI_listbase_clear(&pose.agroups);
    wd.writestruct(BLO_CODE_DATA, SDNA_TYPE_FROM_STRUCT(bPose), 1, &a->pose, &pose);

    /* armature_blend_write + write_bone */
    bArmature arm = a->arm;
    arm.bonehash = nullptr;
    arm.edbo = nullptr;
    arm.needs_flush_to_id = 0;
    arm.act_edbone = nullptr;
    arm.adt = nullptr;
    wd.writestruct(ID_AR, SDNA_TYPE_FROM_STRUCT(bArmature), 1, &a->arm, &arm);
    std::function<void(Bone *)> write_bone = [&](Bone *bone) {
      bone->size[0] = bone->size[1] = bone->size[2] = 1.0f; /* PATCH for upward compatibility */
      Bone b = *bone;
      b.prop = nullptr;
      wd.writestruct(BLO_CODE_DATA, SDNA_TYPE_FROM_STRUCT(Bone), 1, bone, &b);
      LISTBASE_FOREACH (Bone *, cbone, &bone->childbase) write_bone(cbone);
    };
    LISTBASE_FOREACH (Bone *, bone, &a->arm.bonebase) write_bone(bone);

    if (act) { /* action_blend_write + BKE_fcurve_blend_write */
      WS_ID(wd, bAction, act);
      LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
        FCurve c = *fcu;
        c.grp = nullptr;
        c.driver = nullptr;
        BLI_listbase_clear(&c.modifiers);
        wd.writestruct(BLO_CODE_DATA, SDNA_TYPE_FROM_STRUCT(FCurve), 1, fcu, &c);
      }
      LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
        WS(wd, BezTriple, fcu->totvert, fcu->bezt);
        if (fcu->rna_path) wd.writedata(BLO_CODE_DATA, strlen(fcu->rna_path) + 1, fcu->rna_path); /* BLO_write_string */
      }
    }
  }

  /* DNA1 last, then ENDB */
  wd.writedata(BLO_CODE_DNA1, size_t(wd.sdna->data_len), wd.sdna->data);
  BHead bhead;
  memset(&bhead, 0, sizeof(BHead));
  bhead.code = BLO_CODE_ENDB;
  wd.mywrite(&bhead, sizeof(BHead));

  /* free what BKE allocated (the arena frees the rest) */
  BKE_gpencil_free_layers(&gpd->layers);
  if (a) {
    pg_arm_free(a); /* act->curves is a shallow copy of a->curves */
  }
  out.swap(wd.out);
  return true;
}

}  // namespace

extern "C" char *pg_blend_to_project_json(const void *data, size_t len, char *err, int err_len)
{
  if (!data) { set_err(err, err_len, "No data"); return nullptr; }
  BlendFileData fd;
  if (!open_blend(fd, data, len, err, err_len)) return nullptr;
  const std::string json = import_document(fd);
  if (json.empty()) { set_err(err, err_len, "The file has no Grease Pencil (legacy) object"); return nullptr; }
  return dup_out(json);
}

extern "C" void *pg_project_json_to_blend(const char *json, size_t *r_len, char *err, int err_len)
{
  if (!json || !r_len) { set_err(err, err_len, "No data"); return nullptr; }
  JV root;
  JP parser{json};
  if (!parser.parse(root) || root.kind != JV::OBJ) { set_err(err, err_len, "Not a project document"); return nullptr; }
  std::string out;
  if (!export_document(root, out, err, err_len)) return nullptr;
  void *bytes = std::malloc(out.size());
  if (!bytes) { set_err(err, err_len, "Out of memory"); return nullptr; }
  std::memcpy(bytes, out.data(), out.size());
  *r_len = out.size();
  return bytes;
}
