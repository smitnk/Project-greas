#include <GLES3/gl3.h>

#include "BLI_listbase.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "draw_cache.h"
#include "draw_cache_impl.h"
#include "GPU_index_buffer.h"
#include "GPU_vertex_buffer.h"
#include "GPU_batch.h"

extern "C" int project_grease_android_gpu_configure_batch(GPUBatch *batch);

extern "C" int project_grease_android_gp_cache_upload_probe()
{
  if (glGetString(GL_VERSION) == nullptr) {
    return 0;
  }

  BKE_idtype_init();

  Main *bmain = BKE_main_new();
  if (bmain == nullptr) {
    return 0;
  }

  bGPdata *gpd = BKE_gpencil_data_addnew(bmain, "Android GP buffer probe");
  if (gpd == nullptr) {
    BKE_main_free(bmain);
    return 0;
  }

  BKE_gpencil_batch_cache_dirty_tag_cb = DRW_gpencil_batch_cache_dirty_tag;
  BKE_gpencil_batch_cache_free_cb = DRW_gpencil_batch_cache_free;

  bGPDlayer *layer = BKE_gpencil_layer_addnew(gpd, "GP_Layer", true, false);
  bGPDframe *frame = BKE_gpencil_frame_addnew(layer, 1);
  bGPDstroke *stroke = BKE_gpencil_stroke_add(frame, 0, 3, 4, false);

  if (!layer || !frame || !stroke) {
    BKE_main_free(bmain);
    return 0;
  }

  stroke->points[0].x = -0.5f;
  stroke->points[0].y = -0.4f;
  stroke->points[0].pressure = 1.0f;
  stroke->points[0].strength = 1.0f;
  stroke->points[1].x = 0.0f;
  stroke->points[1].y = 0.5f;
  stroke->points[1].pressure = 1.0f;
  stroke->points[1].strength = 1.0f;
  stroke->points[2].x = 0.5f;
  stroke->points[2].y = -0.4f;
  stroke->points[2].pressure = 1.0f;
  stroke->points[2].strength = 1.0f;

  Object *ob = BKE_object_add_only_object(bmain, OB_GPENCIL_LEGACY, "Android GP Probe");
  if (ob == nullptr) {
    BKE_main_free(bmain);
    return 0;
  }
  ob->data = gpd;

  GPUBatch *batch = DRW_cache_gpencil_get(ob, frame->framenum);
  GPUVertBuf *position = DRW_cache_gpencil_position_buffer_get(ob, frame->framenum);
  GPUVertBuf *color = DRW_cache_gpencil_color_buffer_get(ob, frame->framenum);

  if (batch == nullptr || position == nullptr || color == nullptr || batch->elem == nullptr) {
    ob->data = nullptr;
    BKE_id_free(bmain, &ob->id);
    BKE_main_free(bmain);
    return 0;
  }

  GPU_vertbuf_use(position);
  GPU_vertbuf_use(color);
  GPU_indexbuf_use(batch->elem);

  const bool upload_ok = glGetError() == GL_NO_ERROR;
  const bool vao_ok = project_grease_android_gpu_configure_batch(batch) != 0;

  DRW_gpencil_batch_cache_free(gpd);
  ob->data = nullptr;
  BKE_id_free(bmain, &ob->id);
  BKE_main_free(bmain);

  return (upload_ok && vao_ok) ? 1 : 0;
}
