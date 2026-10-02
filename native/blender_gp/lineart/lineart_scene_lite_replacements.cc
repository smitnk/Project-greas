/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Scene-lite replacements for the parts of Blender 3.6.23 lineart_cpu.cc that read Mesh / Object /
 * Depsgraph / Collection / Material data. tools/gen_lineart_lite.py inserts each "//@@ name"
 * section at the position of the original item of that name; everything else in the generated
 * file is the pinned source verbatim.
 *
 * Each replacement follows the original line by line and only changes where data comes from:
 *  - Mesh            -> PGMeshLite (verts, fan-triangulated tris with polygon / material / smooth
 *                       per triangle). corner_verts is the identity (a triangle stores vertex
 *                       indices), looptri_polys is tri_poly, material_indices is tri_material,
 *                       sharp_face is !tri_smooth, sharp_edge / freestyle marks come from the
 *                       PGLiteEdge flags, BKE_mesh_looptri_get_real_edges is "the edge is not a
 *                       polygon diagonal".
 *  - Object          -> PGObjectLite (matrix_world, line_art_usage); LineartObjectInfo::original_ob
 *                       and ::original_me point at the PGObjectLite (identity only; never
 *                       dereferenced as Object / Mesh). No per-object crease override, no
 *                       collections (usage comes from the object), no instancing.
 *  - Material        -> Blender's default material values (no Line Art material settings yet):
 *                       mat_occlusion 1, no mask bits, no custom intersection priority, no back
 *                       face culling flag.
 *  - Scene / Depsgraph / Render -> the PGSceneLite camera and render size, one thread.
 */

//@@ prelude
/* Scene-lite access for the replacements below. */
static const PGObjectLite *pg_obi_object(const LineartObjectInfo *obi)
{
  return reinterpret_cast<const PGObjectLite *>(obi->original_me);
}
/* BKE_mesh_looptri_get_real_edges(): the mesh edge of every triangle side, -1 for polygon
 * diagonals. Scene-lite edges are looked up by their sorted vertex pair. */
static int pg_lite_find_edge(const PGMeshLite *me, int a, int b)
{
  if (a > b) {
    std::swap(a, b);
  }
  int lo = 0, hi = me->totedge - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const PGLiteEdge &e = me->edges[mid];
    if (e.v[0] == a && e.v[1] == b) {
      return mid;
    }
    if (e.v[0] < a || (e.v[0] == a && e.v[1] < b)) {
      lo = mid + 1;
    }
    else {
      hi = mid - 1;
    }
  }
  return -1;
}
static void pg_lite_looptri_get_real_edges(const PGMeshLite *me, int tri_index, int r_edges[3])
{
  for (int k = 0; k < 3; k++) {
    const int e = pg_lite_find_edge(me, me->tris[tri_index][k], me->tris[tri_index][(k + 1) % 3]);
    r_edges[k] = (e >= 0 && !(me->edges[e].flag & PG_LITE_EDGE_POLY_INTERNAL)) ? e : -1;
  }
}

//@@ EdgeFeatData
struct EdgeFeatData {
  LineartData *ld;
  const PGMeshLite *me;
  const int *material_indices; /* per triangle */
  LineartTriangle *tri_array;
  LineartVert *v_array;
  float crease_threshold;
  bool use_auto_smooth;
  bool use_freestyle_edge;
  LineartEdgeNeighbor *edge_nabr;
};

