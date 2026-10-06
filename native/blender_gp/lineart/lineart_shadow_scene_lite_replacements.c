/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Scene-lite replacement for the one part of Blender 3.6.23 lineart_shadow.c that reads the
 * Depsgraph / light object: lineart_main_try_generate_shadow(). tools/gen_lineart_lite.py inserts
 * the "//@@" section at the position of the original; the rest of the generated
 * project_grease_lineart_shadow.c is the pinned source verbatim.
 *
 * It follows the original line by line; only the data sources change:
 *  - lmd->light_contour_object -> scene->light (present, type, matrix_world); a light that is
 *    not LA_SUN is perspective, as is every non-light object in Blender.
 *  - lmd->shadow_camera_near / far / size -> PGLineartSettings.
 *  - lineart_main_load_geometries(depsgraph, scene, NULL, ld, ..., true, NULL) ->
 *    pg_lineart_load_geometries_for_shadow() (the Scene-lite loader in project_grease_lineart_cpu.cc).
 *  - Memory lifetime (no effect on results): the original frees the light-view LineartData here
 *    when shapes are not re-projected, but the projected-shadow edges it returns keep pointers into
 *    that data (lineart_shadow_cast_generate_edges(): e->t1 / e->t2 = sedge->e_ref, read later by
 *    lineart_edge_from_triangle() during occlusion) - a use-after-free in Blender 3.6 that only
 *    works because the freed blocks are not reused. Here it is freed after the main computation
 *    instead (pg_lineart_free_deferred_shadow(), called at the end of the compute).
 */

//@@ lineart_main_try_generate_shadow
static LineartData *pg_shadow_ld_deferred = NULL;

void pg_lineart_free_deferred_shadow(void)
{
  if (pg_shadow_ld_deferred) {
    lineart_destroy_render_data_keep_init(pg_shadow_ld_deferred);
    MEM_freeN(pg_shadow_ld_deferred);
    pg_shadow_ld_deferred = NULL;
  }
}

