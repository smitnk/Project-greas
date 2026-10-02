/* Host test for project_grease_document_state.c: save/load round trip of the state that the
 * point API does not carry (stroke style, layer state, material palette). The "saved" form is a
 * plain array of Info structs, exactly what the JNI getters hand to the JSON writer. */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "project_grease_document_state.h"

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static bool same4(const float *a, const float *b)
{
  return memcmp(a, b, 4 * sizeof(float)) == 0;
}

static void make_material(Material *ma, MaterialGPencilStyle *style)
{
  memset(ma, 0, sizeof(*ma));
  memset(style, 0, sizeof(*style));
  ma->gp_style = style;
}

static void test_stroke_roundtrip(void)
{
  bGPDstroke src;
  memset(&src, 0, sizeof(src));
  src.mat_nr = 3;
  src.thickness = 14;
  src.flag = GP_STROKE_CYCLIC | GP_STROKE_SELECT;
  src.fill_opacity_fac = 0.25f;
  const float fill[4] = {0.1f, 0.2f, 0.3f, 0.4f};
  memcpy(src.vert_color_fill, fill, sizeof(fill));

  PGStrokeInfo saved;
  CHECK(pg_doc_stroke_info_get(&src, &saved));
  CHECK(saved.material_index == 3 && saved.thickness == 14.0f && saved.cyclic == 1);
  CHECK(saved.fill_opacity_fac == 0.25f && same4(saved.fill_color, fill));

  bGPDstroke dst;
  memset(&dst, 0, sizeof(dst));
  dst.mat_nr = 0;
  dst.thickness = 1;
  dst.flag = GP_STROKE_SELECT; /* unrelated flags must survive */
  CHECK(pg_doc_stroke_info_apply(&dst, &saved));
  CHECK(dst.mat_nr == 3 && dst.thickness == 14 && (dst.flag & GP_STROKE_CYCLIC) != 0);
  CHECK((dst.flag & GP_STROKE_SELECT) != 0);
  CHECK(dst.fill_opacity_fac == 0.25f && same4(dst.vert_color_fill, fill));

  /* open stroke: the cyclic flag is cleared again */
  saved.cyclic = 0;
  CHECK(pg_doc_stroke_info_apply(&dst, &saved));
  CHECK((dst.flag & GP_STROKE_CYCLIC) == 0 && (dst.flag & GP_STROKE_SELECT) != 0);

  /* hostile values are clamped, never stored raw */
  PGStrokeInfo bad = saved;
  bad.material_index = -5;
  bad.thickness = 1.0e9f;
  bad.fill_opacity_fac = NAN;
  CHECK(pg_doc_stroke_info_apply(&dst, &bad));
  CHECK(dst.mat_nr == 0 && dst.thickness == 32767 && dst.fill_opacity_fac == 0.0f);
  bad.thickness = 0.0f;
  CHECK(pg_doc_stroke_info_apply(&dst, &bad));
  CHECK(dst.thickness == 1);

  CHECK(!pg_doc_stroke_info_get(NULL, &saved));
  CHECK(!pg_doc_stroke_info_apply(&dst, NULL));
}