//@@ lineart_identify_mlooptri_feature_edges
static void lineart_identify_mlooptri_feature_edges(void *__restrict userdata,
                                                    const int i,
                                                    const TaskParallelTLS *__restrict tls)
{
  EdgeFeatData *e_feat_data = (EdgeFeatData *)userdata;
  EdgeFeatReduceData *reduce_data = (EdgeFeatReduceData *)tls->userdata_chunk;
  const PGMeshLite *me = e_feat_data->me;
  const int *material_indices = e_feat_data->material_indices;
  LineartEdgeNeighbor *edge_nabr = e_feat_data->edge_nabr;

  uint16_t edge_flag_result = 0;

  /* Because the edge neighbor array contains loop edge pairs, we only need to process the first
   * edge in the pair. Otherwise we would add the same edge that the loops represent twice. */
  if (i < edge_nabr[i].e) {
    return;
  }

  /* Freestyle face marks: Scene-lite has none, so the face-mark filter never applies. */
  const bool only_contour = false;

  /* Mesh boundary */
  if (edge_nabr[i].e == -1) {
    edge_nabr[i].flags = LRT_EDGE_FLAG_CONTOUR;
    reduce_data->feat_edges += 1;
    return;
  }

  LineartTriangle *tri1, *tri2;
  LineartVert *vert;
  LineartData *ld = e_feat_data->ld;

  int f1 = i / 3, f2 = edge_nabr[i].e / 3;

  /* The mesh should already be triangulated now, so we can assume each face is a triangle. */
  tri1 = lineart_triangle_from_index(ld, e_feat_data->tri_array, f1);
  tri2 = lineart_triangle_from_index(ld, e_feat_data->tri_array, f2);

  vert = &e_feat_data->v_array[edge_nabr[i].v1];

  double view_vector_persp[3];
  double *view_vector = view_vector_persp;
  double dot_v1 = 0, dot_v2 = 0;
  double result;
  bool material_back_face = ((tri1->flags | tri2->flags) & LRT_TRIANGLE_MAT_BACK_FACE_CULLING);

  if (ld->conf.use_contour || ld->conf.use_back_face_culling || material_back_face) {
    if (ld->conf.cam_is_persp) {
      sub_v3_v3v3_db(view_vector, ld->conf.camera_pos, vert->gloc);
    }
    else {
      view_vector = ld->conf.view_vector;
    }

    dot_v1 = dot_v3v3_db(view_vector, tri1->gn);
    dot_v2 = dot_v3v3_db(view_vector, tri2->gn);

    if ((result = dot_v1 * dot_v2) <= 0 && (dot_v1 + dot_v2)) {
      edge_flag_result |= LRT_EDGE_FLAG_CONTOUR;
    }

    if (ld->conf.use_back_face_culling) {
      if (dot_v1 < 0) {
        tri1->flags |= LRT_CULL_DISCARD;
      }
      if (dot_v2 < 0) {
        tri2->flags |= LRT_CULL_DISCARD;
      }
    }
    if (material_back_face) {
      if (tri1->flags & LRT_TRIANGLE_MAT_BACK_FACE_CULLING && dot_v1 < 0) {
        tri1->flags |= LRT_CULL_DISCARD;
      }
      if (tri2->flags & LRT_TRIANGLE_MAT_BACK_FACE_CULLING && dot_v2 < 0) {
        tri2->flags |= LRT_CULL_DISCARD;
      }
    }
  }

  /* use_contour_secondary needs a light reference object: not supported. */

  if (!only_contour) {
    if (ld->conf.use_crease) {
      bool do_crease = true;
      /* sharp_face == flat shaded polygon */
      if (!ld->conf.force_crease && !e_feat_data->use_auto_smooth && me->tri_smooth[f1] &&
          me->tri_smooth[f2])
      {
        do_crease = false;
      }
      if (do_crease && (dot_v3v3_db(tri1->gn, tri2->gn) < e_feat_data->crease_threshold)) {
        edge_flag_result |= LRT_EDGE_FLAG_CREASE;
      }
    }

    int mat1 = material_indices ? material_indices[f1] : 0;
    int mat2 = material_indices ? material_indices[f2] : 0;

    if (mat1 != mat2) {
      /* Both materials have Blender's default mat_occlusion (1), so the "occlusion 0 vs non-0"
       * contour rule never applies. */
      if (ld->conf.use_material) {
        edge_flag_result |= LRT_EDGE_FLAG_MATERIAL;
      }
    }
  }

  int real_edges[3];
  pg_lite_looptri_get_real_edges(me, i / 3, real_edges);

  if (real_edges[i % 3] >= 0) {
    const int eflag = me->edges[real_edges[i % 3]].flag;
    if (ld->conf.use_crease && ld->conf.sharp_as_crease && (eflag & PG_LITE_EDGE_SHARP)) {
      edge_flag_result |= LRT_EDGE_FLAG_CREASE;
    }

    if (ld->conf.use_edge_marks && e_feat_data->use_freestyle_edge) {
      if (eflag & PG_LITE_EDGE_FREESTYLE) {
        edge_flag_result |= LRT_EDGE_FLAG_EDGE_MARK;
      }
    }
  }

  edge_nabr[i].flags = edge_flag_result;

  if (edge_flag_result) {
    /* Only allocate for feature edge (instead of all edges) to save memory.
     * If allow duplicated edges, one edge gets added multiple times if it has multiple types.
     */
    reduce_data->feat_edges += e_feat_data->ld->conf.allow_duplicated_types ?
                                   lineart_edge_type_duplication_count(edge_flag_result) :
                                   1;
  }
}

//@@ TriData
struct TriData {
  LineartObjectInfo *ob_info;
  const PGMeshLite *me;
  LineartVert *vert_arr;
  LineartTriangle *tri_arr;
  int lineart_triangle_size;
  LineartTriangleAdjacent *tri_adj;
};

//@@ lineart_load_tri_task
static void lineart_load_tri_task(void *__restrict userdata,
                                  const int i,
                                  const TaskParallelTLS *__restrict /*tls*/)
{
  TriData *tri_task_data = (TriData *)userdata;
  LineartObjectInfo *ob_info = tri_task_data->ob_info;
  const PGMeshLite *me = tri_task_data->me;
  LineartVert *vert_arr = tri_task_data->vert_arr;
  LineartTriangle *tri = tri_task_data->tri_arr;

  tri = (LineartTriangle *)(((uchar *)tri) + tri_task_data->lineart_triangle_size * i);

  int v1 = me->tris[i][0];
  int v2 = me->tris[i][1];
  int v3 = me->tris[i][2];

  tri->v[0] = &vert_arr[v1];
  tri->v[1] = &vert_arr[v2];
  tri->v[2] = &vert_arr[v3];

  /* Material mask bits and occlusion effectiveness assignment: Blender's default material. */
  tri->material_mask_bits |= 0;
  tri->mat_occlusion |= 1;
  tri->intersection_priority = ob_info->intersection_priority;

  tri->intersection_mask = ob_info->override_intersection_mask;

  tri->target_reference = (ob_info->obindex | (i & LRT_OBINDEX_LOWER));

  double gn[3];
  float no[3];
  normal_tri_v3(no, me->verts[v1], me->verts[v2], me->verts[v3]);
  copy_v3db_v3fl(gn, no);
  mul_v3_mat3_m4v3_db(tri->gn, ob_info->normal, gn);
  normalize_v3_db(tri->gn);

  if (ob_info->usage == OBJECT_LRT_INTERSECTION_ONLY) {
    tri->flags |= LRT_TRIANGLE_INTERSECTION_ONLY;
  }
  else if (ob_info->usage == OBJECT_LRT_FORCE_INTERSECTION) {
    tri->flags |= LRT_TRIANGLE_FORCE_INTERSECTION;
  }
  else if (ELEM(ob_info->usage, OBJECT_LRT_NO_INTERSECTION, OBJECT_LRT_OCCLUSION_ONLY)) {
    tri->flags |= LRT_TRIANGLE_NO_INTERSECTION;
  }

  /* Re-use this field to refer to adjacent info, will be cleared after culling stage. */
  tri->intersecting_verts = static_cast<LinkNode *>((void *)&tri_task_data->tri_adj[i]);
}

//@@ EdgeNeighborData
struct EdgeNeighborData {
  LineartEdgeNeighbor *edge_nabr;
  LineartAdjacentEdge *adj_e;
  const PGMeshLite *me;
};

