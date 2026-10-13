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
#define GPU_FRAGMENT_SHADER
#define GLOW 
#define USE_GPU_SHADER_CREATE_INFO

/* Pass Resources. */
layout(binding = 0) uniform sampler2D colorBuf;
layout(binding = 1) uniform sampler2D revealBuf;

/* Batch Resources. */

/* Push Constants. */
uniform vec4 glowColor;
uniform vec2 offset;
uniform int sampCount;
uniform vec4 threshold;
uniform bool firstPass;
uniform bool glowUnder;
uniform int blendMode;


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

float gaussian_weight(float x)
{
  return exp(-x * x / (2.0 * 0.35 * 0.35));
}

#if defined(COMPOSITE)

void main()
{
  if (isFirstPass) {
    /* Blend mode is multiply. */
    fragColor.rgb = fragRevealage.rgb = texture(revealBuf, uvcoordsvar.xy).rgb;
    fragColor.a = fragRevealage.a = 1.0;
  }
  else {
    /* Blend mode is additive. */
    fragRevealage = vec4(0.0);
    fragColor.rgb = texture(colorBuf, uvcoordsvar.xy).rgb;
    fragColor.a = 0.0;
  }
}

#elif defined(COLORIZE)

const mat3 sepia_mat = mat3(
    vec3(0.393, 0.349, 0.272), vec3(0.769, 0.686, 0.534), vec3(0.189, 0.168, 0.131));

#  define MODE_GRAYSCALE 0
#  define MODE_SEPIA 1
#  define MODE_DUOTONE 2
#  define MODE_CUSTOM 3
#  define MODE_TRANSPARENT 4

void main()
{
  fragColor = texture(colorBuf, uvcoordsvar.xy);
  fragRevealage = texture(revealBuf, uvcoordsvar.xy);

  float luma = dot(fragColor.rgb, vec3(0.2126, 0.7152, 0.723));

  /* No blending. */
  switch (mode) {
    case MODE_GRAYSCALE:
      fragColor.rgb = mix(fragColor.rgb, vec3(luma), factor);
      break;
    case MODE_SEPIA:
      fragColor.rgb = mix(fragColor.rgb, sepia_mat * fragColor.rgb, factor);
      break;
    case MODE_DUOTONE:
      fragColor.rgb = luma * ((luma <= factor) ? lowColor : highColor);
      break;
    case MODE_CUSTOM:
      fragColor.rgb = mix(fragColor.rgb, luma * lowColor, factor);
      break;
    case MODE_TRANSPARENT:
    default:
      fragColor.rgb *= factor;
      fragRevealage.rgb = mix(vec3(1.0), fragRevealage.rgb, factor);
      break;
  }
}

#elif defined(BLUR)

void main()
{
  vec2 pixel_size = 1.0 / vec2(textureSize(revealBuf, 0).xy);
  vec2 ofs = offset * pixel_size;

  fragColor = vec4(0.0);
  fragRevealage = vec4(0.0);

  /* No blending. */
  float weight_accum = 0.0;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    vec2 uv = uvcoordsvar.xy + ofs * x;
    fragColor.rgb += texture(colorBuf, uv).rgb * weight;
    fragRevealage.rgb += texture(revealBuf, uv).rgb * weight;
  }

  fragColor /= weight_accum;
  fragRevealage /= weight_accum;
}

#elif defined(TRANSFORM)

void main()
{
  vec2 uv = (uvcoordsvar.xy - 0.5) * axisFlip + 0.5;

  /* Wave deform. */
  float wave_time = dot(uv, waveDir.xy);
  uv += sin(wave_time + wavePhase) * waveOffset;
  /* Swirl deform. */
  if (swirlRadius > 0.0) {
    vec2 tex_size = vec2(textureSize(colorBuf, 0).xy);
    vec2 pix_coord = uv * tex_size - swirlCenter;
    float dist = length(pix_coord);
    float percent = clamp((swirlRadius - dist) / swirlRadius, 0.0, 1.0);
    float theta = percent * percent * swirlAngle;
    float s = sin(theta);
    float c = cos(theta);
    mat2 rot = mat2(vec2(c, -s), vec2(s, c));
    uv = (rot * pix_coord + swirlCenter) / tex_size;
  }

  fragColor = texture(colorBuf, uv);
  fragRevealage = texture(revealBuf, uv);
}

#elif defined(GLOW)

