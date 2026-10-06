/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Fill tool ported from Blender 3.6.23 editors/gpencil_legacy/gpencil_fill.c.
 *
 * Raster part (boundary fill, leak check, borders, dilate / contract, Moore outline, points from the
 * stack) runs Blender's own code, copied verbatim, on a CPU float RGBA buffer standing in for the
 * GP_fill ImBuf. Extend Lines (GP_FILL_EMODE_EXTEND with the stroke collision check) runs Blender's
 * own extension code on the document's strokes in the 2D canvas plane. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;

#define PG_FILL_LEAK 3.0f        /* FILL_LEAK */
#define PG_FILL_MIN_WINDOW 128   /* MIN_WINDOW_SIZE */
#define PG_FILL_MIN_FAC 0.05f    /* GPENCIL_MIN_FILL_FAC */
#define PG_FILL_MAX_FAC 8.0f     /* GPENCIL_MAX_FILL_FAC */
/* Canvas units per Blender unit: Blender's extension length (fill_extend_fac * 0.1) and its bbox
 * margin (1.1) are in world units; the Project Grease canvas has no world unit, this maps them. */
#define PG_FILL_CANVAS_UNITS_PER_BU 100.0f

/* gpencil_session_init_fill(): fill_factor clamped to [0.05, 8]. */
float pg_fill_factor_clamp(float fill_factor);
/* gpencil_session_init_fill(): fill_leak = (int)ceil(FILL_LEAK * fill_factor). */
int pg_fill_leak_from_factor(float fill_factor);
/* gpencil_render_offscreen(): max_ii((int)win * fill_factor, MIN_WINDOW_SIZE). */
int pg_fill_render_size(int win, float fill_factor);

/* Resamples a mask (RGBA float, row 0 = bottom) to dw x dh: a destination pixel takes the pixel
 * with the highest alpha in its source footprint (so 1 px lines survive a reduced resolution). */
void pg_fill_resample_mask(const float *src, int sw, int sh, float *dst, int dw, int dh);

/* gpencil_do_frame_fill() (not inverted) on rgba (w x h, row 0 = bottom, red = boundary as the
 * GP_fill render). draw_mouse_position() is reproduced by painting a blue square of point_size
 * px (4 * fill_factor * sqrt(2)) around the seed on non-boundary pixels. fill_leak as tgpf->fill_leak,
 * dilate_pixels as brush dilate_pixels. On success returns the number of outline points and sets
 * *r_xy (malloc'ed x,y pairs in image px, Blender's stack order); 0 when nothing is filled
 * (*r_border_contact tells when the flood reached the image border). */
int pg_fill_raster(float *rgba, int w, int h, int seed_x, int seed_y, float point_size, int fill_leak,
                   int dilate_pixels, float **r_xy, int *r_border_contact);

/* Extend Lines (gpencil_load_array_strokes + gpencil_update_extensions_line +
 * gpencil_cut_extensions [+ gpencil_stroke_collision]) for frame cfra of every visible layer.
 * Canvas -> pixel: px = x * px_scale + px_ox, py = y * px_scale + px_oy. Writes up to max_segments
 * drawable extension segments (x0,y0,x1,y1 canvas units; gpencil_stroke_is_drawable with
 * is_render) and returns their count. (canvas_cx, canvas_cy) is the canvas centre, used as the
 * world origin. The document is left unchanged. */
int pg_fill_extend_lines(struct bGPdata *gpd, int cfra, float extend_fac, int use_stroke_collide,
                         float px_scale, float px_ox, float px_oy, float canvas_cx, float canvas_cy,
                         float *out, int max_segments);

#ifdef __cplusplus
}
#endif