//@@ lineart_edge_neighbor_init_task
static void lineart_edge_neighbor_init_task(void *__restrict userdata,
                                            const int i,
                                            const TaskParallelTLS *__restrict /*tls*/)
{
  EdgeNeighborData *en_data = (EdgeNeighborData *)userdata;
  LineartAdjacentEdge *adj_e = &en_data->adj_e[i];
  const int *looptri = en_data->me->tris[i / 3];
  LineartEdgeNeighbor *edge_nabr = &en_data->edge_nabr[i];

  adj_e->e = i;
  adj_e->v1 = looptri[i % 3];
  adj_e->v2 = looptri[(i + 1) % 3];
  if (adj_e->v1 > adj_e->v2) {
    std::swap(adj_e->v1, adj_e->v2);
  }
  edge_nabr->e = -1;

  edge_nabr->v1 = adj_e->v1;
  edge_nabr->v2 = adj_e->v2;
  edge_nabr->flags = 0;
}

//@@ lineart_build_edge_neighbor
static LineartEdgeNeighbor *lineart_build_edge_neighbor(const PGMeshLite *me, int total_edges)
{
  LineartAdjacentEdge *adj_e = static_cast<LineartAdjacentEdge *>(
      MEM_mallocN(sizeof(LineartAdjacentEdge) * total_edges, "LineartAdjacentEdge arr"));
  LineartEdgeNeighbor *edge_nabr = static_cast<LineartEdgeNeighbor *>(
      MEM_mallocN(sizeof(LineartEdgeNeighbor) * total_edges, "LineartEdgeNeighbor arr"));

  TaskParallelSettings en_settings;
  BLI_parallel_range_settings_defaults(&en_settings);
  /* Set the minimum amount of edges a thread has to process. */
  en_settings.min_iter_per_thread = 50000;

  EdgeNeighborData en_data;
  en_data.adj_e = adj_e;
  en_data.edge_nabr = edge_nabr;
  en_data.me = me;

  BLI_task_parallel_range(0, total_edges, &en_data, lineart_edge_neighbor_init_task, &en_settings);

  lineart_sort_adjacent_items(adj_e, total_edges);

  for (int i = 0; i < total_edges - 1; i++) {
    if (adj_e[i].v1 == adj_e[i + 1].v1 && adj_e[i].v2 == adj_e[i + 1].v2) {
      edge_nabr[adj_e[i].e].e = adj_e[i + 1].e;
      edge_nabr[adj_e[i + 1].e].e = adj_e[i].e;
    }
  }

  MEM_freeN(adj_e);

  return edge_nabr;
}

