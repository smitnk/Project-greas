#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DArray;
precision highp sampler3D;
precision highp samplerBuffer;
precision highp isamplerBuffer;
precision highp usamplerBuffer;
precision highp sampler2DShadow;
precision highp usampler2D;
precision highp isampler2D;
uniform int gpu_BaseInstance;
#define gpu_InstanceIndex (gl_InstanceID + gpu_BaseInstance)
#define gpu_Array(_type) _type[]
#define DFDX_SIGN 1.0
#define DFDY_SIGN 1.0

/* Cubemap support and fallback implementation declarations. */
#ifdef GPU_ARB_texture_cube_map_array
#  define textureLod_cubemapArray(tex, co, lod) textureLod(tex, co, lod)
#else
#endif

/* Texture format tokens -- Type explicitness required by other Graphics APIs. */
#define depth2D sampler2D
#define depth2DArray sampler2DArray
#define depth2DMS sampler2DMS
#define depth2DMSArray sampler2DMSArray
#define depthCube samplerCube
#define depthCubeArray samplerCubeArray
#define depth2DArrayShadow sampler2DArrayShadow

/* Backend Functions. */
#define select(A, B, mask) mix(A, B, mask)

bool is_zero(vec2 A)
{
  return all(equal(A, vec2(0.0)));
}

bool is_zero(vec3 A)
{
  return all(equal(A, vec3(0.0)));
}

bool is_zero(vec4 A)
{
  return all(equal(A, vec4(0.0)));
}
#define GPU_SHADER
#define GPU_OPENGL
#define GPU_VERTEX_SHADER
#define ModelMatrix (drw_matrices[resource_id].model)
#define ModelMatrixInverse (drw_matrices[resource_id].model_inverse)
#define drw_view drw_view_[drw_view_id]
#define DYNAMIC_RESOURCE_ID 
#define USE_GPU_SHADER_CREATE_INFO
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 *
 * Glue definition to make shared declaration of struct & functions work in both C / C++ and GLSL.
 * We use the same vector and matrix types as Blender C++. Some math functions are defined to use
 * the float version to match the GLSL syntax.
 * This file can be used for C & C++ code and the syntax used should follow the same rules.
 * Some preprocessing is done by the GPU back-end to make it GLSL compatible.
 *
 * IMPORTANT:
 * - Always use `u` suffix for enum values. GLSL do not support implicit cast.
 * - Define all values. This is in order to simplify custom pre-processor code.
 * - (C++ only) Always use `uint32_t` as underlying type (`enum eMyEnum : uint32_t`).
 * - (C only) do NOT use the enum type inside UBO/SSBO structs and use `uint` instead.
 * - Use float suffix by default for float literals to avoid double promotion in C++.
 * - Pack one float or int after a vec3/ivec3 to fulfill alignment rules.
 *
 * NOTE: Due to alignment restriction and buggy drivers, do not try to use mat3 inside structs.
 * NOTE: (UBO only) Do not use arrays of float. They are padded to arrays of vec4 and are not worth
 * it. This does not apply to SSBO.
 *
 * IMPORTANT: Do not forget to align mat4, vec3 and vec4 to 16 bytes, and vec2 to 8 bytes.
 *
 * NOTE: You can use bool type using bool1 a int boolean type matching the GLSL type.
 */

#ifdef GPU_SHADER
#  define BLI_STATIC_ASSERT_ALIGN(type_, align_)
#  define BLI_STATIC_ASSERT_SIZE(type_, size_)
#  define static
#  define inline
#  define cosf cos
#  define sinf sin
#  define tanf tan
#  define acosf acos
#  define asinf asin
#  define atanf atan
#  define floorf floor
#  define ceilf ceil
#  define sqrtf sqrt
#  define expf exp

#  define bool1 bool
/* Type name collision with Metal shading language - These type-names are already defined. */
#  ifndef GPU_METAL
#    define float2 vec2
#    define float3 vec3
#    define float4 vec4
#    define float4x4 mat4
#    define int2 ivec2
#    define int3 ivec3
#    define int4 ivec4
#    define uint2 uvec2
#    define uint3 uvec3
#    define uint4 uvec4
#    define bool2 bvec2
#    define bool3 bvec3
#    define bool4 bvec4
#    define packed_float3 vec3
#  endif

#else /* C / C++ */
#  pragma once

#  include  BLI_assert.h 

#  ifdef __cplusplus
#    include  BLI_math_matrix_types.hh 
#    include  BLI_math_vector_types.hh 
using blender::float2;
using blender::float3;
using blender::float4;
using blender::float4x4;
using blender::int2;
using blender::int3;
using blender::int4;
using blender::uint2;
using blender::uint3;
using blender::uint4;
using bool1 = int;
using bool2 = blender::int2;
using bool3 = blender::int3;
using bool4 = blender::int4;
using packed_float3 = blender::float3;

#  else /* C */
typedef float float2[2];
typedef float float3[3];
typedef float float4[4];
typedef float float4x4[4][4];
typedef int int2[2];
typedef int int3[2];
typedef int int4[4];
typedef uint uint2[2];
typedef uint uint3[3];
typedef uint uint4[4];
typedef int bool1;
typedef int bool2[2];
typedef int bool3[2];
typedef int bool4[4];
typedef float3 packed_float3;
#  endif

#endif
/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  include  GPU_shader_shared_utils.h 

#  include  DNA_action_types.h 
#  include  DNA_view3d_types.h 