static void test_layer_roundtrip(void)
{
  bGPDlayer src;
  memset(&src, 0, sizeof(src));
  strcpy(src.info, "Inks");
  src.flag = GP_LAYER_HIDE | GP_LAYER_LOCKED;
  src.opacity = 0.5f;

  PGLayerInfo saved;
  CHECK(pg_doc_layer_info_get(&src, &saved));
  CHECK(strcmp(saved.name, "Inks") == 0 && saved.visible == 0 && saved.locked == 1);
  CHECK(saved.opacity == 0.5f);

  bGPDlayer dst;
  memset(&dst, 0, sizeof(dst));
  strcpy(dst.info, "GP_Layer");
  dst.opacity = 1.0f;
  CHECK(pg_doc_layer_info_apply(&dst, &saved));
  CHECK(strcmp(dst.info, "Inks") == 0);
  CHECK((dst.flag & GP_LAYER_HIDE) != 0 && (dst.flag & GP_LAYER_LOCKED) != 0);
  CHECK(dst.opacity == 0.5f);

  /* visible + unlocked clears both flags and leaves other flags alone */
  dst.flag |= GP_LAYER_UNLOCK_COLOR;
  saved.visible = 1;
  saved.locked = 0;
  CHECK(pg_doc_layer_info_apply(&dst, &saved));
  CHECK((dst.flag & (GP_LAYER_HIDE | GP_LAYER_LOCKED)) == 0 && (dst.flag & GP_LAYER_UNLOCK_COLOR) != 0);

  /* an empty name keeps the current one; an over-long name is truncated and terminated */
  PGLayerInfo nameless = saved;
  nameless.name[0] = '\0';
  CHECK(pg_doc_layer_info_apply(&dst, &nameless));
  CHECK(strcmp(dst.info, "Inks") == 0);
  PGLayerInfo longname = saved;
  memset(longname.name, 'x', sizeof(longname.name));
  CHECK(pg_doc_layer_info_apply(&dst, &longname));
  CHECK(strlen(dst.info) == sizeof(dst.info) - 1);

  /* opacity is clamped */
  saved.opacity = 7.0f;
  CHECK(pg_doc_layer_info_apply(&dst, &saved) && dst.opacity == 1.0f);
  saved.opacity = -1.0f;
  CHECK(pg_doc_layer_info_apply(&dst, &saved) && dst.opacity == 0.0f);

  CHECK(!pg_doc_layer_info_get(&src, NULL));
}

static void test_material_roundtrip(void)
{
  enum { N = 3 };
  Material src_ma[N], dst_ma[N];
  MaterialGPencilStyle src_st[N], dst_st[N];
  for (int i = 0; i < N; i++) {
    make_material(&src_ma[i], &src_st[i]);
    make_material(&dst_ma[i], &dst_st[i]);
    for (int c = 0; c < 4; c++) {
      src_st[i].stroke_rgba[c] = 0.1f * (float)(i + 1) + 0.01f * (float)c;
      src_st[i].fill_rgba[c] = 0.9f - 0.1f * (float)(i + c);
    }
  }
  src_st[1].flag = GP_MATERIAL_HIDE;
  src_st[2].flag = GP_MATERIAL_FILL_SHOW;

  /* save */
  PGMaterialInfo palette[N];
  for (int i = 0; i < N; i++) {
    CHECK(pg_doc_material_info_get(&src_ma[i], &palette[i]));
  }
  CHECK(palette[0].visible == 1 && palette[0].fill_enabled == 0);
  CHECK(palette[1].visible == 0 && palette[1].fill_enabled == 0);
  CHECK(palette[2].visible == 1 && palette[2].fill_enabled == 1);

  /* load into a document whose flags differ from the saved ones in every direction:
   * [0] must lose both flags, [1] must gain HIDE, [2] must gain FILL_SHOW and lose HIDE */
  const int start_flags[N] = {GP_MATERIAL_HIDE | GP_MATERIAL_FILL_SHOW, 0, GP_MATERIAL_HIDE};
  for (int i = 0; i < N; i++) {
    dst_st[i].flag = start_flags[i];
    CHECK(pg_doc_material_info_apply(&dst_ma[i], &palette[i]));
  }
  for (int i = 0; i < N; i++) {
    CHECK(same4(dst_st[i].stroke_rgba, src_st[i].stroke_rgba));
    CHECK(same4(dst_st[i].fill_rgba, src_st[i].fill_rgba));
    CHECK(((dst_st[i].flag & GP_MATERIAL_HIDE) != 0) == ((src_st[i].flag & GP_MATERIAL_HIDE) != 0));
    CHECK(((dst_st[i].flag & GP_MATERIAL_FILL_SHOW) != 0) ==
          ((src_st[i].flag & GP_MATERIAL_FILL_SHOW) != 0));
  }

  Material no_style;
  memset(&no_style, 0, sizeof(no_style));
  CHECK(!pg_doc_material_info_get(&no_style, &palette[0]));
  CHECK(!pg_doc_material_info_apply(&no_style, &palette[0]));
}