//@@ lineart_geometry_object_load
static void lineart_geometry_object_load(LineartObjectInfo *ob_info,
                                         LineartData *la_data,
                                         ListBase * /*shadow_elns*/)
{
  const PGObjectLite *pob = pg_obi_object(ob_info);
  const PGMeshLite *me = &pob->mesh;
  if (!me->totedge) {
    return;
  }

  /* Triangulate: Scene-lite meshes are triangulated on import. */
  const int looptris_size = me->tottri;

  const int *material_indices = me->tri_material;

  /* Freestyle edge marks come from the edge flags; there are no freestyle face marks. */
  bool can_find_freestyle_edge = false;
  for (int e = 0; e < me->totedge; e++) {
    if (me->edges[e].flag & PG_LITE_EDGE_FREESTYLE) {
      can_find_freestyle_edge = true;
      break;
    }
  }

  LineartVert *la_v_arr = static_cast<LineartVert *>(
      lineart_mem_acquire_thread(&la_data->render_data_pool, sizeof(LineartVert) * me->totvert));
  LineartTriangle *la_tri_arr = static_cast<LineartTriangle *>(lineart_mem_acquire_thread(
      &la_data->render_data_pool, looptris_size * la_data->sizeof_triangle));

  Object *orig_ob = ob_info->original_ob;

  BLI_spin_lock(&la_data->lock_task);
  LineartElementLinkNode *elem_link_node = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized_thread(&la_data->geom.vertex_buffer_pointers,
                                                    &la_data->render_data_pool,
                                                    la_v_arr,
                                                    sizeof(LineartElementLinkNode)));
  BLI_spin_unlock(&la_data->lock_task);

  elem_link_node->obindex = ob_info->obindex;
  elem_link_node->element_count = me->totvert;
  elem_link_node->object_ref = orig_ob;
  ob_info->v_eln = elem_link_node;

  /* No per-object crease override and no auto smooth in Scene-lite. */
  bool use_auto_smooth = false;
  float crease_angle = la_data->conf.crease_threshold;

  BLI_spin_lock(&la_data->lock_task);
  elem_link_node = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized_thread(&la_data->geom.triangle_buffer_pointers,
                                                    &la_data->render_data_pool,
                                                    la_tri_arr,
                                                    sizeof(LineartElementLinkNode)));
  BLI_spin_unlock(&la_data->lock_task);

  int usage = ob_info->usage;

  elem_link_node->element_count = looptris_size;
  elem_link_node->object_ref = orig_ob;
  elem_link_node->flags = eLineArtElementNodeFlag(
      elem_link_node->flags |
      ((usage == OBJECT_LRT_NO_INTERSECTION) ? LRT_ELEMENT_NO_INTERSECTION : 0));

  /* Note this memory is not from pool, will be deleted after culling. */
  LineartTriangleAdjacent *tri_adj = static_cast<LineartTriangleAdjacent *>(
      MEM_callocN(sizeof(LineartTriangleAdjacent) * looptris_size, "LineartTriangleAdjacent"));
  /* Link is minimal so we use pool anyway. */
  BLI_spin_lock(&la_data->lock_task);
  lineart_list_append_pointer_pool_thread(
      &la_data->geom.triangle_adjacent_pointers, &la_data->render_data_pool, tri_adj);
  BLI_spin_unlock(&la_data->lock_task);

  /* Convert all vertices to lineart verts. */
  TaskParallelSettings vert_settings;
  BLI_parallel_range_settings_defaults(&vert_settings);
  /* Set the minimum amount of verts a thread has to process. */
  vert_settings.min_iter_per_thread = 4000;

  VertData vert_data;
  vert_data.positions = me->verts;
  vert_data.v_arr = la_v_arr;
  vert_data.model_view = ob_info->model_view;
  vert_data.model_view_proj = ob_info->model_view_proj;

  BLI_task_parallel_range(
      0, me->totvert, &vert_data, lineart_mvert_transform_task, &vert_settings);

  /* Convert all mesh triangles into lineart triangles.
   * Also create an edge map to get connectivity between edges and triangles. */
  TaskParallelSettings tri_settings;
  BLI_parallel_range_settings_defaults(&tri_settings);
  /* Set the minimum amount of triangles a thread has to process. */
  tri_settings.min_iter_per_thread = 4000;

  TriData tri_data;
  tri_data.ob_info = ob_info;
  tri_data.me = me;
  tri_data.vert_arr = la_v_arr;
  tri_data.tri_arr = la_tri_arr;
  tri_data.lineart_triangle_size = la_data->sizeof_triangle;
  tri_data.tri_adj = tri_adj;

  uint32_t total_edges = looptris_size * 3;

  BLI_task_parallel_range(0, looptris_size, &tri_data, lineart_load_tri_task, &tri_settings);

  /* Check for contour lines in the mesh.
   * IE check if the triangle edges lies in area where the triangles go from front facing to back
   * facing.
   */
  EdgeFeatReduceData edge_reduce = {0};
  TaskParallelSettings edge_feat_settings;
  BLI_parallel_range_settings_defaults(&edge_feat_settings);
  /* Set the minimum amount of edges a thread has to process. */
  edge_feat_settings.min_iter_per_thread = 4000;
  edge_feat_settings.userdata_chunk = &edge_reduce;
  edge_feat_settings.userdata_chunk_size = sizeof(EdgeFeatReduceData);
  edge_feat_settings.func_reduce = feat_data_sum_reduce;

  EdgeFeatData edge_feat_data = {nullptr};
  edge_feat_data.ld = la_data;
  edge_feat_data.me = me;
  edge_feat_data.material_indices = material_indices;
  edge_feat_data.edge_nabr = lineart_build_edge_neighbor(me, total_edges);
  edge_feat_data.tri_array = la_tri_arr;
  edge_feat_data.v_array = la_v_arr;
  edge_feat_data.crease_threshold = crease_angle;
  edge_feat_data.use_auto_smooth = use_auto_smooth;
  edge_feat_data.use_freestyle_edge = can_find_freestyle_edge;

  BLI_task_parallel_range(0,
                          total_edges,
                          &edge_feat_data,
                          lineart_identify_mlooptri_feature_edges,
                          &edge_feat_settings);

  LooseEdgeData loose_data = {0};

  if (la_data->conf.use_loose) {
    /* Only identifying floating edges at this point because other edges has been taken care of
     * inside #lineart_identify_mlooptri_feature_edges function. */
    int loose_count = 0;
    for (int e = 0; e < me->totedge; e++) {
      loose_count += (me->edges[e].flag & PG_LITE_EDGE_LOOSE) != 0;
    }
    loose_data.loose_array = static_cast<int *>(
        MEM_malloc_arrayN(loose_count, sizeof(int), __func__));
    if (loose_count > 0) {
      loose_data.loose_count = 0;
      for (int edge_i = 0; edge_i < me->totedge; edge_i++) {
        if (me->edges[edge_i].flag & PG_LITE_EDGE_LOOSE) {
          loose_data.loose_array[loose_data.loose_count] = edge_i;
          loose_data.loose_count++;
        }
      }
    }
  }

  int allocate_la_e = edge_reduce.feat_edges + loose_data.loose_count;

  LineartEdge *la_edge_arr = static_cast<LineartEdge *>(
      lineart_mem_acquire_thread(la_data->edge_data_pool, sizeof(LineartEdge) * allocate_la_e));
  LineartEdgeSegment *la_seg_arr = static_cast<LineartEdgeSegment *>(lineart_mem_acquire_thread(
      la_data->edge_data_pool, sizeof(LineartEdgeSegment) * allocate_la_e));
  BLI_spin_lock(&la_data->lock_task);
  elem_link_node = static_cast<LineartElementLinkNode *>(
      lineart_list_append_pointer_pool_sized_thread(&la_data->geom.line_buffer_pointers,
                                                    la_data->edge_data_pool,
                                                    la_edge_arr,
                                                    sizeof(LineartElementLinkNode)));
  BLI_spin_unlock(&la_data->lock_task);
  elem_link_node->element_count = allocate_la_e;
  elem_link_node->object_ref = orig_ob;
  elem_link_node->obindex = ob_info->obindex;

  /* Start of the edge/seg arr */
  LineartEdge *la_edge;
  LineartEdgeSegment *la_seg;
  la_edge = la_edge_arr;
  la_seg = la_seg_arr;

  for (int i = 0; i < total_edges; i++) {
    LineartEdgeNeighbor *edge_nabr = &edge_feat_data.edge_nabr[i];

    if (i < edge_nabr->e) {
      continue;
    }

    /* Not a feature line, so we skip. */
    if (edge_nabr->flags == 0) {
      continue;
    }

    LineartEdge *edge_added = nullptr;

    /* See eLineartEdgeFlag for details. */
    for (int flag_bit = 0; flag_bit < LRT_MESH_EDGE_TYPES_COUNT; flag_bit++) {
      int use_type = LRT_MESH_EDGE_TYPES[flag_bit];
      if (!(use_type & edge_nabr->flags)) {
        continue;
      }

      la_edge->v1 = &la_v_arr[edge_nabr->v1];
      la_edge->v2 = &la_v_arr[edge_nabr->v2];
      int findex = i / 3;
      la_edge->t1 = lineart_triangle_from_index(la_data, la_tri_arr, findex);
      if (!edge_added) {
        lineart_triangle_adjacent_assign(la_edge->t1, &tri_adj[findex], la_edge);
      }
      if (edge_nabr->e != -1) {
        findex = edge_nabr->e / 3;
        la_edge->t2 = lineart_triangle_from_index(la_data, la_tri_arr, findex);
        if (!edge_added) {
          lineart_triangle_adjacent_assign(la_edge->t2, &tri_adj[findex], la_edge);
        }
      }
      la_edge->flags = use_type;
      la_edge->object_ref = orig_ob;
      la_edge->edge_identifier = LRT_EDGE_IDENTIFIER(ob_info, la_edge);
      BLI_addtail(&la_edge->segments, la_seg);

      if (ELEM(usage,
               OBJECT_LRT_INHERIT,
               OBJECT_LRT_INCLUDE,
               OBJECT_LRT_NO_INTERSECTION,
               OBJECT_LRT_FORCE_INTERSECTION))
      {
        lineart_add_edge_to_array_thread(ob_info, la_edge);
      }

      if (edge_added) {
        edge_added->flags |= LRT_EDGE_FLAG_NEXT_IS_DUPLICATION;
      }

      edge_added = la_edge;

      la_edge++;
      la_seg++;

      if (!la_data->conf.allow_duplicated_types) {
        break;
      }
    }
  }

  if (loose_data.loose_array) {
    for (int i = 0; i < loose_data.loose_count; i++) {
      const PGLiteEdge &edge = me->edges[loose_data.loose_array[i]];
      la_edge->v1 = &la_v_arr[edge.v[0]];
      la_edge->v2 = &la_v_arr[edge.v[1]];
      la_edge->flags = LRT_EDGE_FLAG_LOOSE;
      la_edge->object_ref = orig_ob;
      la_edge->edge_identifier = LRT_EDGE_IDENTIFIER(ob_info, la_edge);
      BLI_addtail(&la_edge->segments, la_seg);
      if (ELEM(usage,
               OBJECT_LRT_INHERIT,
               OBJECT_LRT_INCLUDE,
               OBJECT_LRT_NO_INTERSECTION,
               OBJECT_LRT_FORCE_INTERSECTION))
      {
        lineart_add_edge_to_array_thread(ob_info, la_edge);
      }
      la_edge++;
      la_seg++;
    }
    MEM_SAFE_FREE(loose_data.loose_array);
  }

  MEM_freeN(edge_feat_data.edge_nabr);
}

