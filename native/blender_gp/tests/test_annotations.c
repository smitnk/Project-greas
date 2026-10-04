/* Host test for the annotation data (native/blender_gp/project_grease_annotations.c). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "DNA_gpencil_legacy_types.h"
#include "project_grease_annotations.h"

int pg_test_mem_free_count = 0; /* see select_shim/MEM_guardedalloc.h */

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)

static int frame_count(const bGPdata *a)
{
  int n = 0;
  for (const bGPDframe *f = ((const bGPDlayer *)a->layers.first)->frames.first; f; f = f->next) n++;
  return n;
}

static void line(bGPdata *a, int frame, float y, int n)
{
  pg_annot_begin(a, frame);
  for (int i = 0; i < n; i++) pg_annot_add_point(a, (float)i * 10.0f, y);
  pg_annot_end(a);
}

int main(void)
{
  bGPdata *a = pg_annot_create();
  const bGPDlayer *note = a->layers.first;
  float rgba[4], th = 0;
  CHECK(note && note->next == NULL && note->info[0] == 'N', "one layer \"Note\"");
  CHECK(pg_annot_get_style(a, rgba, &th) && th == 3.0f && rgba[2] == 1.0f, "default style");
  const float red[4] = {1, 0, 0, 2};
  pg_annot_set_style(a, red, 50.0f);
  pg_annot_get_style(a, rgba, &th);
  CHECK(rgba[0] == 1.0f && rgba[3] == 1.0f && th == 20.0f, "style clamped");

  /* drawing, frames and holds */
  line(a, 1, 0, 5);
  line(a, 1, 50, 3);
  line(a, 5, 100, 4);
  CHECK(pg_annot_stroke_count(a) == 3 && frame_count(a) == 2, "two frames, three strokes");
  CHECK(pg_annot_frame_at(a, 3)->framenum == 1 && pg_annot_frame_at(a, 9)->framenum == 5, "frames hold");
  CHECK(pg_annot_frame_at(a, 0) == NULL, "nothing before the first frame");
  pg_annot_begin(a, 7);
  pg_annot_add_point(a, 1, 1);
  CHECK(pg_annot_stroke_count(a) == 3, "open stroke not counted");
  pg_annot_cancel(a);
  CHECK(frame_count(a) == 2, "cancel removes the frame it created");
  pg_annot_begin(a, 1);
  CHECK(pg_annot_end(a) == 0 && pg_annot_stroke_count(a) == 3 && frame_count(a) == 2, "empty stroke dropped");
  pg_annot_begin(a, 2);
  for (int i = 0; i < 200; i++) pg_annot_add_point(a, (float)i, 7.0f);
  pg_annot_end(a);
  CHECK(pg_annot_stroke_count(a) == 4, "long stroke grows its buffer");

  /* eraser splits strokes */
  CHECK(pg_annot_erase(a, 1, 20.0f, 0.0f, 1.0f) == 1, "erase in the middle of the 5-point line");
  int parts = 0, total = 0;
  for (const bGPDstroke *s = pg_annot_frame_at(a, 1)->strokes.first; s; s = s->next) { parts++; total += s->totpoints; }
  CHECK(parts == 3 && total == 7, "split into 0-10 and 30-40, 3-point line untouched");
  CHECK(pg_annot_erase(a, 1, 0.0f, 50.0f, 1.0f) == 1, "erase the end of the 3-point line");
  CHECK(pg_annot_erase(a, 1, 500.0f, 500.0f, 5.0f) == 0, "nothing in range");
  CHECK(pg_annot_erase(a, 3, 100.0f, 100.0f, 5.0f) == 0, "frame 3 shows frame 2's annotation (y=7), not frame 5's");

  /* dump / load round trip */
  const int need = pg_annot_dump(a, NULL, 0);
  float *buf = malloc(sizeof(float) * (size_t)need);
  CHECK(pg_annot_dump(a, buf, need) == need && buf[0] == 3.0f, "dump");
  bGPdata *b = pg_annot_create();
  CHECK(pg_annot_load(b, buf, need) == 1 && pg_annot_stroke_count(b) == pg_annot_stroke_count(a), "load");
  CHECK(pg_annot_dump(b, NULL, 0) == need, "same size after load");
  CHECK(pg_annot_load(b, buf, need - 1) == 0 && pg_annot_stroke_count(b) == pg_annot_stroke_count(a), "truncated dump rejected, data kept");
  const float bad[] = {1, 0, 1, 9999, 0, 0};
  CHECK(pg_annot_load(b, bad, 6) == 0, "point count beyond the data rejected");
  free(buf);

  CHECK(pg_annot_clear(a) == 1 && pg_annot_stroke_count(a) == 0 && frame_count(a) == 0, "clear");
  CHECK(pg_annot_clear(a) == 0, "clear twice");
  pg_annot_free(a);
  pg_annot_free(b);
  pg_annot_free(NULL);
  /* Regression (emulator sweep: annotations drew nothing): a drag's points all reach the stroke.
   * The open marker lived in bit 30 of bGPDstroke.flag, a short in Blender's DNA, so it was lost
   * and every point after begin was dropped. */
  {
    bGPdata *b = pg_annot_create();
    line(b, 1, 50.0f, 25);
    const bGPDframe *f = pg_annot_frame_at(b, 1);
    const bGPDstroke *s = f ? (const bGPDstroke *)f->strokes.last : NULL;
    CHECK(s && s->totpoints == 25, "every point of the drag is in the stroke");
    CHECK(pg_annot_stroke_count(b) == 1, "one finished stroke");
    short as_short = (short)(1 << 5);
    CHECK(as_short != 0, "the open marker fits a short flag");
    pg_annot_free(b);
  }
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
