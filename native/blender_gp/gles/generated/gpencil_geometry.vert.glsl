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
#define GP_LIGHT 
#define DRW_GPENCIL_INFO 
#define gpThicknessIsScreenSpace (gpThicknessWorldScale < 0.0)
#define ModelMatrix (drw_matrices[resource_id].model)
#define ModelMatrixInverse (drw_matrices[resource_id].model_inverse)
#define drw_view drw_view_[drw_view_id]
#define OBINFO_LIB 
#define OrcoTexCoFactors (drw_infos[resource_id].orco_mul_bias)
#define ObjectInfo (drw_infos[resource_id].infos)
#define ObjectColor (drw_infos[resource_id].ob_color)
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

#define GPENCIL_MATERIAL_BUFFER_LEN 256

#define GPENCIL_LIGHT_BUFFER_LEN 128

/* High bits are used to pass material ID to fragment shader. */
#define GPENCIl_MATID_SHIFT 16u
/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  include  GPU_shader_shared_utils.h 

#  ifndef __cplusplus
typedef struct gpMaterial gpMaterial;
typedef struct gpLight gpLight;
typedef enum gpMaterialFlag gpMaterialFlag;
#    ifdef GP_LIGHT
typedef enum gpLightType gpLightType;
#    endif
#  endif
#endif

#define  gpMaterialFlag  uint
const uint 
  GP_STROKE_ALIGNMENT_STROKE = 1u,
  GP_STROKE_ALIGNMENT_OBJECT = 2u,
  GP_STROKE_ALIGNMENT_FIXED = 3u,
  GP_STROKE_ALIGNMENT = 0x3u,
  GP_STROKE_OVERLAP = (1u << 2u),
  GP_STROKE_TEXTURE_USE = (1u << 3u),
  GP_STROKE_TEXTURE_STENCIL = (1u << 4u),
  GP_STROKE_TEXTURE_PREMUL = (1u << 5u),
  GP_STROKE_DOTS = (1u << 6u),
  GP_STROKE_HOLDOUT = (1u << 7u),
  GP_FILL_HOLDOUT = (1u << 8u),
  GP_FILL_TEXTURE_USE = (1u << 10u),
  GP_FILL_TEXTURE_PREMUL = (1u << 11u),
  GP_FILL_TEXTURE_CLIP = (1u << 12u),
  GP_FILL_GRADIENT_USE = (1u << 13u),
  GP_FILL_GRADIENT_RADIAL = (1u << 14u),
  GP_FILL_FLAGS = (GP_FILL_TEXTURE_USE | GP_FILL_TEXTURE_PREMUL | GP_FILL_TEXTURE_CLIP |
                   GP_FILL_GRADIENT_USE | GP_FILL_GRADIENT_RADIAL | GP_FILL_HOLDOUT);

#define  gpLightType  uint
const uint 
  GP_LIGHT_TYPE_POINT = 0u,
  GP_LIGHT_TYPE_SPOT = 1u,
  GP_LIGHT_TYPE_SUN = 2u,
  GP_LIGHT_TYPE_AMBIENT = 3u;

#define GP_IS_STROKE_VERTEX_BIT (1 << 30)
#define GP_VERTEX_ID_SHIFT 2

/* Avoid compiler funkiness with enum types not being strongly typed in C. */
#ifndef GPU_SHADER
#  define gpMaterialFlag uint
#  define gpLightType uint
#endif

struct gpMaterial {
  float4 stroke_color;
  float4 fill_color;
  float4 fill_mix_color;
  float4 fill_uv_rot_scale;
#ifndef GPU_SHADER
  float2 fill_uv_offset;
  float2 alignment_rot;
  float stroke_texture_mix;
  float stroke_u_scale;
  float fill_texture_mix;
  gpMaterialFlag flag;
#else
  /* Some drivers are completely messing the alignment or the fetches here.
   * We are forced to pack these into vec4 otherwise we only get 0.0 as value. */
  /* NOTE(@fclem): This was the case on MacOS OpenGL implementation.
   * This might be fixed in newer APIs. */
  float4 packed1;
  float4 packed2;
#  define _fill_uv_offset packed1.xy
#  define _alignment_rot packed1.zw
#  define _stroke_texture_mix packed2.x
#  define _stroke_u_scale packed2.y
#  define _fill_texture_mix packed2.z
  /** NOTE(@fclem): Needs floatBitsToUint(). */
#  define _flag packed2.w
#endif
};
BLI_STATIC_ASSERT_ALIGN(gpMaterial, 16)

#ifdef GP_LIGHT
struct gpLight {
#  ifndef GPU_SHADER
  float3 color;
  gpLightType type;
  float3 right;
  float spot_size;
  float3 up;
  float spot_blend;
  float3 forward;
  float _pad0;
  float3 position;
  float _pad1;
#  else
  /* Some drivers are completely messing the alignment or the fetches here.
   * We are forced to pack these into vec4 otherwise we only get 0.0 as value. */
  /* NOTE(@fclem): This was the case on MacOS OpenGL implementation.
   * This might be fixed in newer APIs. */
  float4 packed0;
  float4 packed1;
  float4 packed2;
  float4 packed3;
  float4 packed4;
#    define _color packed0.xyz
#    define _type packed0.w
#    define _right packed1.xyz
#    define _spot_size packed1.w
#    define _up packed2.xyz
#    define _spot_blend packed2.w
#    define _forward packed3.xyz
#    define _position packed4.xyz
#  endif
};
BLI_STATIC_ASSERT_ALIGN(gpLight, 16)
#endif