//@@ lineart_intersection_mask_check
/* Collections are not modeled: no collection intersection masks. */
static uchar lineart_intersection_mask_check(const PGObjectLite * /*ob*/)
{
  return 0;
}

//@@ lineart_intersection_priority_check
/* Collections are not modeled: no collection intersection priority. */
static uchar lineart_intersection_priority_check(const PGObjectLite * /*ob*/)
{
  return 0;
}

//@@ lineart_usage_check
/* lineart_usage_check() without collections: the object's own line art usage (INHERIT means
 * INCLUDE, as when no collection overrides it). */
static int lineart_usage_check(const PGObjectLite *ob)
{
  if (ob->line_art_usage != OBJECT_LRT_INHERIT) {
    return ob->line_art_usage;
  }
  return OBJECT_LRT_INHERIT;
}

//@@ lineart_geometry_check_visible
static bool lineart_geometry_check_visible(double model_view_proj[4][4],
                                           double shift_x,
                                           double shift_y,
                                           const PGMeshLite *use_mesh)
{
  if (!use_mesh || use_mesh->totvert == 0) {
    return false;
  }
  float mesh_min[3], mesh_max[3];
  INIT_MINMAX(mesh_min, mesh_max);
  for (int i = 0; i < use_mesh->totvert; i++) {
    minmax_v3v3_v3(mesh_min, mesh_max, use_mesh->verts[i]);
  }
  /* BKE_boundbox_init_from_minmax() corner order */
  float bb[8][3];
  bb[0][0] = bb[1][0] = bb[2][0] = bb[3][0] = mesh_min[0];
  bb[4][0] = bb[5][0] = bb[6][0] = bb[7][0] = mesh_max[0];
  bb[0][1] = bb[1][1] = bb[4][1] = bb[5][1] = mesh_min[1];
  bb[2][1] = bb[3][1] = bb[6][1] = bb[7][1] = mesh_max[1];
  bb[0][2] = bb[3][2] = bb[4][2] = bb[7][2] = mesh_min[2];
  bb[1][2] = bb[2][2] = bb[5][2] = bb[6][2] = mesh_max[2];

  double co[8][4];
  double tmp[3];
  for (int i = 0; i < 8; i++) {
    copy_v3db_v3fl(co[i], bb[i]);
    copy_v3_v3_db(tmp, co[i]);
    mul_v4_m4v3_db(co[i], model_view_proj, tmp);
    co[i][0] -= shift_x * 2 * co[i][3];
    co[i][1] -= shift_y * 2 * co[i][3];
  }

  bool cond[6] = {true, true, true, true, true, true};
  /* Because for a point to be inside clip space, it must satisfy `-Wc <= XYCc <= Wc`, here if
   * all verts falls to the same side of the clip space border, we know it's outside view. */
  for (int i = 0; i < 8; i++) {
    cond[0] &= (co[i][0] < -co[i][3]);
    cond[1] &= (co[i][0] > co[i][3]);
    cond[2] &= (co[i][1] < -co[i][3]);
    cond[3] &= (co[i][1] > co[i][3]);
    cond[4] &= (co[i][2] < -co[i][3]);
    cond[5] &= (co[i][2] > co[i][3]);
  }
  for (int i = 0; i < 6; i++) {
    if (cond[i]) {
      return false;
    }
  }
  return true;
}

