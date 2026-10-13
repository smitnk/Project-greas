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
#  define samplerCubeArray sampler2DArray
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
#define BLUR 
#define USE_GPU_SHADER_CREATE_INFO

/* Pass Resources. */
layout(binding = 0) uniform sampler2D colorBuf;
layout(binding = 1) uniform sampler2D revealBuf;

/* Batch Resources. */

/* Push Constants. */
uniform vec2 offset;
uniform int sampCount;


/* Inputs. */

/* Interfaces. */
out fullscreen_iface{
  smooth vec4 uvcoordsvar;
};


#ifndef USE_GPU_SHADER_CREATE_INFO
out vec4 uvcoordsvar;
#endif

void main()
{
  int v = gl_VertexID % 3;
  float x = -1.0 + float((v & 1) << 2);
  float y = -1.0 + float((v & 2) << 1);
  gl_Position = vec4(x, y, 1.0, 1.0);
  uvcoordsvar = vec4((gl_Position.xy + 1.0) * 0.5, 0.0, 0.0);
}