#ifndef GPU_SHADER
#  undef gpMaterialFlag
#  undef gpLightType
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

/* Pass Resources. */
layout(binding = 2) uniform sampler2D gpFillTexture;
layout(binding = 3) uniform sampler2D gpStrokeTexture;
layout(binding = 4) uniform sampler2D gpSceneDepthTexture;
layout(binding = 5) uniform sampler2D gpMaskTexture;
layout(binding = 0) uniform samplerBuffer gp_pos_tx;
layout(binding = 1) uniform samplerBuffer gp_col_tx;
layout(binding = 0, std140) uniform drw_view_ { ViewMatrices _drw_view_[DRW_VIEW_LEN]; };
#define drw_view_ (_drw_view_)

/* Batch Resources. */
layout(binding = 4, std140) uniform materials { gpMaterial _materials[GPENCIL_MATERIAL_BUFFER_LEN]; };
layout(binding = 3, std140) uniform lights { gpLight _lights[GPENCIL_LIGHT_BUFFER_LEN]; };
layout(binding = 8, std140) uniform drw_matrices { ObjectMatrices _drw_matrices[DRW_RESOURCE_CHUNK_LEN]; };
layout(binding = 1, std140) uniform drw_infos { ObjectInfos _drw_infos[DRW_RESOURCE_CHUNK_LEN]; };
#define materials (_materials)
#define lights (_lights)
#define drw_matrices (_drw_matrices)
#define drw_infos (_drw_infos)

/* Push Constants. */
uniform vec2 viewportSize;
uniform vec3 gpNormal;
uniform bool gpStrokeOrder3d;
uniform int gpMaterialOffset;
uniform float gpVertexColorOpacity;
uniform vec4 gpLayerTint;
uniform float gpLayerOpacity;
uniform float gpStrokeIndexOffset;
uniform float gpThicknessScale;
uniform float gpThicknessWorldScale;
uniform float gpThicknessOffset;


/* Inputs. */

/* Interfaces. */
out gpencil_geometry_iface{
  smooth vec4 color_mul;
  smooth vec4 color_add;
  smooth vec3 pos;
  smooth vec2 uv;
  smooth vec2 thickness;
  smooth float hardness;
  flat vec2 aspect;
  flat vec4 sspos;
  flat uint mat_flag;
  flat float depth;
}
gp_interp;


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

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef COMMON_MATH_LIB_GLSL
#define COMMON_MATH_LIB_GLSL

/* ---------------------------------------------------------------------- */
/** \name Common Math Utilities
 * \{ */

#define M_PI 3.14159265358979323846      /* pi */
#define M_2PI 6.28318530717958647692     /* 2*pi */
#define M_PI_2 1.57079632679489661923    /* pi/2 */
#define M_PI_4 0.78539816339744830962    /* pi/4 */
#define M_1_PI 0.318309886183790671538   /* 1/pi */
#define M_1_2PI 0.159154943091895335768  /* 1/(2*pi) */
#define M_1_PI2 0.101321183642337771443  /* 1/(pi^2) */
#define M_SQRT2 1.41421356237309504880   /* sqrt(2) */
#define M_SQRT1_2 0.70710678118654752440 /* 1/sqrt(2) */
#ifndef FLT_MAX
#  define FLT_MAX 3.402823e+38
#  define FLT_MIN 1.175494e-38
#  define FLT_EPSILON 1.192092896e-07F
#endif

vec3 mul(mat3 m, vec3 v)
{
  return m * v;
}
mat3 mul(mat3 m1, mat3 m2)
{
  return m1 * m2;
}
/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_MATRIX_LIB_GLSL
vec3 transform_direction(mat4 m, vec3 v)
{
  return mat3(m) * v;
}
vec3 transform_point(mat4 m, vec3 v)
{
  return (m * vec4(v, 1.0)).xyz;
}
vec3 project_point(mat4 m, vec3 v)
{
  vec4 tmp = m * vec4(v, 1.0);
  return tmp.xyz / tmp.w;
}
#endif

mat2 rot2_from_angle(float a)
{
  float c = cos(a);
  float s = sin(a);
  return mat2(c, -s, s, c);
}

#define min3(a, b, c) min(a, min(b, c))
#define min4(a, b, c, d) min(a, min3(b, c, d))
#define min5(a, b, c, d, e) min(a, min4(b, c, d, e))
#define min6(a, b, c, d, e, f) min(a, min5(b, c, d, e, f))
#define min7(a, b, c, d, e, f, g) min(a, min6(b, c, d, e, f, g))
#define min8(a, b, c, d, e, f, g, h) min(a, min7(b, c, d, e, f, g, h))
#define min9(a, b, c, d, e, f, g, h, i) min(a, min8(b, c, d, e, f, g, h, i))