//@@ lineart_object_load_single_instance
static void lineart_object_load_single_instance(LineartData *ld,
                                                const PGObjectLite *ob,
                                                float use_mat[4][4],
                                                LineartObjectLoadTaskInfo *olti,
                                                int thread_count,
                                                int obindex)
{
  LineartObjectInfo *obi = static_cast<LineartObjectInfo *>(
      lineart_mem_acquire(&ld->render_data_pool, sizeof(LineartObjectInfo)));
  obi->usage = lineart_usage_check(ob);
  obi->override_intersection_mask = lineart_intersection_mask_check(ob);
  obi->intersection_priority = lineart_intersection_priority_check(ob);

  if (obi->usage == OBJECT_LRT_EXCLUDE) {
    return;
  }

  obi->obindex = obindex << LRT_OBINDEX_SHIFT;

  /* Prepare the matrix used for transforming this specific object (instance). This has to be
   * done before mesh boundbox check because the function needs that. */
  mul_m4db_m4db_m4fl(obi->model_view_proj, ld->conf.view_projection, use_mat);
  mul_m4db_m4db_m4fl(obi->model_view, ld->conf.view, use_mat);

  if (!lineart_geometry_check_visible(
          obi->model_view_proj, ld->conf.shift_x, ld->conf.shift_y, &ob->mesh))
  {
    return;
  }

  /* Make normal matrix. */
  float imat[4][4];
  invert_m4_m4(imat, use_mat);
  transpose_m4(imat);
  copy_m4d_m4(obi->normal, imat);

  /* Identity only: see pg_obi_object(). */
  obi->original_me = reinterpret_cast<Mesh *>(const_cast<PGObjectLite *>(ob));
  obi->original_ob = reinterpret_cast<Object *>(const_cast<PGObjectLite *>(ob));
  obi->original_ob_eval = obi->original_ob;
  lineart_geometry_load_assign_thread(olti, obi, thread_count, ob->mesh.totpoly);
}

//@@ lineart_main_load_geometries
void lineart_main_load_geometries(const PGSceneLite *scene,
                                  LineartData *ld,
                                  ListBase *shadow_elns)
{
  double proj[4][4], view[4][4], result[4][4];
  float inv[4][4];

  /* Camera projection as in Blender (see pg_lite_view_projection(), which carries the verbatim
   * camera math); ld->conf.cam_obmat was set (axes normalized) by lineart_create_render_buffer. */
  {
    pg_lite_view_projection(&scene->camera, ld->w, ld->h, ld->conf.overscan, proj);
    copy_m4_m4_db(ld->conf.view_projection, proj);

    unit_m4_db(view);
    copy_m4_m4_db(ld->conf.view, view);
  }
  (void)result;
  (void)inv;

  BLI_listbase_clear(&ld->geom.triangle_buffer_pointers);
  BLI_listbase_clear(&ld->geom.vertex_buffer_pointers);

  int thread_count = ld->thread_count;
  int obindex = 0;

  /* This memory is in render buffer memory pool. So we don't need to free those after loading. */
  LineartObjectLoadTaskInfo *olti = static_cast<LineartObjectLoadTaskInfo *>(lineart_mem_acquire(
      &ld->render_data_pool, sizeof(LineartObjectLoadTaskInfo) * thread_count));

  /* DEG_OBJECT_ITER over the visible objects -> the Scene-lite objects in order. */
  for (int i = 0; i < scene->totobject; i++) {
    obindex++;
    const PGObjectLite *ob = &scene->objects[i];
    float use_mat[4][4];
    copy_m4_m4(use_mat, ob->matrix_world);
    lineart_object_load_single_instance(ld, ob, use_mat, olti, thread_count, obindex);
  }

  TaskPool *tp = BLI_task_pool_create(nullptr, TASK_PRIORITY_HIGH);

  for (int i = 0; i < thread_count; i++) {
    olti[i].ld = ld;
    olti[i].shadow_elns = shadow_elns;
    olti[i].thread_id = i;
    BLI_task_pool_push(tp, (TaskRunFunction)lineart_object_load_worker, &olti[i], false, nullptr);
  }
  BLI_task_pool_work_and_wait(tp);
  BLI_task_pool_free(tp);

  /* The step below is to serialize vertex index in the whole scene, so
   * lineart_triangle_share_edge() can work properly from the lack of triangle adjacent info. */
  int global_i = 0;

  int edge_count = 0;
  for (int i = 0; i < thread_count; i++) {
    for (LineartObjectInfo *obi = olti[i].pending; obi; obi = obi->next) {
      if (!obi->v_eln) {
        continue;
      }
      edge_count += obi->pending_edges.next;
    }
  }
  lineart_finalize_object_edge_array_reserve(&ld->pending_edges, edge_count);

  for (int i = 0; i < thread_count; i++) {
    for (LineartObjectInfo *obi = olti[i].pending; obi; obi = obi->next) {
      if (!obi->v_eln) {
        continue;
      }
      LineartVert *v = (LineartVert *)obi->v_eln->pointer;
      int v_count = obi->v_eln->element_count;
      obi->v_eln->global_index_offset = global_i;
      for (int vi = 0; vi < v_count; vi++) {
        v[vi].index += global_i;
      }
      obi->global_i_offset = global_i;
      global_i += v_count;
      lineart_finalize_object_edge_array(&ld->pending_edges, obi);
    }
  }
}