bool pg_lineart_main_try_generate_shadow(const PGSceneLite *scene,
                                         LineartData *original_ld,
                                         const PGLineartSettings *lmd,
                                         LineartStaticMemPool *shadow_data_pool,
                                         LineartElementLinkNode **r_veln,
                                         LineartElementLinkNode **r_eeln,
                                         ListBase *r_calculated_edges_eln_list,
                                         LineartData **r_shadow_ld_if_reproject)
{
  if ((!original_ld->conf.use_shadow && !original_ld->conf.use_light_contour &&
       !original_ld->conf.shadow_selection) ||
      (!scene->light.present))
  {
    return false;
  }

  double t_start;
  if (G.debug_value == 4000) {
    t_start = PIL_check_seconds_timer();
  }

  bool is_persp = true;

  if (scene->light.type == PG_LITE_LIGHT_SUN) {
    is_persp = false;
  }

  LineartData *ld = MEM_callocN(sizeof(LineartData), "LineArt render buffer copied");
  memcpy(ld, original_ld, sizeof(LineartData));

  BLI_spin_init(&ld->lock_task);
  BLI_spin_init(&ld->lock_cuts);
  BLI_spin_init(&ld->render_data_pool.lock_mem);

  ld->conf.do_shadow_cast = true;
  ld->shadow_data_pool = shadow_data_pool;

  /* See LineartData::edge_data_pool for explanation. */
  if (ld->conf.shadow_selection) {
    ld->edge_data_pool = shadow_data_pool;
  }
  else {
    ld->edge_data_pool = &ld->render_data_pool;
  }

  copy_v3_v3_db(ld->conf.camera_pos_secondary, ld->conf.camera_pos);
  copy_m4_m4(ld->conf.cam_obmat_secondary, ld->conf.cam_obmat);

  copy_m4_m4(ld->conf.cam_obmat, (float(*)[4])scene->light.matrix_world);
  copy_v3db_v3fl(ld->conf.camera_pos, ld->conf.cam_obmat[3]);
  ld->conf.cam_is_persp_secondary = ld->conf.cam_is_persp;
  ld->conf.cam_is_persp = is_persp;
  ld->conf.near_clip = is_persp ? lmd->shadow_camera_near : -lmd->shadow_camera_far;
  ld->conf.far_clip = lmd->shadow_camera_far;
  ld->w = lmd->shadow_camera_size;
  ld->h = lmd->shadow_camera_size;
  /* Need to prevent wrong camera configuration so that shadow computation won't stall. */
  if (!ld->w || !ld->h) {
    ld->w = ld->h = 200;
  }
  if (!ld->conf.near_clip || !ld->conf.far_clip) {
    ld->conf.near_clip = 0.1f;
    ld->conf.far_clip = 200.0f;
  }
  ld->qtree.recursive_level = is_persp ? LRT_TILE_RECURSIVE_PERSPECTIVE : LRT_TILE_RECURSIVE_ORTHO;

  /* Contour and loose edge from light viewing direction will be cast as shadow, so only
   * force them on. If we need lit/shaded information for other line types, they are then
   * enabled as-is so that cutting positions can also be calculated through shadow projection.
   */
  if (!ld->conf.shadow_selection) {
    ld->conf.use_crease = ld->conf.use_material = ld->conf.use_edge_marks =
        ld->conf.use_intersections = ld->conf.use_light_contour = false;
  }
  else {
    ld->conf.use_contour_secondary = true;
    ld->conf.allow_duplicated_types = true;
  }
  ld->conf.use_loose = true;
  ld->conf.use_contour = true;

  ld->conf.max_occlusion_level = 0; /* No point getting see-through projections there. */
  ld->conf.use_back_face_culling = false;

  /* Override matrices to light "camera". */
  double proj[4][4], view[4][4], result[4][4];
  float inv[4][4];
  if (is_persp) {
    lineart_matrix_perspective_44d(proj, DEG2RAD(160), 1, ld->conf.near_clip, ld->conf.far_clip);
  }
  else {
    lineart_matrix_ortho_44d(
        proj, -ld->w, ld->w, -ld->h, ld->h, ld->conf.near_clip, ld->conf.far_clip);
  }
  invert_m4_m4(inv, ld->conf.cam_obmat);
  mul_m4db_m4db_m4fl(result, proj, inv);
  copy_m4_m4_db(proj, result);
  copy_m4_m4_db(ld->conf.view_projection, proj);
  unit_m4_db(view);
  copy_m4_m4_db(ld->conf.view, view);

  lineart_main_get_view_vector(ld);

  pg_lineart_load_geometries_for_shadow(scene, ld);

  if (!ld->geom.vertex_buffer_pointers.first) {
    /* No geometry loaded, return early. */
    lineart_destroy_render_data_keep_init(ld);
    MEM_freeN(ld);
    return false;
  }

  /* The exact same process as in MOD_lineart_compute_feature_lines() until occlusion finishes.
   */

  lineart_main_bounding_area_make_initial(ld);
  lineart_main_cull_triangles(ld, false);
  lineart_main_cull_triangles(ld, true);
  lineart_main_free_adjacent_data(ld);
  lineart_main_perspective_division(ld);
  lineart_main_discard_out_of_frame_edges(ld);
  lineart_main_add_triangles(ld);
  lineart_main_bounding_areas_connect_post(ld);
  lineart_main_link_lines(ld);
  lineart_main_occlusion_begin(ld);

  /* Do shadow cast stuff then get generated vert/edge data. */
  lineart_shadow_cast(ld, true, false);
  bool any_generated = lineart_shadow_cast_generate_edges(ld, true, r_veln, r_eeln);

  if (ld->conf.shadow_selection) {
    memcpy(r_calculated_edges_eln_list, &ld->geom.line_buffer_pointers, sizeof(ListBase));
  }

  if (ld->conf.shadow_enclose_shapes) {
    /* Need loaded data for re-projecting the 3rd time to get shape boundary against lit/shaded
     * region. */
    (*r_shadow_ld_if_reproject) = ld;
  }
  else {
    /* Freed by pg_lineart_free_deferred_shadow() (see the top of this file). */
    pg_lineart_free_deferred_shadow();
    pg_shadow_ld_deferred = ld;
  }

  if (G.debug_value == 4000) {
    double t_elapsed = PIL_check_seconds_timer() - t_start;
    printf("Line art shadow stage 1 time: %f\n", t_elapsed);
  }

  return any_generated;
}