#define max3(a, b, c) max(a, max(b, c))
#define max4(a, b, c, d) max(a, max3(b, c, d))
#define max5(a, b, c, d, e) max(a, max4(b, c, d, e))
#define max6(a, b, c, d, e, f) max(a, max5(b, c, d, e, f))
#define max7(a, b, c, d, e, f, g) max(a, max6(b, c, d, e, f, g))
#define max8(a, b, c, d, e, f, g, h) max(a, max7(b, c, d, e, f, g, h))
#define max9(a, b, c, d, e, f, g, h, i) max(a, max8(b, c, d, e, f, g, h, i))

#define avg3(a, b, c) (a + b + c) * (1.0 / 3.0)
#define avg4(a, b, c, d) (a + b + c + d) * (1.0 / 4.0)
#define avg5(a, b, c, d, e) (a + b + c + d + e) * (1.0 / 5.0)
#define avg6(a, b, c, d, e, f) (a + b + c + d + e + f) * (1.0 / 6.0)
#define avg7(a, b, c, d, e, f, g) (a + b + c + d + e + f + g) * (1.0 / 7.0)
#define avg8(a, b, c, d, e, f, g, h) (a + b + c + d + e + f + g + h) * (1.0 / 8.0)
#define avg9(a, b, c, d, e, f, g, h, i) (a + b + c + d + e + f + g + h + i) * (1.0 / 9.0)

/* clang-format off */
#define min_v2(v) min((v).x, (v).y)
#define min_v3(v) min((v).x, min((v).y, (v).z))
#define min_v4(v) min(min((v).x, (v).y), min((v).z, (v).w))
#define max_v2(v) max((v).x, (v).y)
#define max_v3(v) max((v).x, max((v).y, (v).z))
#define max_v4(v) max(max((v).x, (v).y), max((v).z, (v).w))

float sum(vec2 v) { return dot(vec2(1.0), v); }
float sum(vec3 v) { return dot(vec3(1.0), v); }
float sum(vec4 v) { return dot(vec4(1.0), v); }

float avg(vec2 v) { return dot(vec2(1.0 / 2.0), v); }
float avg(vec3 v) { return dot(vec3(1.0 / 3.0), v); }
float avg(vec4 v) { return dot(vec4(1.0 / 4.0), v); }

float safe_rcp(float a) { return (a != 0.0) ? (1.0 / a) : 0.0; }
vec2 safe_rcp(vec2 a) { return select(vec2(0.0), (1.0 / a), notEqual(a, vec2(0.0))); }
vec3 safe_rcp(vec3 a) { return select(vec3(0.0), (1.0 / a), notEqual(a, vec3(0.0))); }
vec4 safe_rcp(vec4 a) { return select(vec4(0.0), (1.0 / a), notEqual(a, vec4(0.0))); }

float safe_sqrt(float a) { return sqrt(max(a, 0.0)); }

float safe_acos(float a) { return acos(clamp(a, -1.0, 1.0)); }

float sqr(float a) { return a * a; }
vec2 sqr(vec2 a) { return a * a; }
vec3 sqr(vec3 a) { return a * a; }
vec4 sqr(vec4 a) { return a * a; }

/* Use manual powers for fixed powers. pow() can have unpredictable results on some implementations.
 * (see #87369, #87541) */
float pow6(float x) { return sqr(sqr(x) * x); }
float pow8(float x) { return sqr(sqr(sqr(x))); }

float len_squared(vec3 a) { return dot(a, a); }
float len_squared(vec2 a) { return dot(a, a); }

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_UTILDEFINES_GLSL
bool flag_test(uint flag, uint val) { return (flag & val) != 0u; }
bool flag_test(int flag, uint val) { return flag_test(uint(flag), val); }
bool flag_test(int flag, int val) { return (flag & val) != 0; }

void set_flag_from_test(inout uint value, bool test, uint flag) { if (test) { value |= flag; } else { value &= ~flag; } }
void set_flag_from_test(inout int value, bool test, int flag) { if (test) { value |= flag; } else { value &= ~flag; } }
#endif

#define weighted_sum(val0, val1, val2, val3, weights) ((val0 * weights[0] + val1 * weights[1] + val2 * weights[2] + val3 * weights[3]) * safe_rcp(sum(weights)))
#define weighted_sum_array(val, weights) ((val[0] * weights[0] + val[1] * weights[1] + val[2] * weights[2] + val[3] * weights[3]) * safe_rcp(sum(weights)))

/* clang-format on */

#define saturate(a) clamp(a, 0.0, 1.0)

#define in_range_inclusive(val, min_v, max_v) \
  (all(greaterThanEqual(val, min_v)) && all(lessThanEqual(val, max_v)))
#define in_range_exclusive(val, min_v, max_v) \
  (all(greaterThan(val, min_v)) && all(lessThan(val, max_v)))
#define in_texture_range(texel, tex) \
  (all(greaterThanEqual(texel, ivec2(0))) && all(lessThan(texel, textureSize(tex, 0).xy)))

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_BASE_LIB_GLSL
uint divide_ceil(uint visible_count, uint divisor)
{
  return (visible_count + (divisor - 1u)) / divisor;
}

int divide_ceil(int visible_count, int divisor)
{
  return (visible_count + (divisor - 1)) / divisor;
}
#endif

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_VECTOR_LIB_GLSL
ivec2 divide_ceil(ivec2 visible_count, ivec2 divisor)
{
  return (visible_count + (divisor - 1)) / divisor;
}
#endif