//@@ lineart_create_render_buffer
static LineartData *lineart_create_render_buffer(const PGSceneLite *scene,
                                                 const PGLineartSettings *lmd,
                                                 LineartCache *lc)
{
  LineartData *ld = static_cast<LineartData *>(
      MEM_callocN(sizeof(LineartData), "Line Art render buffer"));
  lc->all_enabled_edge_types = lmd->edge_types;

  const PGCameraLite *c = &scene->camera;
  double clipping_offset = 0;

  if (lmd->calculation_flags & LRT_ALLOW_CLIPPING_BOUNDARIES) {
    /* This way the clipped lines are "stably visible" by prevents depth buffer artifacts. */
    clipping_offset = 0.0001;
  }

  copy_v3db_v3fl(ld->conf.camera_pos, c->matrix_world[3]);
  copy_v3db_v3fl(ld->conf.active_camera_pos, c->matrix_world[3]);
  copy_m4_m4(ld->conf.cam_obmat, c->matrix_world);
  /* Make sure none of the scaling factor makes in, line art expects no scaling on cameras and
   * lights. */
  normalize_v3(ld->conf.cam_obmat[0]);
  normalize_v3(ld->conf.cam_obmat[1]);
  normalize_v3(ld->conf.cam_obmat[2]);

  ld->conf.cam_is_persp = (c->type == CAM_PERSP);
  ld->conf.near_clip = c->clip_start + clipping_offset;
  ld->conf.far_clip = c->clip_end - clipping_offset;
  ld->w = scene->width;
  ld->h = scene->height;

  if (ld->conf.cam_is_persp) {
    ld->qtree.recursive_level = LRT_TILE_RECURSIVE_PERSPECTIVE;
  }
  else {
    ld->qtree.recursive_level = LRT_TILE_RECURSIVE_ORTHO;
  }

  double shift_x, shift_y;
  pg_lite_camera_shift(c, ld->w, ld->h, &shift_x, &shift_y);
  ld->conf.shift_x = shift_x;
  ld->conf.shift_y = shift_y;

  ld->conf.overscan = lmd->overscan;

  ld->conf.shift_x /= (1 + ld->conf.overscan);
  ld->conf.shift_y /= (1 + ld->conf.overscan);

  ld->conf.crease_threshold = cos(M_PI - lmd->crease_threshold);
  ld->conf.chaining_image_threshold = 0.001f;
  ld->conf.angle_splitting_threshold = 0.0f;
  ld->conf.chain_smooth_tolerance = 0.0f;

  ld->conf.fuzzy_intersections = (lmd->calculation_flags & LRT_INTERSECTION_AS_CONTOUR) != 0;
  ld->conf.fuzzy_everything = (lmd->calculation_flags & LRT_EVERYTHING_AS_CONTOUR) != 0;
  ld->conf.allow_boundaries = (lmd->calculation_flags & LRT_ALLOW_CLIPPING_BOUNDARIES) != 0;
  ld->conf.use_loose_as_contour = (lmd->calculation_flags & LRT_LOOSE_AS_CONTOUR) != 0;
  ld->conf.use_loose_edge_chain = (lmd->calculation_flags & LRT_CHAIN_LOOSE_EDGES) != 0;
  ld->conf.use_geometry_space_chain = (lmd->calculation_flags & LRT_CHAIN_GEOMETRY_SPACE) != 0;
  ld->conf.use_image_boundary_trimming = (lmd->calculation_flags &
                                          LRT_USE_IMAGE_BOUNDARY_TRIMMING) != 0;

  /* See lineart_edge_from_triangle() for how this option may impact performance. */
  ld->conf.allow_overlapping_edges = (lmd->calculation_flags & LRT_ALLOW_OVERLAPPING_EDGES) != 0;

  ld->conf.allow_duplicated_types = (lmd->calculation_flags & LRT_ALLOW_OVERLAP_EDGE_TYPES) != 0;

  ld->conf.force_crease = (lmd->calculation_flags & LRT_USE_CREASE_ON_SMOOTH_SURFACES) != 0;
  ld->conf.sharp_as_crease = (lmd->calculation_flags & LRT_USE_CREASE_ON_SHARP_EDGES) != 0;

  ld->conf.chain_preserve_details = (lmd->calculation_flags & LRT_CHAIN_PRESERVE_DETAILS) != 0;

  /* This is used to limit calculation to a certain level to save time, lines who have higher
   * occlusion levels will get ignored. */
  ld->conf.max_occlusion_level = lmd->level_end;

  int16_t edge_types = lmd->edge_types;

  ld->conf.use_contour = (edge_types & LRT_EDGE_FLAG_CONTOUR) != 0;
  ld->conf.use_crease = (edge_types & LRT_EDGE_FLAG_CREASE) != 0;
  ld->conf.use_material = (edge_types & LRT_EDGE_FLAG_MATERIAL) != 0;
  ld->conf.use_edge_marks = (edge_types & LRT_EDGE_FLAG_EDGE_MARK) != 0;
  ld->conf.use_intersections = (edge_types & LRT_EDGE_FLAG_INTERSECTION) != 0;
  ld->conf.use_loose = (edge_types & LRT_EDGE_FLAG_LOOSE) != 0;
  /* Light contour and projected shadow need a light object: not supported. */
  ld->conf.use_light_contour = false;
  ld->conf.use_shadow = false;

  ld->conf.shadow_selection = 0;
  ld->conf.shadow_enclose_shapes = false;
  ld->conf.shadow_use_silhouette = false;

  ld->conf.use_back_face_culling = (lmd->calculation_flags & LRT_USE_BACK_FACE_CULLING) != 0;

  ld->conf.filter_face_mark_invert = (lmd->calculation_flags & LRT_FILTER_FACE_MARK_INVERT) != 0;
  ld->conf.filter_face_mark = (lmd->calculation_flags & LRT_FILTER_FACE_MARK) != 0;
  ld->conf.filter_face_mark_boundaries = (lmd->calculation_flags &
                                          LRT_FILTER_FACE_MARK_BOUNDARIES) != 0;
  ld->conf.filter_face_mark_keep_contour = (lmd->calculation_flags &
                                            LRT_FILTER_FACE_MARK_KEEP_CONTOUR) != 0;

  ld->chain_data_pool = &lc->chain_data_pool;

  /* See #LineartData::edge_data_pool for explanation. */
  ld->edge_data_pool = &ld->render_data_pool;

  BLI_spin_init(&ld->lock_task);
  BLI_spin_init(&ld->lock_cuts);
  BLI_spin_init(&ld->render_data_pool.lock_mem);

  /* One thread: see lineart_lite_runtime.cc. */
  ld->thread_count = 1;

  return ld;
}

//@@ MOD_lineart_compute_feature_lines
/* MOD_lineart_compute_feature_lines() up to and including the occlusion stage (chaining and GP
 * output are batch 3). Returns the LineartData for pg_lineart_compute() to read; nullptr when the
 * scene has no geometry in view. */
