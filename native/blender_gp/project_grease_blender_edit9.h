/* SPDX-License-Identifier: GPL-2.0-or-later
 * Edit/animation core (parity phase 1, items 1-5): proportional editing transforms with pivot modes (transform.c rules),
 * dope-sheet keyframe operations, multi-segment Dash. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDlayer;

/* eProportionalFalloff (DNA_scene_types.h) */
enum { PG_PROP_SMOOTH = 0, PG_PROP_SPHERE = 1, PG_PROP_ROOT = 2, PG_PROP_SHARP = 3, PG_PROP_LIN = 4, PG_PROP_CONST = 5,
       PG_PROP_RANDOM = 6 /* not supported (time-seeded in Blender) */, PG_PROP_INVSQUARE = 7 };
/* pivot (V3D_AROUND_*) */
enum { PG_PIVOT_MEDIAN = 0, PG_PIVOT_BOUNDS = 1, PG_PIVOT_INDIVIDUAL = 2, PG_PIVOT_CURSOR = 3 };
enum { PG_XFORM_TRANSLATE = 0, PG_XFORM_ROTATE = 1, PG_XFORM_SCALE = 2 };

typedef struct PGTransform {
  int type;            /* PG_XFORM_* */
  float a, b;          /* translate dx,dy | rotate radians,- | scale sx,sy */
  int pivot;           /* PG_PIVOT_* */
  float cursor[2];     /* for PG_PIVOT_CURSOR */
  int proportional;    /* 0 off, 1 on */
  int connected;       /* proportional distance along the stroke instead of straight line */
  int falloff;         /* PG_PROP_* */
  float size;          /* proportional radius */
} PGTransform;

/* falloff weight for distance d within radius (transform_mode... calculatePropRatio) */
float pg_prop_falloff(int falloff, float d, float size);
int pg_gp_transform(struct bGPdata *gpd, const struct bGPDlayer *only, const PGTransform *t);

/* Dope sheet (action_edit.c behaviour on GP frames, active layer or all editable layers). */
int pg_gp_frames_select_range(struct bGPdata *gpd, struct bGPDlayer *only, int fmin, int fmax, int extend);
int pg_gp_frames_move(struct bGPdata *gpd, struct bGPDlayer *only, int offset);       /* selected frames */
int pg_gp_frames_scale(struct bGPdata *gpd, struct bGPDlayer *only, int center, float factor);
int pg_gp_frames_copy(struct bGPdata *gpd, struct bGPDlayer *only);                   /* selected frames */
int pg_gp_frames_paste(struct bGPdata *gpd, struct bGPDlayer *target, int at_frame);  /* offset to at_frame */
void pg_gp_frames_clipboard_free(void);

/* Dash with several segments (dash, gap) in sequence, like MOD_gpencil_legacy_dash segments. */
int pg_gp_dash_segments(struct bGPdata *gpd, const struct bGPDlayer *only, const int *dash, const int *gap,
                        int nseg, int offset);
/* Commands, routed by the bridge (args are floats like every edit command). */
enum {
  PG_EDIT9_CMD_TRANSFORM = 117,          /* type, a, b, pivot, cursor x, y, proportional, connected, falloff,
                                            size[, snap increment (0 off)][, all layers] */
  PG_EDIT9_CMD_FRAMES_SELECT_RANGE = 118,/* fmin, fmax[, extend][, all layers] */
  PG_EDIT9_CMD_FRAMES_MOVE = 119,        /* offset[, all layers] */
  PG_EDIT9_CMD_FRAMES_SCALE = 120,       /* center frame, factor[, all layers] */
  PG_EDIT9_CMD_FRAMES_COPY = 121,        /* [all layers] (no document change) */
  PG_EDIT9_CMD_FRAMES_PASTE = 122,       /* at frame (into the active layer, overwrite) */
  PG_EDIT9_CMD_DASH_SEGMENTS = 123,      /* offset, n, dash0, gap0, ... (selected strokes, active layer) */
};
#define PG_EDIT9_CMD_FIRST 117
#define PG_EDIT9_CMD_LAST 123
int pg_gp_edit9_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                         const float *args, int arg_count);
#ifdef __cplusplus
}
#endif