uint bit_field_mask(uint bit_width, uint bit_min)
{
  /* Cannot bit shift more than 31 positions. */
  uint mask = (bit_width > 31u) ? 0x0u : (0xFFFFFFFFu << bit_width);
  return ~mask << bit_min;
}

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_UTILDEFINES_GLSL
uvec2 unpackUvec2x16(uint data)
{
  return (uvec2(data) >> uvec2(0u, 16u)) & uvec2(0xFFFFu);
}

uint packUvec2x16(uvec2 data)
{
  data = (data & 0xFFFFu) << uvec2(0u, 16u);
  return data.x | data.y;
}

uvec4 unpackUvec4x8(uint data)
{
  return (uvec4(data) >> uvec4(0u, 8u, 16u, 24u)) & uvec4(0xFFu);
}

uint packUvec4x8(uvec4 data)
{
  data = (data & 0xFFu) << uvec4(0u, 8u, 16u, 24u);
  return data.x | data.y | data.z | data.w;
}
#endif

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_VECTOR_LIB_GLSL
float distance_squared(vec2 a, vec2 b)
{
  a -= b;
  return dot(a, a);
}

float distance_squared(vec3 a, vec3 b)
{
  a -= b;
  return dot(a, a);
}
#endif

vec3 safe_normalize(vec3 v)
{
  float len = length(v);
  if (isnan(len) || len == 0.0) {
    return vec3(1.0, 0.0, 0.0);
  }
  return v / len;
}

vec2 safe_normalize_len(vec2 v, out float len)
{
  len = length(v);
  if (isnan(len) || len == 0.0) {
    return vec2(1.0, 0.0);
  }
  return v / len;
}

vec2 safe_normalize(vec2 v)
{
  float len;
  return safe_normalize_len(v, len);
}

vec3 normalize_len(vec3 v, out float len)
{
  len = length(v);
  return v / len;
}

vec4 safe_color(vec4 c)
{
  /* Clamp to avoid black square artifacts if a pixel goes NaN. */
  return clamp(c, vec4(0.0), vec4(1e20)); /* 1e20 arbitrary. */
}
vec3 safe_color(vec3 c)
{
  /* Clamp to avoid black square artifacts if a pixel goes NaN. */
  return clamp(c, vec3(0.0), vec3(1e20)); /* 1e20 arbitrary. */
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Fast Math
 * \{ */

/* [Drobot2014a] Low Level Optimizations for GCN */
float fast_sqrt(float v)
{
  return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(v) >> 1));
}

vec2 fast_sqrt(vec2 v)
{
  return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(v) >> 1));
}

/* [Eberly2014] GPGPU Programming for Games and Science */
float fast_acos(float v)
{
  float res = -0.156583 * abs(v) + M_PI_2;
  res *= fast_sqrt(1.0 - abs(v));
  return (v >= 0.0) ? res : M_PI - res;
}

vec2 fast_acos(vec2 v)
{
  vec2 res = -0.156583 * abs(v) + M_PI_2;
  res *= fast_sqrt(1.0 - abs(v));
  v.x = (v.x >= 0.0) ? res.x : M_PI - res.x;
  v.y = (v.y >= 0.0) ? res.y : M_PI - res.y;
  return v;
}

/** \} */

/*
 * For debugging purpose mainly.
 * From https://www.shadertoy.com/view/4dsSzr
 * By Morgan McGuire @morgan3d, http://graphicscodex.com
 * Reuse permitted under the BSD license.
 */
