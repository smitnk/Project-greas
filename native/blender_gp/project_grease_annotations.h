/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Annotations, after Blender's annotation tool (editors/gpencil_legacy/annotate_paint.c): a second
 * bGPdata owned by the document, separate from the drawing, with one layer "Note" whose color[4]
 * and thickness (screen pixels) style every annotation stroke. Drawing uses
 * GP_GETFRAME_ADD_NEW semantics (a stroke goes into a frame at the current frame number, created
 * when missing); a frame shows the last annotation frame at or before it. The eraser removes the
 * points inside its radius and splits strokes, like annotate_paint.c's stroke eraser.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDframe;

/* New annotation data with the "Note" layer (color 0.0/0.6/1.0, thickness 3 px). */
struct bGPdata *pg_annot_create(void);
/* Frees everything pg_annot_create() and the functions below allocated. */
void pg_annot_free(struct bGPdata *annot);

void pg_annot_set_style(struct bGPdata *annot, const float rgba[4], float thickness_px);
/* rgba[4], thickness. Returns 0 without annotation data. */
int pg_annot_get_style(const struct bGPdata *annot, float out_rgba[4], float *out_thickness);

/* Freehand: begin opens a stroke in the frame at `frame` (created when missing); points are
 * appended in canvas space; end keeps the stroke when it has a point, cancel drops it. */
int pg_annot_begin(struct bGPdata *annot, int frame);
int pg_annot_add_point(struct bGPdata *annot, float x, float y);
int pg_annot_end(struct bGPdata *annot);
void pg_annot_cancel(struct bGPdata *annot);

/* Removes annotation points within `radius` of (x, y) in the frame shown at `frame`, splitting
 * strokes; strokes left with fewer than two points are removed. Returns 1 when anything changed. */
int pg_annot_erase(struct bGPdata *annot, int frame, float x, float y, float radius);
/* Removes every annotation frame. Returns 1 when there was anything. */
int pg_annot_clear(struct bGPdata *annot);

/* The annotation frame shown at `frame` (last one at or before it), or NULL. */
const struct bGPDframe *pg_annot_frame_at(const struct bGPdata *annot, int frame);
/* Total annotation strokes over all frames. */
int pg_annot_stroke_count(const struct bGPdata *annot);

/* Flat dump for saving: [frame_count, { framenum, stroke_count, { point_count, x0, y0, ... } }].
 * Returns the number of floats needed; writes only when out != NULL and capacity suffices. */
int pg_annot_dump(const struct bGPdata *annot, float *out, int capacity);
/* Replaces all frames from a dump; 0 when the data is malformed (nothing is changed then). */
int pg_annot_load(struct bGPdata *annot, const float *data, int count);

#ifdef __cplusplus
}
#endif