void main()
{
  vec2 pixel_size = 1.0 / vec2(textureSize(revealBuf, 0).xy);
  vec2 ofs = offset * pixel_size;

  fragColor = vec4(0.0);
  fragRevealage = vec4(0.0);

  float weight_accum = 0.0;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    vec2 uv = uvcoordsvar.xy + ofs * x;
    vec3 col = texture(colorBuf, uv).rgb;
    vec3 rev = texture(revealBuf, uv).rgb;
    if (threshold.x > -1.0) {
      if (threshold.y > -1.0) {
        if (any(greaterThan(abs(col - vec3(threshold)), vec3(threshold.w)))) {
          weight = 0.0;
        }
      }
      else {
        if (dot(col, vec3(1.0 / 3.0)) < threshold.x) {
          weight = 0.0;
        }
      }
    }
    fragColor.rgb += col * weight;
    fragRevealage.rgb += (1.0 - rev) * weight;
  }

  if (weight_accum > 0.0) {
    fragColor *= glowColor.rgbb / weight_accum;
    fragRevealage = fragRevealage / weight_accum;
  }
  fragRevealage = 1.0 - fragRevealage;

  if (glowUnder) {
    if (firstPass) {
      /* In first pass we copy the revealage buffer in the alpha channel.
       * This let us do the alpha under in second pass. */
      vec3 original_revealage = texture(revealBuf, uvcoordsvar.xy).rgb;
      fragRevealage.a = clamp(dot(original_revealage.rgb, vec3(0.333334)), 0.0, 1.0);
    }
    else {
      /* Recover original revealage. */
      fragRevealage.a = texture(revealBuf, uvcoordsvar.xy).a;
    }
  }

  if (!firstPass) {
    fragColor.a = clamp(1.0 - dot(fragRevealage.rgb, vec3(0.333334)), 0.0, 1.0);
    fragRevealage.a *= glowColor.a;
    blend_mode_output(blendMode, fragColor, fragRevealage.a, fragColor, fragRevealage);
  }
}

#elif defined(RIM)

void main()
{
  /* Blur revealage buffer. */
  fragRevealage = vec4(0.0);
  float weight_accum = 0.0;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    vec2 uv = uvcoordsvar.xy + blurDir * x + uvOffset;
    vec3 col = texture(revealBuf, uv).rgb;
    if (any(not(equal(vec2(0.0), floor(uv))))) {
      col = vec3(0.0);
    }
    fragRevealage.rgb += col * weight;
  }
  fragRevealage /= weight_accum;

  if (isFirstPass) {
    /* In first pass we copy the reveal buffer. This let us do alpha masking in second pass. */
    fragColor = texture(revealBuf, uvcoordsvar.xy);
    /* Also add the masked color to the reveal buffer. */
    vec3 col = texture(colorBuf, uvcoordsvar.xy).rgb;
    if (all(lessThan(abs(col - maskColor), vec3(0.05)))) {
      fragColor = vec4(1.0);
    }
  }
  else {
    /* Premult by foreground alpha (alpha mask). */
    float mask = 1.0 - clamp(dot(vec3(0.333334), texture(colorBuf, uvcoordsvar.xy).rgb), 0.0, 1.0);

    /* fragRevealage is blurred shadow. */
    float rim = clamp(dot(vec3(0.333334), fragRevealage.rgb), 0.0, 1.0);

    vec4 color = vec4(rimColor, 1.0);

    blend_mode_output(blendMode, color, rim * mask, fragColor, fragRevealage);
  }
}

#elif defined(SHADOW)

vec2 compute_uvs(float x)
{
  vec2 uv = uvcoordsvar.xy;
  /* Transform UV (loc, rot, scale) */
  uv = uv.x * uvRotX + uv.y * uvRotY + uvOffset;
  uv += blurDir * x;
  /* Wave deform. */
  float wave_time = dot(uv, waveDir.xy);
  uv += sin(wave_time + wavePhase) * waveOffset;
  return uv;
}

void main()
{
  /* Blur revealage buffer. */
  fragRevealage = vec4(0.0);
  float weight_accum = 0.0;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    vec2 uv = compute_uvs(x);
    vec3 col = texture(revealBuf, uv).rgb;
    if (any(not(equal(vec2(0.0), floor(uv))))) {
      col = vec3(1.0);
    }
    fragRevealage.rgb += col * weight;
  }
  fragRevealage /= weight_accum;

  /* No blending in first pass, alpha over premult in second pass. */
  if (isFirstPass) {
    /* In first pass we copy the reveal buffer. This let us do alpha under in second pass. */
    fragColor = texture(revealBuf, uvcoordsvar.xy);
  }
  else {
    /* fragRevealage is blurred shadow. */
    float shadow_fac = 1.0 - clamp(dot(vec3(0.333334), fragRevealage.rgb), 0.0, 1.0);
    /* Premult by foreground revealage (alpha under). */
    vec3 original_revealage = texture(colorBuf, uvcoordsvar.xy).rgb;
    shadow_fac *= clamp(dot(vec3(0.333334), original_revealage), 0.0, 1.0);
    /* Modulate by opacity */
    shadow_fac *= shadowColor.a;
    /* Apply shadow color. */
    fragColor.rgb = mix(vec3(0.0), shadowColor.rgb, shadow_fac);
    /* Alpha over (mask behind the shadow). */
    fragColor.a = shadow_fac;

    fragRevealage.rgb = original_revealage * (1.0 - shadow_fac);
    /* Replace the whole revealage buffer. */
    fragRevealage.a = 1.0;
  }
}

#elif defined(PIXELIZE)

void main()
{
  vec2 pixel = floor((uvcoordsvar.xy - targetPixelOffset) / targetPixelSize);
  vec2 uv = (pixel + 0.5) * targetPixelSize + targetPixelOffset;

  fragColor = vec4(0.0);
  fragRevealage = vec4(0.0);

  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount + 1);
    vec2 uv_ofs = uv + accumOffset * 0.5 * x;
    fragColor += texture(colorBuf, uv_ofs);
    fragRevealage += texture(revealBuf, uv_ofs);
  }

  fragColor /= float(sampCount) * 2.0 + 1.0;
  fragRevealage /= float(sampCount) * 2.0 + 1.0;
}

#endif