vec3 neon_gradient(float t)
{
  return clamp(vec3(t * 1.3 + 0.1, sqr(abs(0.43 - t) * 1.7), (1.0 - t) * 1.7), 0.0, 1.0);
}
vec3 heatmap_gradient(float t)
{
  float a = pow(t, 1.5) * 0.8 + 0.2;
  float b = smoothstep(0.0, 0.35, t) + t * 0.5;
  float c = smoothstep(0.5, 1.0, t);
  float d = max(1.0 - t * 1.7, t * 7.0 - 6.0);
  return saturate(a * vec3(b, c, d));
}
vec3 hue_gradient(float t)
{
  vec3 p = abs(fract(t + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
  return (clamp(p - 1.0, 0.0, 1.0));
}

#endif /* COMMON_MATH_LIB_GLSL */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

#ifndef DRW_GPENCIL_INFO
#  error Missing additional info draw_gpencil
#endif

#ifdef GPU_FRAGMENT_SHADER
float gpencil_stroke_round_cap_mask(vec2 p1, vec2 p2, vec2 aspect, float thickness, float hardfac)
{
  /* We create our own uv space to avoid issues with triangulation and linear
   * interpolation artifacts. */
  vec2 line = p2.xy - p1.xy;
  vec2 pos = gl_FragCoord.xy - p1.xy;
  float line_len = length(line);
  float half_line_len = line_len * 0.5;
  /* Normalize */
  line = (line_len > 0.0) ? (line / line_len) : vec2(1.0, 0.0);
  /* Create a uv space that englobe the whole segment into a capsule. */
  vec2 uv_end;
  uv_end.x = max(abs(dot(line, pos) - half_line_len) - half_line_len, 0.0);
  uv_end.y = dot(vec2(-line.y, line.x), pos);
  /* Divide by stroke radius. */
  uv_end /= thickness;
  uv_end *= aspect;

  float dist = clamp(1.0 - length(uv_end) * 2.0, 0.0, 1.0);
  if (hardfac > 0.999) {
    return step(1e-8, dist);
  }
  else {
    /* Modulate the falloff profile */
    float hardness = 1.0 - hardfac;
    dist = pow(dist, mix(0.01, 10.0, hardness));
    return smoothstep(0.0, 1.0, dist);
  }
}
#endif

vec2 gpencil_decode_aspect(int packed_data)
{
  float asp = float(uint(packed_data) & 0x1FFu) * (1.0 / 255.0);
  return (asp > 1.0) ? vec2(1.0, (asp - 1.0)) : vec2(asp, 1.0);
}

float gpencil_decode_uvrot(int packed_data)
{
  uint udata = uint(packed_data);
  float uvrot = 1e-8 + float((udata & 0x1FE00u) >> 9u) * (1.0 / 255.0);
  return ((udata & 0x20000u) != 0u) ? -uvrot : uvrot;
}

float gpencil_decode_hardness(int packed_data)
{
  return float((uint(packed_data) & 0x3FC0000u) >> 18u) * (1.0 / 255.0);
}

vec2 gpencil_project_to_screenspace(vec4 v, vec4 viewport_size)
{
  return ((v.xy / v.w) * 0.5 + 0.5) * viewport_size.xy;
}

float gpencil_stroke_thickness_modulate(float thickness, vec4 ndc_pos, vec4 viewport_size)
{
  /* Modify stroke thickness by object and layer factors. */
  thickness = max(1.0, thickness * gpThicknessScale + gpThicknessOffset);

  if (gpThicknessIsScreenSpace) {
    /* Multiply offset by view Z so that offset is constant in screenspace.
     * (e.i: does not change with the distance to camera) */
    thickness *= ndc_pos.w;
  }
  else {
    /* World space point size. */
    thickness *= gpThicknessWorldScale * ProjectionMatrix[1][1] * viewport_size.y;
  }
  return thickness;
}

float gpencil_clamp_small_stroke_thickness(float thickness, vec4 ndc_pos)
{
  /* To avoid aliasing artifacts, we clamp the line thickness and
   * reduce its opacity in the fragment shader. */
  float min_thickness = ndc_pos.w * 1.3;
  thickness = max(min_thickness, thickness);

  return thickness;
}

#ifdef GPU_VERTEX_SHADER

int gpencil_stroke_point_id()
{
  return (gl_VertexID & ~GP_IS_STROKE_VERTEX_BIT) >> GP_VERTEX_ID_SHIFT;
}

bool gpencil_is_stroke_vertex()
{
  return flag_test(gl_VertexID, GP_IS_STROKE_VERTEX_BIT);
}

/**
 * Returns value of gl_Position.
 *
 * To declare in vertex shader.
 * in ivec4 ma, ma1, ma2, ma3;
 * in vec4 pos, pos1, pos2, pos3, uv1, uv2, col1, col2, fcol1;
 *
 * All of these attributes are quad loaded the same way
 * as GL_LINES_ADJACENCY would feed a geometry shader:
 * - ma reference the previous adjacency point.
 * - ma1 reference the current line first point.
 * - ma2 reference the current line second point.
 * - ma3 reference the next adjacency point.
 * Note that we are rendering quad instances and not using any index buffer
 *(except for fills).
 *
 * Material : x is material index, y is stroke_id, z is point_id,
 *            w is aspect & rotation & hardness packed.
 * Position : contains thickness in 4th component.
 * UV : xy is UV for fills, z is U of stroke, w is strength.
 *
 *
 * WARNING: Max attribute count is actually 14 because OSX OpenGL implementation
 * considers gl_VertexID and gl_InstanceID as vertex attribute. (see #74536)
 */
vec4 gpencil_vertex(vec4 viewport_size,
                    gpMaterialFlag material_flags,
                    vec2 alignment_rot,
                    /* World Position. */
                    out vec3 out_P,
                    /* World Normal. */
                    out vec3 out_N,
                    /* Vertex Color. */
                    out vec4 out_color,
                    /* Stroke Strength. */
                    out float out_strength,
                    /* UV coordinates. */
                    out vec2 out_uv,
                    /* Screen-Space segment endpoints. */
                    out vec4 out_sspos,
                    /* Stroke aspect ratio. */
                    out vec2 out_aspect,
                    /* Stroke thickness (x: clamped, y: unclamped). */
                    out vec2 out_thickness,
                    /* Stroke hardness. */
                    out float out_hardness)
{
  int stroke_point_id = (gl_VertexID & ~GP_IS_STROKE_VERTEX_BIT) >> GP_VERTEX_ID_SHIFT;

  /* Attribute Loading. */
  vec4 pos = texelFetch(gp_pos_tx, (stroke_point_id - 1) * 3 + 0);
  vec4 pos1 = texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 0);
  vec4 pos2 = texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 0);
  vec4 pos3 = texelFetch(gp_pos_tx, (stroke_point_id + 2) * 3 + 0);
  ivec4 ma = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id - 1) * 3 + 1));
  ivec4 ma1 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 1));
  ivec4 ma2 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 1));
  ivec4 ma3 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 2) * 3 + 1));
  vec4 uv1 = texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 2);
  vec4 uv2 = texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 2);

  vec4 col1 = texelFetch(gp_col_tx, (stroke_point_id + 0) * 2 + 0);
  vec4 col2 = texelFetch(gp_col_tx, (stroke_point_id + 1) * 2 + 0);
  vec4 fcol1 = texelFetch(gp_col_tx, (stroke_point_id + 0) * 2 + 1);