static LineartData *pg_lineart_compute_occlusion(const PGSceneLite *scene,
                                                 const PGLineartSettings *lmd,
                                                 LineartCache **cached_result)
{
  LineartData *ld;

  LineartCache *lc = lineart_init_cache();
  *cached_result = lc;

  ld = lineart_create_render_buffer(scene, lmd, lc);

  /* Triangle thread testing data size varies depending on the thread count.
   * See definition of LineartTriangleThread for details. */
  ld->sizeof_triangle = lineart_triangle_size_get(ld);

  /* Get view vector before loading geometries, because we detect feature lines there. */
  lineart_main_get_view_vector(ld);

  lineart_main_load_geometries(scene, ld, nullptr);

  if (!ld->geom.vertex_buffer_pointers.first) {
    /* No geometry loaded, return early. */
    return ld;
  }

  /* Initialize the bounding box acceleration structure, it's a lot like BVH in 3D. */
  lineart_main_bounding_area_make_initial(ld);

  /* We need to get cut into triangles that are crossing near/far plans, only this way can we get
   * correct coordinates of those clipped lines. Done in two steps,
   * setting clip_far==false for near plane. */
  lineart_main_cull_triangles(ld, false);
  /* `clip_far == true` for far plane. */
  lineart_main_cull_triangles(ld, true);

  /* At this point triangle adjacent info pointers is no longer needed, free them. */
  lineart_main_free_adjacent_data(ld);

  /* Do the perspective division after clipping is done. */
  lineart_main_perspective_division(ld);

  lineart_main_discard_out_of_frame_edges(ld);

  /* Triangle intersections are done here during sequential adding of them. Only after this,
   * triangles and lines are all linked with acceleration structure, and the 2D occlusion stage
   * can do its job. */
  lineart_main_add_triangles(ld);

  /* Re-link bounding areas because they have been subdivided by worker threads and we need
   * adjacent info. */
  lineart_main_bounding_areas_connect_post(ld);

  /* Link lines to acceleration structure, this can only be done after perspective division, if
   * we do it after triangles being added, the acceleration structure has already been
   * subdivided, this way we do less list manipulations. */
  lineart_main_link_lines(ld);

  /* Occlusion is work-and-wait. This call will not return before work is completed. */
  lineart_main_occlusion_begin(ld);

  return ld;
}

//@@ epilogue
/* ---- Project Grease API (project_grease_lineart_lite.h) ----------------------------------- */
void pg_lineart_settings_default(PGLineartSettings *s)
{
  /* _DNA_DEFAULT_LineartGpencilModifierData */
  s->edge_types = LRT_EDGE_FLAG_INIT_TYPE;
  s->calculation_flags = LRT_ALLOW_DUPLI_OBJECTS | LRT_ALLOW_CLIPPING_BOUNDARIES |
                         LRT_USE_CREASE_ON_SHARP_EDGES | LRT_FILTER_FACE_MARK_KEEP_CONTOUR |
                         LRT_GPENCIL_MATCH_OUTPUT_VGROUP;
  s->crease_threshold = DEG2RADF(140.0f);
  s->overscan = 0.1f;
  s->level_end = 0;
}

static int pg_lineart_object_index(const PGSceneLite *scene, const void *object_ref)
{
  for (int i = 0; i < scene->totobject; i++) {
    if (object_ref == &scene->objects[i]) {
      return i;
    }
  }
  return -1;
}

int pg_lineart_compute(const PGSceneLite *scene,
                       const PGLineartSettings *settings,
                       PGLineartSegment **r_segments)
{
  *r_segments = nullptr;
  if (!scene || !settings || scene->width < 1 || scene->height < 1) {
    return -1;
  }
  LineartCache *lc = nullptr;
  LineartData *ld = pg_lineart_compute_occlusion(scene, settings, &lc);
  if (!ld) {
    MOD_lineart_clear_cache(&lc);
    return -1;
  }
  /* Every feature edge (mesh edges and intersection lines) is in pending_edges; its segments
   * split it at ratio points (in projected 2D) with the occlusion level after each point. */
  int count = 0;
  for (int i = 0; i < ld->pending_edges.next; i++) {
    LineartEdge *e = ld->pending_edges.array[i];
    LISTBASE_FOREACH (LineartEdgeSegment *, es, &e->segments) {
      count++;
    }
  }
  PGLineartSegment *out = static_cast<PGLineartSegment *>(
      malloc(sizeof(PGLineartSegment) * size_t(count > 0 ? count : 1)));
  int n = 0;
  for (int i = 0; i < ld->pending_edges.next && out; i++) {
    LineartEdge *e = ld->pending_edges.array[i];
    LISTBASE_FOREACH (LineartEdgeSegment *, es, &e->segments) {
      const double r0 = es->ratio;
      const double r1 = es->next ? es->next->ratio : 1.0;
      PGLineartSegment &s = out[n++];
      s.x0 = float(interpd(e->v2->fbcoord[0], e->v1->fbcoord[0], r0));
      s.y0 = float(interpd(e->v2->fbcoord[1], e->v1->fbcoord[1], r0));
      s.x1 = float(interpd(e->v2->fbcoord[0], e->v1->fbcoord[0], r1));
      s.y1 = float(interpd(e->v2->fbcoord[1], e->v1->fbcoord[1], r1));
      s.occlusion = es->occlusion;
      s.edge_type = int(e->flags & LRT_EDGE_FLAG_ALL_TYPE);
      s.object_index = (e->flags & LRT_EDGE_FLAG_INTERSECTION) ?
                           -1 :
                           pg_lineart_object_index(scene, e->object_ref);
    }
  }
  lineart_destroy_render_data_keep_init(ld);
  MEM_freeN(ld);
  MOD_lineart_clear_cache(&lc);
  *r_segments = out;
  return out ? n : -1;
}

void pg_lineart_free_segments(PGLineartSegment *segments)
{
  free(segments);
}