/* Whole-document pass: save every layer/stroke/material into flat arrays, wipe a second document,
 * restore it, and compare field by field. */
static void test_document_roundtrip(void)
{
  enum { LAYERS = 2, STROKES = 3 };
  bGPDlayer layers[LAYERS];
  bGPDstroke strokes[LAYERS][STROKES];
  memset(layers, 0, sizeof(layers));
  memset(strokes, 0, sizeof(strokes));
  for (int l = 0; l < LAYERS; l++) {
    snprintf(layers[l].info, sizeof(layers[l].info), "Layer %d", l + 1);
    layers[l].opacity = 1.0f - 0.25f * (float)l;
    if (l == 1) {
      layers[l].flag = GP_LAYER_HIDE | GP_LAYER_LOCKED;
    }
    for (int s = 0; s < STROKES; s++) {
      bGPDstroke *g = &strokes[l][s];
      g->mat_nr = (l + s) % 3;
      g->thickness = (short)(2 + 5 * s + l);
      g->flag = (s == 1) ? GP_STROKE_CYCLIC : 0;
      g->fill_opacity_fac = 0.2f * (float)(s + 1);
      g->vert_color_fill[0] = 0.5f * (float)s;
      g->vert_color_fill[3] = 1.0f;
    }
  }

  PGLayerInfo saved_layers[LAYERS];
  PGStrokeInfo saved_strokes[LAYERS][STROKES];
  for (int l = 0; l < LAYERS; l++) {
    CHECK(pg_doc_layer_info_get(&layers[l], &saved_layers[l]));
    for (int s = 0; s < STROKES; s++) {
      CHECK(pg_doc_stroke_info_get(&strokes[l][s], &saved_strokes[l][s]));
    }
  }

  bGPDlayer new_layers[LAYERS];
  bGPDstroke new_strokes[LAYERS][STROKES];
  memset(new_layers, 0, sizeof(new_layers));
  memset(new_strokes, 0, sizeof(new_strokes));
  for (int l = 0; l < LAYERS; l++) {
    strcpy(new_layers[l].info, "GP_Layer");
    new_layers[l].opacity = 1.0f;
  }
  CHECK(memcmp(&new_strokes[0][1], &strokes[0][1], sizeof(bGPDstroke)) != 0); /* really different */
  for (int l = 0; l < LAYERS; l++) {
    CHECK(pg_doc_layer_info_apply(&new_layers[l], &saved_layers[l]));
    for (int s = 0; s < STROKES; s++) {
      CHECK(pg_doc_stroke_info_apply(&new_strokes[l][s], &saved_strokes[l][s]));
    }
  }
  for (int l = 0; l < LAYERS; l++) {
    CHECK(strcmp(new_layers[l].info, layers[l].info) == 0);
    CHECK(new_layers[l].flag == layers[l].flag && new_layers[l].opacity == layers[l].opacity);
    for (int s = 0; s < STROKES; s++) {
      const bGPDstroke *a = &strokes[l][s], *b = &new_strokes[l][s];
      CHECK(a->mat_nr == b->mat_nr && a->thickness == b->thickness);
      CHECK(((a->flag & GP_STROKE_CYCLIC) != 0) == ((b->flag & GP_STROKE_CYCLIC) != 0));
      CHECK(a->fill_opacity_fac == b->fill_opacity_fac && same4(a->vert_color_fill, b->vert_color_fill));
    }
  }
}

int main(void)
{
  test_stroke_roundtrip();
  test_layer_roundtrip();
  test_material_roundtrip();
  test_document_roundtrip();
  if (failures) {
    printf("%d FAILED\n", failures);
    return 1;
  }
  printf("ALL PASSED\n");
  return 0;
}