#  define thickness1 pos1.w
#  define thickness2 pos2.w
#  define strength1 uv1.w
#  define strength2 uv2.w
/* Packed! need to be decoded. */
#  define hardness1 ma1.w
#  define hardness2 ma2.w
#  define uvrot1 ma1.w
#  define aspect1 ma1.w

  vec4 out_ndc;

  if (gpencil_is_stroke_vertex()) {
    bool is_dot = flag_test(material_flags, GP_STROKE_ALIGNMENT);
    bool is_squares = !flag_test(material_flags, GP_STROKE_DOTS);

    /* Special Case. Stroke with single vert are rendered as dots. Do not discard them. */
    if (!is_dot && ma.x == -1 && ma2.x == -1) {
      is_dot = true;
      is_squares = false;
    }

    /* Endpoints, we discard the vertices. */
    if (!is_dot && ma2.x == -1) {
      /* We set the vertex at the camera origin to generate 0 fragments. */
      out_ndc = vec4(0.0, 0.0, -3e36, 0.0);
      return out_ndc;
    }

    /* Avoid using a vertex attribute for quad positioning. */
    float x = float(gl_VertexID & 1) * 2.0 - 1.0; /* [-1..1] */
    float y = float(gl_VertexID & 2) - 1.0;       /* [-1..1] */

    bool use_curr = is_dot || (x == -1.0);

    vec3 wpos_adj = transform_point(ModelMatrix, (use_curr) ? pos.xyz : pos3.xyz);
    vec3 wpos1 = transform_point(ModelMatrix, pos1.xyz);
    vec3 wpos2 = transform_point(ModelMatrix, pos2.xyz);

    vec3 T;
    if (is_dot) {
      /* Shade as facing billboards. */
      T = ViewMatrixInverse[0].xyz;
    }
    else if (use_curr && ma.x != -1) {
      T = wpos1 - wpos_adj;
    }
    else {
      T = wpos2 - wpos1;
    }
    T = safe_normalize(T);

    vec3 B = cross(T, ViewMatrixInverse[2].xyz);
    out_N = normalize(cross(B, T));

    vec4 ndc_adj = point_world_to_ndc(wpos_adj);
    vec4 ndc1 = point_world_to_ndc(wpos1);
    vec4 ndc2 = point_world_to_ndc(wpos2);

    out_ndc = (use_curr) ? ndc1 : ndc2;
    out_P = (use_curr) ? wpos1 : wpos2;
    out_strength = abs((use_curr) ? strength1 : strength2);

    vec2 ss_adj = gpencil_project_to_screenspace(ndc_adj, viewport_size);
    vec2 ss1 = gpencil_project_to_screenspace(ndc1, viewport_size);
    vec2 ss2 = gpencil_project_to_screenspace(ndc2, viewport_size);
    /* Screenspace Lines tangents. */
    float line_len;
    vec2 line = safe_normalize_len(ss2 - ss1, line_len);
    vec2 line_adj = safe_normalize((use_curr) ? (ss1 - ss_adj) : (ss_adj - ss2));

    float thickness = abs((use_curr) ? thickness1 : thickness2);
    thickness = gpencil_stroke_thickness_modulate(thickness, out_ndc, viewport_size);
    float clamped_thickness = gpencil_clamp_small_stroke_thickness(thickness, out_ndc);

    out_uv = vec2(x, y) * 0.5 + 0.5;
    out_hardness = gpencil_decode_hardness(use_curr ? hardness1 : hardness2);

    if (is_dot) {
      uint alignement_mode = material_flags & GP_STROKE_ALIGNMENT;

      /* For one point strokes use object alignment. */
      if (alignement_mode == GP_STROKE_ALIGNMENT_STROKE && ma.x == -1 && ma2.x == -1) {
        alignement_mode = GP_STROKE_ALIGNMENT_OBJECT;
      }

      vec2 x_axis;
      if (alignement_mode == GP_STROKE_ALIGNMENT_STROKE) {
        x_axis = (ma2.x == -1) ? line_adj : line;
      }
      else if (alignement_mode == GP_STROKE_ALIGNMENT_FIXED) {
        /* Default for no-material drawing. */
        x_axis = vec2(1.0, 0.0);
      }
      else { /* GP_STROKE_ALIGNMENT_OBJECT */
        vec4 ndc_x = point_world_to_ndc(wpos1 + ModelMatrix[0].xyz);
        vec2 ss_x = gpencil_project_to_screenspace(ndc_x, viewport_size);
        x_axis = safe_normalize(ss_x - ss1);
      }

      /* Rotation: Encoded as Cos + Sin sign. */
      float uv_rot = gpencil_decode_uvrot(uvrot1);
      float rot_sin = sqrt(max(0.0, 1.0 - uv_rot * uv_rot)) * sign(uv_rot);
      float rot_cos = abs(uv_rot);
      /* TODO(@fclem): Optimize these 2 matrix mul into one by only having one rotation angle and
       * using a cosine approximation. */
      x_axis = mat2(rot_cos, -rot_sin, rot_sin, rot_cos) * x_axis;
      x_axis = mat2(alignment_rot.x, -alignment_rot.y, alignment_rot.y, alignment_rot.x) * x_axis;
      /* Rotate 90° Counter-Clockwise. */
      vec2 y_axis = vec2(-x_axis.y, x_axis.x);

      out_aspect = gpencil_decode_aspect(aspect1);

      x *= out_aspect.x;
      y *= out_aspect.y;

      /* Invert for vertex shader. */
      out_aspect = 1.0 / out_aspect;

      out_ndc.xy += (x * x_axis + y * y_axis) * viewport_size.zw * clamped_thickness;

      out_sspos.xy = ss1;
      out_sspos.zw = ss1 + x_axis * 0.5;
      out_thickness.x = (is_squares) ? 1e18 : (clamped_thickness / out_ndc.w);
      out_thickness.y = (is_squares) ? 1e18 : (thickness / out_ndc.w);
    }
    else {
      bool is_stroke_start = (ma.x == -1 && x == -1.0);
      bool is_stroke_end = (ma3.x == -1 && x == 1.0);

      /* Mitter tangent vector. */
      vec2 miter_tan = safe_normalize(line_adj + line);
      float miter_dot = dot(miter_tan, line_adj);
      /* Break corners after a certain angle to avoid really thick corners. */
      const float miter_limit = 0.5; /* cos(60°) */
      bool miter_break = (miter_dot < miter_limit);
      miter_tan = (miter_break || is_stroke_start || is_stroke_end) ? line :
                                                                      (miter_tan / miter_dot);
      /* Rotate 90° Counter-Clockwise. */
      vec2 miter = vec2(-miter_tan.y, miter_tan.x);

      out_sspos.xy = ss1;
      out_sspos.zw = ss2;
      out_thickness.x = clamped_thickness / out_ndc.w;
      out_thickness.y = thickness / out_ndc.w;
      out_aspect = vec2(1.0);

      vec2 screen_ofs = miter * y;

      /* Reminder: we packed the cap flag into the sign of strength and thickness sign. */
      if ((is_stroke_start && strength1 > 0.0) || (is_stroke_end && thickness1 > 0.0) ||
          (miter_break && !is_stroke_start && !is_stroke_end))
      {
        screen_ofs += line * x;
      }

      out_ndc.xy += screen_ofs * viewport_size.zw * clamped_thickness;

      out_uv.x = (use_curr) ? uv1.z : uv2.z;
    }

    out_color = (use_curr) ? col1 : col2;
  }
  else {
    /* Fill vertex. */
    out_P = transform_point(ModelMatrix, pos1.xyz);
    out_ndc = point_world_to_ndc(out_P);
    out_uv = uv1.xy;
    out_thickness.x = 1e18;
    out_thickness.y = 1e20;
    out_hardness = 1.0;
    out_aspect = vec2(1.0);
    out_sspos = vec4(0.0);

    /* Flat normal following camera and object bounds. */
    vec3 V = cameraVec(ModelMatrix[3].xyz);
    vec3 N = normal_world_to_object(V);
    N *= OrcoTexCoFactors[1].xyz;
    N = normal_object_to_world(N);
    out_N = safe_normalize(N);

    /* Decode fill opacity. */
    out_color = vec4(fcol1.rgb, floor(fcol1.a / 10.0) / 10000.0);

    /* We still offset the fills a little to avoid overlaps */
    out_ndc.z += 0.000002;
  }