#  ifdef __cplusplus
extern  C  {
#  else
typedef enum OVERLAY_GridBits OVERLAY_GridBits;
#  endif
typedef struct OVERLAY_GridData OVERLAY_GridData;
#endif

/* TODO(fclem): Should eventually become OVERLAY_BackgroundType.
 * But there is no uint push constant functions at the moment. */
#define BG_SOLID 0
#define BG_GRADIENT 1
#define BG_CHECKER 2
#define BG_RADIAL 3
#define BG_SOLID_CHECKER 4
#define BG_MASK 5

#define  OVERLAY_GridBits  uint
const uint 
  SHOW_AXIS_X = (1u << 0u),
  SHOW_AXIS_Y = (1u << 1u),
  SHOW_AXIS_Z = (1u << 2u),
  SHOW_GRID = (1u << 3u),
  PLANE_XY = (1u << 4u),
  PLANE_XZ = (1u << 5u),
  PLANE_YZ = (1u << 6u),
  CLIP_ZPOS = (1u << 7u),
  CLIP_ZNEG = (1u << 8u),
  GRID_BACK = (1u << 9u),
  GRID_CAMERA = (1u << 10u),
  PLANE_IMAGE = (1u << 11u),
  CUSTOM_GRID = (1u << 12u);
#ifndef GPU_SHADER
ENUM_OPERATORS(OVERLAY_GridBits, CUSTOM_GRID)
#endif

/* Match: #SI_GRID_STEPS_LEN */
#define OVERLAY_GRID_STEPS_LEN 8

struct OVERLAY_GridData {
  float4 steps[OVERLAY_GRID_STEPS_LEN]; /* float arrays are padded to float4 in std130. */
  float4 size;                          /* float3 padded to float4. */
  float distance;
  float line_size;
  float zoom_factor; /* Only for UV editor */
  float _pad0;
};
BLI_STATIC_ASSERT_ALIGN(OVERLAY_GridData, 16)

#ifdef GPU_SHADER
/* Keep the same values as in `draw_cache_imp_curve.c` */
#  define ACTIVE_NURB (1u << 2)
#  define BEZIER_HANDLE (1u << 3)
#  define EVEN_U_BIT (1u << 4)
#  define COLOR_SHIFT 5u

/* Keep the same value in `handle_display` in `DNA_view3d_types.h` */
#  define CURVE_HANDLE_SELECTED 0u
#  define CURVE_HANDLE_ALL 1u

#  define GP_EDIT_POINT_SELECTED 1u  /* 1 << 0 */
#  define GP_EDIT_STROKE_SELECTED 2u /* 1 << 1 */
#  define GP_EDIT_MULTIFRAME 4u      /* 1 << 2 */
#  define GP_EDIT_STROKE_START 8u    /* 1 << 3 */
#  define GP_EDIT_STROKE_END 16u     /* 1 << 4 */
#  define GP_EDIT_POINT_DIMMED 32u   /* 1 << 5 */

#  define MOTIONPATH_VERT_SEL (1u << 0)
#  define MOTIONPATH_VERT_KEY (1u << 1)

#else
/* TODO(fclem): Find a better way to share enums/defines from DNA files with GLSL. */
BLI_STATIC_ASSERT(CURVE_HANDLE_SELECTED == 0u,  Ensure value is sync );
BLI_STATIC_ASSERT(CURVE_HANDLE_ALL == 1u,  Ensure value is sync );
BLI_STATIC_ASSERT(MOTIONPATH_VERT_SEL == (1u << 0),  Ensure value is sync );
BLI_STATIC_ASSERT(MOTIONPATH_VERT_KEY == (1u << 1),  Ensure value is sync );
#endif

#ifndef GPU_SHADER
#  ifdef __cplusplus
}
#  endif
#endif
/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  pragma once

#  include  GPU_shader.h 
#  include  GPU_shader_shared_utils.h 
#  include  draw_defines.h 

typedef struct ViewCullingData ViewCullingData;
typedef struct ViewMatrices ViewMatrices;
typedef struct ObjectMatrices ObjectMatrices;
typedef struct ObjectInfos ObjectInfos;
typedef struct ObjectBounds ObjectBounds;
typedef struct VolumeInfos VolumeInfos;
typedef struct CurvesInfos CurvesInfos;
typedef struct ObjectAttribute ObjectAttribute;
typedef struct LayerAttribute LayerAttribute;
typedef struct DrawCommand DrawCommand;
typedef struct DispatchCommand DispatchCommand;
typedef struct DRWDebugPrintBuffer DRWDebugPrintBuffer;
typedef struct DRWDebugVert DRWDebugVert;
typedef struct DRWDebugDrawBuffer DRWDebugDrawBuffer;
typedef struct FrustumCorners FrustumCorners;
typedef struct FrustumPlanes FrustumPlanes;

/* __cplusplus is true when compiling with MSL. */
#  if defined(__cplusplus) && !defined(GPU_SHADER)
/* C++ only forward declarations. */
struct Object;
struct ViewLayer;
struct GPUUniformAttr;
struct GPULayerAttr;

namespace blender::draw {

struct ObjectRef;

}  // namespace blender::draw

#  else /* __cplusplus */
/* C only forward declarations. */
typedef enum eObjectInfoFlag eObjectInfoFlag;

#  endif
#endif

#define DRW_SHADER_SHARED_H

#define DRW_RESOURCE_CHUNK_LEN 512

/* Define the maximum number of grid we allow in a volume UBO. */
#define DRW_GRID_PER_VOLUME_MAX 16

/* Define the maximum number of attribute we allow in a curves UBO.
 * This should be kept in sync with `GPU_ATTR_MAX` */
#define DRW_ATTRIBUTE_PER_CURVES_MAX 15

/* -------------------------------------------------------------------- */
/** \name Views
 * \{ */

/**
 * The maximum of indexable views is dictated by:
 * - The UBO limit (16KiB) of the ViewMatrices container.
 * - The maximum resource index supported for shaders using multi-view (see DRW_VIEW_SHIFT).
 */
#define DRW_VIEW_MAX 64

#ifndef DRW_VIEW_LEN
/* Single-view case (default). */
#  define drw_view_id 0
#  define DRW_VIEW_LEN 1
#  define DRW_VIEW_SHIFT 0
#  define DRW_VIEW_FROM_RESOURCE_ID
#else

/* Multi-view case. */
/** This should be already defined at shaderCreateInfo level. */
// #  define DRW_VIEW_LEN 64
/** Global that needs to be set correctly in each shader stage. */
uint drw_view_id = 0;
/**
 * In order to reduce the memory requirements, the view id is merged with resource id to avoid
 * doubling the memory required only for view indexing.
 */
/** \note This is simply log2(DRW_VIEW_LEN) but making sure it is optimized out. */
#  define DRW_VIEW_SHIFT \
    ((DRW_VIEW_LEN > 32) ? 6 : \
     (DRW_VIEW_LEN > 16) ? 5 : \
     (DRW_VIEW_LEN > 8)  ? 4 : \
     (DRW_VIEW_LEN > 4)  ? 3 : \
     (DRW_VIEW_LEN > 2)  ? 2 : \
                           1)
#  define DRW_VIEW_MASK ~(0xFFFFFFFFu << DRW_VIEW_SHIFT)
#  define DRW_VIEW_FROM_RESOURCE_ID drw_view_id = (drw_ResourceID & DRW_VIEW_MASK)
#endif

struct FrustumCorners {
  float4 corners[8];
};
BLI_STATIC_ASSERT_ALIGN(FrustumCorners, 16)

struct FrustumPlanes {
  /* [0] left
   * [1] right
   * [2] bottom
   * [3] top
   * [4] near
   * [5] far */
  float4 planes[6];
};
BLI_STATIC_ASSERT_ALIGN(FrustumPlanes, 16)

struct ViewCullingData {
  /** \note vec3 array padded to vec4. */
  /** Frustum corners. */
  FrustumCorners frustum_corners;
  FrustumPlanes frustum_planes;
  float4 bound_sphere;
};
BLI_STATIC_ASSERT_ALIGN(ViewCullingData, 16)

struct ViewMatrices {
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;
};
BLI_STATIC_ASSERT_ALIGN(ViewMatrices, 16)

