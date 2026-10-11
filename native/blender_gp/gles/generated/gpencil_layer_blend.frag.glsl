#version 320 es
#extension GL_EXT_shader_implicit_conversions : require
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
#define GPU_FRAGMENT_SHADER
#define USE_GPU_SHADER_CREATE_INFO

/* Pass Resources. */
layout(binding = 0) uniform sampler2D colorBuf;
layout(binding = 1) uniform sampler2D revealBuf;
layout(binding = 2) uniform sampler2D maskBuf;

/* Batch Resources. */

/* Push Constants. */
uniform int blendMode;
uniform float blendOpacity;


/* Interfaces. */
in fullscreen_iface{
  smooth vec4 uvcoordsvar;
};

/* Outputs. */
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragRevealage;


/* Must match eGPLayerBlendModes */
#define MODE_REGULAR 0
#define MODE_HARDLIGHT 1
#define MODE_ADD 2
#define MODE_SUB 3
#define MODE_MULTIPLY 4
#define MODE_DIVIDE 5
#define MODE_HARDLIGHT_SECOND_PASS 999

void blend_mode_output(
    int blend_mode, vec4 color, float opacity, out vec4 frag_color, out vec4 frag_revealage)
{
  switch (blend_mode) {
    case MODE_REGULAR:
      /* Reminder: Blending func is premult alpha blend `(dst.rgba * (1 - src.a) + src.rgb)`. */
      color *= opacity;
      frag_color = color;
      frag_revealage = vec4(0.0, 0.0, 0.0, color.a);
      break;
    case MODE_MULTIPLY:
      /* Reminder: Blending func is multiply blend `(dst.rgba * src.rgba)`. */
      color.a *= opacity;
      frag_revealage = frag_color = (1.0 - color.a) + color.a * color;
      break;
    case MODE_DIVIDE:
      /* Reminder: Blending func is multiply blend `(dst.rgba * src.rgba)`. */
      color.a *= opacity;
      frag_revealage = frag_color = clamp(1.0 / max(vec4(1e-6), 1.0 - color * color.a), 0.0, 1e18);
      break;
    case MODE_HARDLIGHT: {
      /* Reminder: Blending func is multiply blend `(dst.rgba * src.rgba)`. */
      /**
       * We need to separate the overlay equation into 2 term (one mul and one add).
       * This is the standard overlay equation (per channel):
       * `rtn = (src < 0.5) ? (2.0 * src * dst) : (1.0 - 2.0 * (1.0 - src) * (1.0 - dst));`
       * We rewrite the second branch like this:
       * `rtn = 1 - 2 * (1 - src) * (1 - dst);`
       * `rtn = 1 - 2 (1 - dst + src * dst - src);`
       * `rtn = 1 - 2 (1 - dst * (1 - src) - src);`
       * `rtn = 1 - 2 + dst * (2 - 2 * src) + 2 * src;`
       * `rtn = (- 1 + 2 * src) + dst * (2 - 2 * src);`
       */
      color = mix(vec4(0.5), color, color.a * opacity);
      vec4 s = step(-0.5, -color);
      frag_revealage = frag_color = 2.0 * s + 2.0 * color * (1.0 - s * 2.0);
      frag_revealage = max(vec4(0.0), frag_revealage);
      break;
    }
    case MODE_HARDLIGHT_SECOND_PASS:
      /* Reminder: Blending func is additive blend (dst.rgba + src.rgba). */
      color = mix(vec4(0.5), color, color.a * opacity);
      frag_revealage = frag_color = (-1.0 + 2.0 * color) * step(-0.5, -color);
      frag_revealage = max(vec4(0.0), frag_revealage);
      break;
    case MODE_SUB:
    case MODE_ADD:
      /* Reminder: Blending func is additive / subtractive blend `(dst.rgba +/- src.rgba)`. */
      frag_color = color * color.a * opacity;
      frag_revealage = vec4(0.0);
      break;
  }
}

#pragma BLENDER_REQUIRE(gpencil_common_lib.glsl)

void main()
{
  vec4 color;

  /* Remember, this is associated alpha (aka. premult). */
  color.rgb = textureLod(colorBuf, uvcoordsvar.xy, 0).rgb;
  /* Stroke only render mono-chromatic revealage. We convert to alpha. */
  color.a = 1.0 - textureLod(revealBuf, uvcoordsvar.xy, 0).r;

  float mask = textureLod(maskBuf, uvcoordsvar.xy, 0).r;
  mask *= blendOpacity;

  fragColor = vec4(1.0, 0.0, 1.0, 1.0);
  fragRevealage = vec4(1.0, 0.0, 1.0, 1.0);

  blend_mode_output(blendMode, color, mask, fragColor, fragRevealage);
}