#  undef thickness1
#  undef thickness2
#  undef strength1
#  undef strength2
#  undef hardness1
#  undef hardness2
#  undef uvrot1
#  undef aspect1

  return out_ndc;
}

vec4 gpencil_vertex(vec4 viewport_size,
                    out vec3 out_P,
                    out vec3 out_N,
                    out vec4 out_color,
                    out float out_strength,
                    out vec2 out_uv,
                    out vec4 out_sspos,
                    out vec2 out_aspect,
                    out vec2 out_thickness,
                    out float out_hardness)
{
  return gpencil_vertex(viewport_size,
                        0u,
                        vec2(1.0, 0.0),
                        out_P,
                        out_N,
                        out_color,
                        out_strength,
                        out_uv,
                        out_sspos,
                        out_aspect,
                        out_thickness,
                        out_hardness);
}

#endif

#pragma BLENDER_REQUIRE(common_gpencil_lib.glsl)

void gpencil_color_output(vec4 stroke_col, vec4 vert_col, float vert_strength, float mix_tex)
{
  /* Mix stroke with other colors. */
  vec4 mixed_col = stroke_col;
  mixed_col.rgb = mix(mixed_col.rgb, vert_col.rgb, vert_col.a * gpVertexColorOpacity);
  mixed_col.rgb = mix(mixed_col.rgb, gpLayerTint.rgb, gpLayerTint.a);
  mixed_col.a *= vert_strength * gpLayerOpacity;
  /**
   * This is what the fragment shader looks like.
   * out = col * gp_interp.color_mul + col.a * gp_interp.color_add.
   * gp_interp.color_mul is how much of the texture color to keep.
   * gp_interp.color_add is how much of the mixed color to add.
   * Note that we never add alpha. This is to keep the texture act as a stencil.
   * We do however, modulate the alpha (reduce it).
   */
  /* We add the mixed color. This is 100% mix (no texture visible). */
  gp_interp.color_mul = vec4(mixed_col.aaa, mixed_col.a);
  gp_interp.color_add = vec4(mixed_col.rgb * mixed_col.a, 0.0);
  /* Then we blend according to the texture mix factor.
   * Note that we keep the alpha modulation. */
  gp_interp.color_mul.rgb *= mix_tex;
  gp_interp.color_add.rgb *= 1.0 - mix_tex;
}