/* Do not override old definitions if the shader uses this header but not shader info. */
#ifdef USE_GPU_SHADER_CREATE_INFO
/* TODO(@fclem): Mass rename. */
#  define ViewMatrix drw_view.viewmat
#  define ViewMatrixInverse drw_view.viewinv
#  define ProjectionMatrix drw_view.winmat
#  define ProjectionMatrixInverse drw_view.wininv
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 * \{ */

struct ObjectMatrices {
  float4x4 model;
  float4x4 model_inverse;

#if !defined(GPU_SHADER) && defined(__cplusplus)
  void sync(const Object &object);
  void sync(const float4x4 &model_matrix);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectMatrices, 16)

#define  eObjectInfoFlag  uint
const uint 
  OBJECT_SELECTED = (1u << 0u),
  OBJECT_FROM_DUPLI = (1u << 1u),
  OBJECT_FROM_SET = (1u << 2u),
  OBJECT_ACTIVE = (1u << 3u),
  OBJECT_NEGATIVE_SCALE = (1u << 4u),
  /* Avoid skipped info to change culling. */
  OBJECT_NO_INFO = ~OBJECT_NEGATIVE_SCALE
;

struct ObjectInfos {
#if defined(GPU_SHADER) && !defined(DRAW_FINALIZE_SHADER)
  /* TODO Rename to struct member for glsl too. */
  float4 orco_mul_bias[2];
  float4 ob_color;
  float4 infos;
#else
  /** Uploaded as center + size. Converted to mul+bias to local coord. */
  float3 orco_add;
  uint object_attrs_offset;
  float3 orco_mul;
  uint object_attrs_len;

  float4 ob_color;
  uint index;
  uint _pad2;
  float random;
  eObjectInfoFlag flag;
#endif

#if !defined(GPU_SHADER) && defined(__cplusplus)
  void sync();
  void sync(const blender::draw::ObjectRef ref, bool is_active_object);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectInfos, 16)

struct ObjectBounds {
  /**
   * Uploaded as vertex (0, 4, 3, 1) of the bbox in local space, matching XYZ axis order.
   * Then processed by GPU and stored as (0, 4-0, 3-0, 1-0) in world space for faster culling.
   */
  float4 bounding_corners[4];
  /** Bounding sphere derived from the bounding corner. Computed on GPU. */
  float4 bounding_sphere;
  /** Radius of the inscribed sphere derived from the bounding corner. Computed on GPU. */
#define _inner_sphere_radius bounding_corners[3].w

#if !defined(GPU_SHADER) && defined(__cplusplus)
  void sync();
  void sync(Object &ob);
  void sync(const float3 &center, const float3 &size);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectBounds, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object attributes
 * \{ */

struct VolumeInfos {
  /** Object to grid-space. */
  float4x4 grids_xform[DRW_GRID_PER_VOLUME_MAX];
  /** \note vec4 for alignment. Only float3 needed. */
  float4 color_mul;
  float density_scale;
  float temperature_mul;
  float temperature_bias;
  float _pad;
};
BLI_STATIC_ASSERT_ALIGN(VolumeInfos, 16)

struct CurvesInfos {
  /** Per attribute scope, follows loading order.
   * \note uint as bool in GLSL is 4 bytes.
   * \note GLSL pad arrays of scalar to 16 bytes (std140). */
  uint4 is_point_attribute[DRW_ATTRIBUTE_PER_CURVES_MAX];
};
BLI_STATIC_ASSERT_ALIGN(CurvesInfos, 16)

#pragma pack(push, 4)
struct ObjectAttribute {
  /* Workaround the padding cost from alignment requirements.
   * (see GL spec : 7.6.2.2 Standard Uniform Block Layout) */
  float data_x, data_y, data_z, data_w;
  uint hash_code;

#if !defined(GPU_SHADER) && defined(__cplusplus)
  bool sync(const blender::draw::ObjectRef &ref, const GPUUniformAttr &attr);
#endif
};
#pragma pack(pop)
/** \note we only align to 4 bytes and fetch data manually so make sure
 * C++ compiler gives us the same size. */
BLI_STATIC_ASSERT_ALIGN(ObjectAttribute, 20)

#pragma pack(push, 4)
struct LayerAttribute {
  float4 data;
  uint hash_code;
  uint buffer_length; /* Only in the first record. */
  uint _pad1, _pad2;

#if !defined(GPU_SHADER) && defined(__cplusplus)
  bool sync(Scene *scene, ViewLayer *layer, const GPULayerAttr &attr);
#endif
};
#pragma pack(pop)
BLI_STATIC_ASSERT_ALIGN(LayerAttribute, 32)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Indirect commands structures.
 * \{ */

struct DrawCommand {
  /* TODO(fclem): Rename */
  uint vertex_len;
  uint instance_len;
  uint vertex_first;
#if defined(GPU_SHADER)
  uint base_index;
  /** \note base_index is i_first for non-indexed draw-calls. */
#  define _instance_first_array base_index
#else
  union {
    uint base_index;
    /* Use this instead of instance_first_indexed for non indexed draw calls. */
    uint instance_first_array;
  };
#endif

  uint instance_first_indexed;

  uint _pad0, _pad1, _pad2;
};
BLI_STATIC_ASSERT_ALIGN(DrawCommand, 16)

struct DispatchCommand {
  uint num_groups_x;
  uint num_groups_y;
  uint num_groups_z;
  uint _pad0;
};
BLI_STATIC_ASSERT_ALIGN(DispatchCommand, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug print
 * \{ */

/* Take the header (DrawCommand) into account. */
#define DRW_DEBUG_PRINT_MAX (8 * 1024) - 4
/** \note Cannot be more than 255 (because of column encoding). */
#define DRW_DEBUG_PRINT_WORD_WRAP_COLUMN 120u

/* The debug print buffer is laid-out as the following struct.
 * But we use plain array in shader code instead because of driver issues. */
struct DRWDebugPrintBuffer {
  DrawCommand command;
  /** Each character is encoded as 3 `uchar` with char_index, row and column position. */
  uint char_array[DRW_DEBUG_PRINT_MAX];
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugPrintBuffer, 16)

/* Use number of char as vertex count. Equivalent to `DRWDebugPrintBuffer.command.v_count`. */
#define drw_debug_print_cursor drw_debug_print_buf[0]
/* Reuse first instance as row index as we don't use instancing. Equivalent to
 * `DRWDebugPrintBuffer.command.i_first`. */
#define drw_debug_print_row_shared drw_debug_print_buf[3]
/** Offset to the first data. Equal to: `sizeof(DrawCommand) / sizeof(uint)`.
 * This is needed because we bind the whole buffer as a `uint` array. */
#define drw_debug_print_offset 8

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 * \{ */

struct DRWDebugVert {
  /* This is a weird layout, but needed to be able to use DRWDebugVert as
   * a DrawCommand and avoid alignment issues. See drw_debug_verts_buf[] definition. */
  uint pos0;
  uint pos1;
  uint pos2;
  /* Named vert_color to avoid global namespace collision with uniform color. */
  uint vert_color;
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugVert, 16)

inline DRWDebugVert debug_vert_make(uint in_pos0, uint in_pos1, uint in_pos2, uint in_vert_color)
{
  DRWDebugVert debug_vert;
  debug_vert.pos0 = in_pos0;
  debug_vert.pos1 = in_pos1;
  debug_vert.pos2 = in_pos2;
  debug_vert.vert_color = in_vert_color;
  return debug_vert;
}

/* Take the header (DrawCommand) into account. */
#define DRW_DEBUG_DRAW_VERT_MAX (64 * 8192) - 1

/* The debug draw buffer is laid-out as the following struct.
 * But we use plain array in shader code instead because of driver issues. */
struct DRWDebugDrawBuffer {
  DrawCommand command;
  DRWDebugVert verts[DRW_DEBUG_DRAW_VERT_MAX];
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugPrintBuffer, 16)

/* Equivalent to `DRWDebugDrawBuffer.command.v_count`. */
#define drw_debug_draw_v_count drw_debug_verts_buf[0].pos0
/** Offset to the first data. Equal to: `sizeof(DrawCommand) / sizeof(DRWDebugVert)`.
 * This is needed because we bind the whole buffer as a `DRWDebugVert` array. */
#define drw_debug_draw_offset 2

/** \} */
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#ifndef GPU_SHADER
#  include  GPU_shader_shared_utils.h 

typedef struct GlobalsUboStorage GlobalsUboStorage;
#endif

/* Future Plan: These globals were once shared between multiple overlay engines. But now that they
 * have been merged into one engine, there is no reasons to keep these globals out of the overlay
 * engine. */

#define UBO_FIRST_COLOR color_wire
#define UBO_LAST_COLOR color_uv_shadow

/* Used as ubo but colors can be directly referenced as well */
/* \note Also keep all color as vec4 and between #UBO_FIRST_COLOR and #UBO_LAST_COLOR. */
struct GlobalsUboStorage {
  /* UBOs data needs to be 16 byte aligned (size of vec4) */
  float4 color_wire;
  float4 color_wire_edit;
  float4 color_active;
  float4 color_select;
  float4 color_library_select;
  float4 color_library;
  float4 color_transform;
  float4 color_light;
  float4 color_speaker;
  float4 color_camera;
  float4 color_camera_path;
  float4 color_empty;
  float4 color_vertex;
  float4 color_vertex_select;
  float4 color_vertex_unreferenced;
  float4 color_vertex_missing_data;
  float4 color_edit_mesh_active;
  float4 color_edge_select;
  float4 color_edge_seam;
  float4 color_edge_sharp;
  float4 color_edge_crease;
  float4 color_edge_bweight;
  float4 color_edge_face_select;
  float4 color_edge_freestyle;
  float4 color_face;
  float4 color_face_select;
  float4 color_face_retopology;
  float4 color_face_freestyle;
  float4 color_gpencil_vertex;
  float4 color_gpencil_vertex_select;
  float4 color_normal;
  float4 color_vnormal;
  float4 color_lnormal;
  float4 color_facedot;
  float4 color_skinroot;

  float4 color_deselect;
  float4 color_outline;
  float4 color_light_no_alpha;

  float4 color_background;
  float4 color_background_gradient;
  float4 color_checker_primary;
  float4 color_checker_secondary;
  float4 color_clipping_border;
  float4 color_edit_mesh_middle;

  float4 color_handle_free;
  float4 color_handle_auto;
  float4 color_handle_vect;
  float4 color_handle_align;
  float4 color_handle_autoclamp;
  float4 color_handle_sel_free;
  float4 color_handle_sel_auto;
  float4 color_handle_sel_vect;
  float4 color_handle_sel_align;
  float4 color_handle_sel_autoclamp;
  float4 color_nurb_uline;
  float4 color_nurb_vline;
  float4 color_nurb_sel_uline;
  float4 color_nurb_sel_vline;
  float4 color_active_spline;

  float4 color_bone_pose;
  float4 color_bone_pose_active;
  float4 color_bone_pose_active_unsel;
  float4 color_bone_pose_constraint;
  float4 color_bone_pose_ik;
  float4 color_bone_pose_spline_ik;
  float4 color_bone_pose_target;
  float4 color_bone_solid;
  float4 color_bone_locked;
  float4 color_bone_active;
  float4 color_bone_active_unsel;
  float4 color_bone_select;
  float4 color_bone_ik_line;
  float4 color_bone_ik_line_no_target;
  float4 color_bone_ik_line_spline;

  float4 color_text;
  float4 color_text_hi;

  float4 color_bundle_solid;

  float4 color_mball_radius;
  float4 color_mball_radius_select;
  float4 color_mball_stiffness;
  float4 color_mball_stiffness_select;

  float4 color_current_frame;

  float4 color_grid;
  float4 color_grid_emphasis;
  float4 color_grid_axis_x;
  float4 color_grid_axis_y;
  float4 color_grid_axis_z;

  float4 color_face_back;
  float4 color_face_front;

  float4 color_uv_shadow;

  /* NOTE: Put all color before #UBO_LAST_COLOR. */
  float4 size_viewport; /* Packed as vec4. */

  /* Pack individual float at the end of the buffer to avoid alignment errors */
  float size_pixel, pixel_fac;
  float size_object_center, size_light_center, size_light_circle, size_light_circle_shadow;
  float size_vertex, size_edge, size_edge_fix, size_face_dot;
  float size_checker;
  float size_vertex_gpencil;
};
BLI_STATIC_ASSERT_ALIGN(GlobalsUboStorage, 16)

#ifdef GPU_SHADER
/* Keep compatibility_with old global scope syntax. */
/* TODO(@fclem) Mass rename and remove the camel case. */
#  define colorWire globalsBlock.color_wire
#  define colorWireEdit globalsBlock.color_wire_edit
#  define colorActive globalsBlock.color_active
#  define colorSelect globalsBlock.color_select
#  define colorLibrarySelect globalsBlock.color_library_select
#  define colorLibrary globalsBlock.color_library
#  define colorTransform globalsBlock.color_transform
#  define colorLight globalsBlock.color_light
#  define colorSpeaker globalsBlock.color_speaker
#  define colorCamera globalsBlock.color_camera
#  define colorCameraPath globalsBlock.color_camera_path
#  define colorEmpty globalsBlock.color_empty
#  define colorVertex globalsBlock.color_vertex
#  define colorVertexSelect globalsBlock.color_vertex_select
#  define colorVertexUnreferenced globalsBlock.color_vertex_unreferenced
#  define colorVertexMissingData globalsBlock.color_vertex_missing_data
#  define colorEditMeshActive globalsBlock.color_edit_mesh_active
#  define colorEdgeSelect globalsBlock.color_edge_select
#  define colorEdgeSeam globalsBlock.color_edge_seam
#  define colorEdgeSharp globalsBlock.color_edge_sharp
#  define colorEdgeCrease globalsBlock.color_edge_crease
#  define colorEdgeBWeight globalsBlock.color_edge_bweight
#  define colorEdgeFaceSelect globalsBlock.color_edge_face_select
#  define colorEdgeFreestyle globalsBlock.color_edge_freestyle
#  define colorFace globalsBlock.color_face
#  define colorFaceSelect globalsBlock.color_face_select
#  define colorFaceRetopology globalsBlock.color_face_retopology
#  define colorFaceFreestyle globalsBlock.color_face_freestyle
#  define colorGpencilVertex globalsBlock.color_gpencil_vertex
#  define colorGpencilVertexSelect globalsBlock.color_gpencil_vertex_select
#  define colorNormal globalsBlock.color_normal
#  define colorVNormal globalsBlock.color_vnormal
#  define colorLNormal globalsBlock.color_lnormal
#  define colorFaceDot globalsBlock.color_facedot
#  define colorSkinRoot globalsBlock.color_skinroot
#  define colorDeselect globalsBlock.color_deselect
#  define colorOutline globalsBlock.color_outline
#  define colorLightNoAlpha globalsBlock.color_light_no_alpha
#  define colorBackground globalsBlock.color_background
#  define colorBackgroundGradient globalsBlock.color_background_gradient
#  define colorCheckerPrimary globalsBlock.color_checker_primary
#  define colorCheckerSecondary globalsBlock.color_checker_secondary
#  define colorClippingBorder globalsBlock.color_clipping_border
#  define colorEditMeshMiddle globalsBlock.color_edit_mesh_middle
#  define colorHandleFree globalsBlock.color_handle_free
#  define colorHandleAuto globalsBlock.color_handle_auto
#  define colorHandleVect globalsBlock.color_handle_vect
#  define colorHandleAlign globalsBlock.color_handle_align
#  define colorHandleAutoclamp globalsBlock.color_handle_autoclamp
#  define colorHandleSelFree globalsBlock.color_handle_sel_free
#  define colorHandleSelAuto globalsBlock.color_handle_sel_auto
#  define colorHandleSelVect globalsBlock.color_handle_sel_vect
#  define colorHandleSelAlign globalsBlock.color_handle_sel_align
#  define colorHandleSelAutoclamp globalsBlock.color_handle_sel_autoclamp
#  define colorNurbUline globalsBlock.color_nurb_uline
#  define colorNurbVline globalsBlock.color_nurb_vline
#  define colorNurbSelUline globalsBlock.color_nurb_sel_uline
#  define colorNurbSelVline globalsBlock.color_nurb_sel_vline
#  define colorActiveSpline globalsBlock.color_active_spline
#  define colorBonePose globalsBlock.color_bone_pose
#  define colorBonePoseActive globalsBlock.color_bone_pose_active
#  define colorBonePoseActiveUnsel globalsBlock.color_bone_pose_active_unsel
#  define colorBonePoseConstraint globalsBlock.color_bone_pose_constraint
#  define colorBonePoseIK globalsBlock.color_bone_pose_ik
#  define colorBonePoseSplineIK globalsBlock.color_bone_pose_spline_ik
#  define colorBonePoseTarget globalsBlock.color_bone_pose_target
#  define colorBoneSolid globalsBlock.color_bone_solid
#  define colorBoneLocked globalsBlock.color_bone_locked
#  define colorBoneActive globalsBlock.color_bone_active
#  define colorBoneActiveUnsel globalsBlock.color_bone_active_unsel
#  define colorBoneSelect globalsBlock.color_bone_select
#  define colorBoneIKLine globalsBlock.color_bone_ik_line
#  define colorBoneIKLineNoTarget globalsBlock.color_bone_ik_line_no_target
#  define colorBoneIKLineSpline globalsBlock.color_bone_ik_line_spline
#  define colorText globalsBlock.color_text
#  define colorTextHi globalsBlock.color_text_hi
#  define colorBundleSolid globalsBlock.color_bundle_solid
#  define colorMballRadius globalsBlock.color_mball_radius
#  define colorMballRadiusSelect globalsBlock.color_mball_radius_select
#  define colorMballStiffness globalsBlock.color_mball_stiffness
#  define colorMballStiffnessSelect globalsBlock.color_mball_stiffness_select
#  define colorCurrentFrame globalsBlock.color_current_frame
#  define colorGrid globalsBlock.color_grid
#  define colorGridEmphasis globalsBlock.color_grid_emphasis
#  define colorGridAxisX globalsBlock.color_grid_axis_x
#  define colorGridAxisY globalsBlock.color_grid_axis_y
#  define colorGridAxisZ globalsBlock.color_grid_axis_z
#  define colorFaceBack globalsBlock.color_face_back
#  define colorFaceFront globalsBlock.color_face_front
#  define colorUVShadow globalsBlock.color_uv_shadow
#  define sizeViewport globalsBlock.size_viewport.xy
#  define sizeViewportInv globalsBlock.size_viewport.zw
#  define sizePixel globalsBlock.size_pixel
#  define pixelFac globalsBlock.pixel_fac
#  define sizeObjectCenter globalsBlock.size_object_center
#  define sizeLightCenter globalsBlock.size_light_center
#  define sizeLightCircle globalsBlock.size_light_circle
#  define sizeLightCircleShadow globalsBlock.size_light_circle_shadow
#  define sizeVertex globalsBlock.size_vertex
#  define sizeEdge globalsBlock.size_edge
#  define sizeEdgeFix globalsBlock.size_edge_fix
#  define sizeFaceDot globalsBlock.size_face_dot
#  define sizeChecker globalsBlock.size_checker
#  define sizeVertexGpencil globalsBlock.size_vertex_gpencil
#endif

/* See: 'draw_cache_impl.h' for matching includes. */
#define VERT_GPENCIL_BEZT_HANDLE (1u << 30)
/* data[0] (1st byte flags) */
#define FACE_ACTIVE (1u << 0)
#define FACE_SELECTED (1u << 1)
#define FACE_FREESTYLE (1u << 2)
#define VERT_UV_SELECT (1u << 3)
#define VERT_UV_PINNED (1u << 4)
#define EDGE_UV_SELECT (1u << 5)
#define FACE_UV_ACTIVE (1u << 6)
#define FACE_UV_SELECT (1u << 7)
/* data[1] (2st byte flags) */
#define VERT_ACTIVE (1u << 0)
#define VERT_SELECTED (1u << 1)
#define VERT_SELECTED_BEZT_HANDLE (1u << 2)
#define EDGE_ACTIVE (1u << 3)
#define EDGE_SELECTED (1u << 4)
#define EDGE_SEAM (1u << 5)
#define EDGE_SHARP (1u << 6)
#define EDGE_FREESTYLE (1u << 7)

#define COMMON_GLOBALS_LIB

/* Pass Resources. */
layout(binding = 0) uniform sampler2D weightTex;
layout(binding = 0, std140) uniform drw_view_ { ViewMatrices _drw_view_[DRW_VIEW_LEN]; };
layout(binding = 7, std140) uniform globalsBlock { GlobalsUboStorage _globalsBlock; };
#define drw_view_ (_drw_view_)
#define globalsBlock (_globalsBlock)

/* Batch Resources. */
layout(binding = 8, std140) uniform drw_matrices { ObjectMatrices _drw_matrices[DRW_RESOURCE_CHUNK_LEN]; };
#define drw_matrices (_drw_matrices)

/* Push Constants. */
uniform float normalSize;
uniform bool doMultiframe;
uniform bool doStrokeEndpoints;
uniform bool hideSelect;
uniform bool doWeightColor;
uniform float gpEditOpacity;
uniform vec4 gpEditColor;


/* Inputs. */
layout(location = 0) in vec3 pos;
layout(location = 1) in int ma;
layout(location = 2) in uint vflag;
layout(location = 3) in float weight;

/* Interfaces. */
out overlay_edit_smooth_color_iface{
  smooth vec4 finalColor;
};


/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef COMMON_VIEW_LIB_GLSL
#define COMMON_VIEW_LIB_GLSL

#ifndef DRW_RESOURCE_CHUNK_LEN
#  error Missing draw_view additional create info on shader create info
#endif

/* Not supported anymore. TODO(fclem): Add back support. */
// #define IS_DEBUG_MOUSE_FRAGMENT (ivec2(gl_FragCoord) == drw_view.mouse_pixel)
#define IS_FIRST_INVOCATION (gl_GlobalInvocationID == uvec3(0))

#define cameraForward ViewMatrixInverse[2].xyz
#define cameraPos ViewMatrixInverse[3].xyz
vec3 cameraVec(vec3 P)
{
  return ((ProjectionMatrix[3][3] == 0.0) ? normalize(cameraPos - P) : cameraForward);
}
#define viewCameraVec(vP) ((ProjectionMatrix[3][3] == 0.0) ? normalize(-vP) : vec3(0.0, 0.0, 1.0))

#ifdef COMMON_GLOBALS_LIB
/* TODO move to overlay engine. */
float mul_project_m4_v3_zfac(vec3 co)
{
  vec3 vP = (ViewMatrix * vec4(co, 1.0)).xyz;
  return pixelFac * ((ProjectionMatrix[0][3] * vP.x) + (ProjectionMatrix[1][3] * vP.y) +
                     (ProjectionMatrix[2][3] * vP.z) + ProjectionMatrix[3][3]);
}
#endif

/* Not the right place but need to be common to all overlay's.
 * TODO: Split to an overlay lib. */
mat4 extract_matrix_packed_data(mat4 mat, out vec4 dataA, out vec4 dataB)
{
  const float div = 1.0 / 255.0;
  int a = int(mat[0][3]);
  int b = int(mat[1][3]);
  int c = int(mat[2][3]);
  int d = int(mat[3][3]);
  dataA = vec4(a & 0xFF, a >> 8, b & 0xFF, b >> 8) * div;
  dataB = vec4(c & 0xFF, c >> 8, d & 0xFF, d >> 8) * div;
  mat[0][3] = mat[1][3] = mat[2][3] = 0.0;
  mat[3][3] = 1.0;
  return mat;
}

/* Same here, Not the right place but need to be common to all overlay's.
 * TODO: Split to an overlay lib. */
/* edge_start and edge_pos needs to be in the range [0..sizeViewport]. */
vec4 pack_line_data(vec2 frag_co, vec2 edge_start, vec2 edge_pos)
{
  vec2 edge = edge_start - edge_pos;
  float len = length(edge);
  if (len > 0.0) {
    edge /= len;
    vec2 perp = vec2(-edge.y, edge.x);
    float dist = dot(perp, frag_co - edge_start);
    /* Add 0.1 to differentiate with cleared pixels. */
    return vec4(perp * 0.5 + 0.5, dist * 0.25 + 0.5 + 0.1, 1.0);
  }
  else {
    /* Default line if the origin is perfectly aligned with a pixel. */
    return vec4(1.0, 0.0, 0.5 + 0.1, 1.0);
  }
}

/* Temporary until we fully make the switch. */
#ifndef USE_GPU_SHADER_CREATE_INFO
uniform int drw_resourceChunk;
#endif /* USE_GPU_SHADER_CREATE_INFO */

#ifdef GPU_VERTEX_SHADER

/* Temporary until we fully make the switch. */
#  ifndef USE_GPU_SHADER_CREATE_INFO

/* clang-format off */
#    if defined(IN_PLACE_INSTANCES) || defined(INSTANCED_ATTR) || defined(DRW_LEGACY_MODEL_MATRIX) || defined(GPU_DEPRECATED_AMD_DRIVER)
/* clang-format on */
/* When drawing instances of an object at the same position. */
#      define instanceId 0
#    else
#      define instanceId gl_InstanceID
#    endif

#    if defined(UNIFORM_RESOURCE_ID)
/* This is in the case we want to do a special instance drawcall for one object but still want to
 * have the right resourceId and all the correct ubo datas. */
uniform int drw_ResourceID;
#      define resource_id drw_ResourceID
#    else
#      define resource_id (gpu_BaseInstance + instanceId)
#    endif

/* Use this to declare and pass the value if
 * the fragment shader uses the resource_id. */
#    if defined(EEVEE_GENERATED_INTERFACE)
#      define RESOURCE_ID_VARYING
#      define PASS_RESOURCE_ID resourceIDFrag = resource_id;
#    elif defined(USE_GEOMETRY_SHADER)
#      define RESOURCE_ID_VARYING flat out int resourceIDGeom;
#      define PASS_RESOURCE_ID resourceIDGeom = resource_id;
#    else
#      define RESOURCE_ID_VARYING flat out int resourceIDFrag;
#      define PASS_RESOURCE_ID resourceIDFrag = resource_id;
#    endif

#  endif /* USE_GPU_SHADER_CREATE_INFO */

#endif /* GPU_VERTEX_SHADER */

/* Temporary until we fully make the switch. */
#ifdef USE_GPU_SHADER_CREATE_INFO
/* TODO(fclem): Rename PASS_RESOURCE_ID to DRW_RESOURCE_ID_VARYING_SET */
#  if defined(UNIFORM_RESOURCE_ID)
#    define resource_id drw_ResourceID
#    define PASS_RESOURCE_ID

#  elif defined(GPU_VERTEX_SHADER)
#    if defined(UNIFORM_RESOURCE_ID_NEW)
#      define resource_id (drw_ResourceID >> DRW_VIEW_SHIFT)
#    else
#      define resource_id gpu_InstanceIndex
#    endif
#    define PASS_RESOURCE_ID drw_ResourceID_iface.resource_index = resource_id;

#  elif defined(GPU_GEOMETRY_SHADER)
#    define resource_id drw_ResourceID_iface_in[0].resource_index
#    define PASS_RESOURCE_ID drw_ResourceID_iface_out.resource_index = resource_id;

#  elif defined(GPU_FRAGMENT_SHADER)
#    define resource_id drw_ResourceID_iface.resource_index
#  endif

/* TODO(fclem): Remove. */
#  define RESOURCE_ID_VARYING

#else
/* If used in a fragment / geometry shader, we pass
 * resource_id as varying. */
#  ifdef GPU_GEOMETRY_SHADER
/* TODO(fclem): Remove. This is getting ridiculous. */
#    if !defined(EEVEE_GENERATED_INTERFACE)
#      define RESOURCE_ID_VARYING \
        flat out int resourceIDFrag; \
        flat in int resourceIDGeom[];
#    else
#      define RESOURCE_ID_VARYING
#    endif

#    define resource_id resourceIDGeom
#    define PASS_RESOURCE_ID resourceIDFrag = resource_id[0];
#  endif

#  if defined(GPU_FRAGMENT_SHADER)
#    if !defined(EEVEE_GENERATED_INTERFACE)
flat in int resourceIDFrag;
#    endif
#    define resource_id resourceIDFrag
#  endif
#endif

/* Breaking this across multiple lines causes issues for some older GLSL compilers. */
/* clang-format off */
#if !defined(GPU_INTEL) && !defined(GPU_DEPRECATED_AMD_DRIVER) && (!defined(OS_MAC) || defined(GPU_METAL)) && !defined(INSTANCED_ATTR) && !defined(DRW_LEGACY_MODEL_MATRIX)
/* clang-format on */

/* Temporary until we fully make the switch. */
#  ifndef DRW_SHADER_SHARED_H

struct ObjectMatrices {
  mat4 model;
  mat4 model_inverse;
};
#  endif /* DRW_SHADER_SHARED_H */

#  ifndef USE_GPU_SHADER_CREATE_INFO
layout(std140) uniform modelBlock
{
  ObjectMatrices drw_matrices[DRW_RESOURCE_CHUNK_LEN];
};

#    define ModelMatrix (drw_matrices[resource_id].model)
#    define ModelMatrixInverse (drw_matrices[resource_id].model_inverse)
#  endif /* USE_GPU_SHADER_CREATE_INFO */

#else /* GPU_INTEL */

/* Temporary until we fully make the switch. */
#  ifndef USE_GPU_SHADER_CREATE_INFO
/* Intel GPU seems to suffer performance impact when the model matrix is in UBO storage.
 * So for now we just force using the legacy path. */
/* Note that this is also a workaround of a problem on OSX (AMD or NVIDIA)
 * and older amd driver on windows. */
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;
#  endif /* USE_GPU_SHADER_CREATE_INFO */

#endif

/* Temporary until we fully make the switch. */
#ifndef USE_GPU_SHADER_CREATE_INFO
#  define resource_handle (drw_resourceChunk * DRW_RESOURCE_CHUNK_LEN + resource_id)
#endif

/** Transform shortcuts. */
/* Rule of thumb: Try to reuse world positions and normals because converting through viewspace
 * will always be decomposed in at least 2 matrix operation. */

/**
 * Some clarification:
 * Usually Normal matrix is transpose(inverse(ViewMatrix * ModelMatrix))
 *
 * But since it is slow to multiply matrices we decompose it. Decomposing
 * inversion and transposition both invert the product order leaving us with
 * the same original order:
 * transpose(ViewMatrixInverse) * transpose(ModelMatrixInverse)
 *
 * Knowing that the view matrix is orthogonal, the transpose is also the inverse.
 * NOTE: This is only valid because we are only using the mat3 of the ViewMatrixInverse.
 * ViewMatrix * transpose(ModelMatrixInverse)
 */
#define NormalMatrix transpose(mat3(ModelMatrixInverse))
#define NormalMatrixInverse transpose(mat3(ModelMatrix))

#define normal_object_to_view(n) (mat3(ViewMatrix) * (NormalMatrix * n))
#define normal_object_to_world(n) (NormalMatrix * n)
#define normal_world_to_object(n) (NormalMatrixInverse * n)
#define normal_world_to_view(n) (mat3(ViewMatrix) * n)
#define normal_view_to_world(n) (mat3(ViewMatrixInverse) * n)

#define point_object_to_ndc(p) \
  (ProjectionMatrix * (ViewMatrix * vec4((ModelMatrix * vec4(p, 1.0)).xyz, 1.0)))
#define point_object_to_view(p) ((ViewMatrix * vec4((ModelMatrix * vec4(p, 1.0)).xyz, 1.0)).xyz)
#define point_object_to_world(p) ((ModelMatrix * vec4(p, 1.0)).xyz)
#define point_view_to_object(p) ((ModelMatrixInverse * (ViewMatrixInverse * vec4(p, 1.0))).xyz)
#define point_world_to_object(p) ((ModelMatrixInverse * vec4(p, 1.0)).xyz)

vec4 point_view_to_ndc(vec3 p)
{
  return ProjectionMatrix * vec4(p, 1.0);
}

vec3 point_view_to_world(vec3 p)
{
  return (ViewMatrixInverse * vec4(p, 1.0)).xyz;
}

vec4 point_world_to_ndc(vec3 p)
{
  return ProjectionMatrix * (ViewMatrix * vec4(p, 1.0));
}

vec3 point_world_to_view(vec3 p)
{
  return (ViewMatrix * vec4(p, 1.0)).xyz;
}

/* Viewspace Z is used to adjust for perspective projection.
 * Homogenous W is used to convert from NDC to homogenous space.
 * Offset is in viewspace, so positive values are closer to the camera. */
float get_homogenous_z_offset(float vs_z, float hs_w, float vs_offset)
{
  if (vs_offset == 0.0) {
    /* Don't calculate homogenous offset if viewspace offset is zero. */
    return 0.0;
  }
  else if (ProjectionMatrix[3][3] == 0.0) {
    /* Clamp offset to half of Z to avoid floating point precision errors. */
    vs_offset = min(vs_offset, vs_z * -0.5);
    /* From "Projection Matrix Tricks" by Eric Lengyel:
     * http://www.terathon.com/gdc07_lengyel.pdf (p. 24 Depth Modification) */
    return ProjectionMatrix[3][2] * (vs_offset / (vs_z * (vs_z + vs_offset))) * hs_w;
  }
  else {
    return ProjectionMatrix[2][2] * vs_offset * hs_w;
  }
}

/* Due to some shader compiler bug, we somewhat need to access gl_VertexID
 * to make vertex shaders work. even if it's actually dead code. */
#if defined(GPU_INTEL) && defined(GPU_OPENGL)
#  define GPU_INTEL_VERTEX_SHADER_WORKAROUND gl_Position.x = float(gl_VertexID);
#else
#  define GPU_INTEL_VERTEX_SHADER_WORKAROUND
#endif

#define DRW_BASE_SELECTED (1 << 1)
#define DRW_BASE_FROM_DUPLI (1 << 2)
#define DRW_BASE_FROM_SET (1 << 3)
#define DRW_BASE_ACTIVE (1 << 4)

/* ---- Opengl Depth conversion ---- */

float linear_depth(bool is_persp, float z, float zf, float zn)
{
  if (is_persp) {
    return (zn * zf) / (z * (zn - zf) + zf);
  }
  else {
    return (z * 2.0 - 1.0) * zf;
  }
}

float buffer_depth(bool is_persp, float z, float zf, float zn)
{
  if (is_persp) {
    return (zf * (zn - z)) / (z * (zn - zf));
  }
  else {
    return (z / (zf * 2.0)) + 0.5;
  }
}

float get_view_z_from_depth(float depth)
{
  float d = 2.0 * depth - 1.0;
  if (ProjectionMatrix[3][3] == 0.0) {
    d = -ProjectionMatrix[3][2] / (d + ProjectionMatrix[2][2]);
  }
  else {
    d = (d - ProjectionMatrix[3][2]) / ProjectionMatrix[2][2];
  }
  return d;
}

float get_depth_from_view_z(float z)
{
  float d;
  if (ProjectionMatrix[3][3] == 0.0) {
    d = (-ProjectionMatrix[3][2] / z) - ProjectionMatrix[2][2];
  }
  else {
    d = ProjectionMatrix[2][2] * z + ProjectionMatrix[3][2];
  }
  return d * 0.5 + 0.5;
}

vec2 get_uvs_from_view(vec3 view)
{
  vec4 ndc = ProjectionMatrix * vec4(view, 1.0);
  return (ndc.xy / ndc.w) * 0.5 + 0.5;
}

vec3 get_view_space_from_depth(vec2 uvcoords, float depth)
{
  vec3 ndc = vec3(uvcoords, depth) * 2.0 - 1.0;
  vec4 p = ProjectionMatrixInverse * vec4(ndc, 1.0);
  return p.xyz / p.w;
}

vec3 get_world_space_from_depth(vec2 uvcoords, float depth)
{
  return (ViewMatrixInverse * vec4(get_view_space_from_depth(uvcoords, depth), 1.0)).xyz;
}

vec3 get_view_vector_from_screen_uv(vec2 uvcoords)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    vec2 ndc = vec2(uvcoords * 2.0 - 1.0);
    /* This is the manual inversion of the ProjectionMatrix. */
    vec3 vV = vec3((-ndc - ProjectionMatrix[2].xy) /
                       vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]),
                   -ProjectionMatrix[2][2] - ProjectionMatrix[3][2]);
    return normalize(vV);
  }
  /* Orthographic case. */
  return vec3(0.0, 0.0, 1.0);
}

#endif /* COMMON_VIEW_LIB_GLSL */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#if defined(GPU_VERTEX_SHADER) || defined(GPU_GEOMETRY_SHADER)

void view_clipping_distances(vec3 wpos)
{
#  ifdef USE_WORLD_CLIP_PLANES
  vec4 pos_4d = vec4(wpos, 1.0);
  gl_ClipDistance[0] = dot(drw_clipping_[0], pos_4d);
  gl_ClipDistance[1] = dot(drw_clipping_[1], pos_4d);
  gl_ClipDistance[2] = dot(drw_clipping_[2], pos_4d);
  gl_ClipDistance[3] = dot(drw_clipping_[3], pos_4d);
  gl_ClipDistance[4] = dot(drw_clipping_[4], pos_4d);
  gl_ClipDistance[5] = dot(drw_clipping_[5], pos_4d);
#  endif
}

void view_clipping_distances_bypass()
{
#  ifdef USE_WORLD_CLIP_PLANES
  gl_ClipDistance[0] = 1.0;
  gl_ClipDistance[1] = 1.0;
  gl_ClipDistance[2] = 1.0;
  gl_ClipDistance[3] = 1.0;
  gl_ClipDistance[4] = 1.0;
  gl_ClipDistance[5] = 1.0;
#  endif
}

/* Kept as define for compiler compatibility. */
#  ifdef USE_WORLD_CLIP_PLANES
#    define view_clipping_distances_set(c) \
      gl_ClipDistance[0] = (c).gl_ClipDistance[0]; \
      gl_ClipDistance[1] = (c).gl_ClipDistance[1]; \
      gl_ClipDistance[2] = (c).gl_ClipDistance[2]; \
      gl_ClipDistance[3] = (c).gl_ClipDistance[3]; \
      gl_ClipDistance[4] = (c).gl_ClipDistance[4]; \
      gl_ClipDistance[5] = (c).gl_ClipDistance[5];

#  else
#    define view_clipping_distances_set(c)
#  endif

#endif

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void discard_vert()
{
  /* We set the vertex at the camera origin to generate 0 fragments. */
  gl_Position = vec4(0.0, 0.0, -3e36, 0.0);
}

#ifdef USE_POINTS
#  define gp_colorUnselect colorGpencilVertex
#  define gp_colorSelect colorGpencilVertexSelect
#else
#  define gp_colorUnselect gpEditColor
#  define gp_colorSelect (hideSelect ? gp_colorUnselect : colorGpencilVertexSelect)
#endif

vec3 weight_to_rgb(float t)
{
  if (t < 0.0) {
    /* No weight */
    return gp_colorUnselect.rgb;
  }
  else if (t > 1.0) {
    /* Error color */
    return vec3(1.0, 0.0, 1.0);
  }
  else {
    return texture(weightTex, vec2(t, 0.5)).rgb;
  }
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  bool is_multiframe = (vflag & GP_EDIT_MULTIFRAME) != 0u;
  bool is_stroke_sel = (vflag & GP_EDIT_STROKE_SELECTED) != 0u;
  bool is_point_sel = (vflag & GP_EDIT_POINT_SELECTED) != 0u;
  bool is_point_dimmed = (vflag & GP_EDIT_POINT_DIMMED) != 0u;

  if (doWeightColor) {
    finalColor.rgb = weight_to_rgb(weight);
    finalColor.a = gpEditOpacity;
  }
  else {
    finalColor = (is_point_sel) ? gp_colorSelect : gp_colorUnselect;
    finalColor.a *= gpEditOpacity;
  }

#ifdef USE_POINTS
  gl_PointSize = sizeVertexGpencil * 2.0;

  if (is_point_dimmed) {
    finalColor.rgb = clamp(gp_colorUnselect.rgb + vec3(0.3), 0.0, 1.0);
  }

  if (doStrokeEndpoints && !doWeightColor) {
    bool is_stroke_start = (vflag & GP_EDIT_STROKE_START) != 0u;
    bool is_stroke_end = (vflag & GP_EDIT_STROKE_END) != 0u;

    if (is_stroke_start) {
      gl_PointSize *= 2.0;
      finalColor.rgb = vec3(0.0, 1.0, 0.0);
    }
    else if (is_stroke_end) {
      gl_PointSize *= 1.5;
      finalColor.rgb = vec3(1.0, 0.0, 0.0);
    }
  }

  if ((!is_stroke_sel && !doWeightColor) || (!doMultiframe && is_multiframe)) {
    discard_vert();
  }
#endif

  /* Discard unwanted padding vertices. */
  if (ma == -1 || (is_multiframe && !doMultiframe)) {
    discard_vert();
  }

  view_clipping_distances(world_pos);
}