void main()
{
  float vert_strength;
  vec4 vert_color;
  vec3 vert_N;

  ivec4 ma1 = floatBitsToInt(texelFetch(gp_pos_tx, gpencil_stroke_point_id() * 3 + 1));
  gpMaterial gp_mat = materials[ma1.x + gpMaterialOffset];
  gpMaterialFlag gp_flag = floatBitsToUint(gp_mat._flag);

  gl_Position = gpencil_vertex(vec4(viewportSize, 1.0 / viewportSize),
                               gp_flag,
                               gp_mat._alignment_rot,
                               gp_interp.pos,
                               vert_N,
                               vert_color,
                               vert_strength,
                               gp_interp.uv,
                               gp_interp.sspos,
                               gp_interp.aspect,
                               gp_interp.thickness,
                               gp_interp.hardness);

  if (gpencil_is_stroke_vertex()) {
    if (!flag_test(gp_flag, GP_STROKE_ALIGNMENT)) {
      gp_interp.uv.x *= gp_mat._stroke_u_scale;
    }

    /* Special case: We don't use vertex color if material Holdout. */
    if (flag_test(gp_flag, GP_STROKE_HOLDOUT)) {
      vert_color = vec4(0.0);
    }

    gpencil_color_output(
        gp_mat.stroke_color, vert_color, vert_strength, gp_mat._stroke_texture_mix);

    gp_interp.mat_flag = gp_flag & ~GP_FILL_FLAGS;

    if (gpStrokeOrder3d) {
      /* Use the fragment depth (see fragment shader). */
      gp_interp.depth = -1.0;
    }
    else if (flag_test(gp_flag, GP_STROKE_OVERLAP)) {
      /* Use the index of the point as depth.
       * This means the stroke can overlap itself. */
      float point_index = float(ma1.z);
      gp_interp.depth = (point_index + gpStrokeIndexOffset + 2.0) * 0.0000002;
    }
    else {
      /* Use the index of first point of the stroke as depth.
       * We render using a greater depth test this means the stroke
       * cannot overlap itself.
       * We offset by one so that the fill can be overlapped by its stroke.
       * The offset is ok since we pad the strokes data because of adjacency infos. */
      float stroke_index = float(ma1.y);
      gp_interp.depth = (stroke_index + gpStrokeIndexOffset + 2.0) * 0.0000002;
    }
  }
  else {
    int stroke_point_id = gpencil_stroke_point_id();
    vec4 uv1 = texelFetch(gp_pos_tx, stroke_point_id * 3 + 2);
    vec4 fcol1 = texelFetch(gp_col_tx, stroke_point_id * 2 + 1);
    vec4 fill_col = gp_mat.fill_color;

    /* Special case: We don't modulate alpha in gradient mode. */
    if (flag_test(gp_flag, GP_FILL_GRADIENT_USE)) {
      fill_col.a = 1.0;
    }

    /* Decode fill opacity. */
    vec4 fcol_decode = vec4(fcol1.rgb, floor(fcol1.a / 10.0));
    float fill_opacity = fcol1.a - (fcol_decode.a * 10.0);
    fcol_decode.a /= 10000.0;

    /* Special case: We don't use vertex color if material Holdout. */
    if (flag_test(gp_flag, GP_FILL_HOLDOUT)) {
      fcol_decode = vec4(0.0);
    }

    /* Apply opacity. */
    fill_col.a *= fill_opacity;
    /* If factor is > 1 force opacity. */
    if (fill_opacity > 1.0) {
      fill_col.a += fill_opacity - 1.0;
    }

    fill_col.a = clamp(fill_col.a, 0.0, 1.0);

    gpencil_color_output(fill_col, fcol_decode, 1.0, gp_mat._fill_texture_mix);

    gp_interp.mat_flag = gp_flag & GP_FILL_FLAGS;
    gp_interp.mat_flag |= uint(ma1.x + gpMaterialOffset) << GPENCIl_MATID_SHIFT;

    gp_interp.uv = mat2(gp_mat.fill_uv_rot_scale.xy, gp_mat.fill_uv_rot_scale.zw) * uv1.xy +
                   gp_mat._fill_uv_offset;

    if (gpStrokeOrder3d) {
      /* Use the fragment depth (see fragment shader). */
      gp_interp.depth = -1.0;
    }
    else {
      /* Use the index of first point of the stroke as depth. */
      float stroke_index = float(ma1.y);
      gp_interp.depth = (stroke_index + gpStrokeIndexOffset + 1.0) * 0.0000002;
    }
  }
}
