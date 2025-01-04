#include <metal_stdlib>
#include <simd/simd.h>
#include "shaders_lib.h"
using namespace metal;

typedef struct
{
  float4x4 u_modelView;
  float4x4 u_projection;
  float4x4 u_pivotTransform;
  float4 u_routeParams;
  float4 u_color;
  float4 u_maskColor;
  float4 u_outlineColor;
  packed_float2 u_pattern;
  packed_float2 u_angleCosSin;
  float u_arrowHalfWidth;
  float u_opacity;
  packed_float2 u_fakeBorders;
  float4 u_fakeColor;
  float4 u_fakeOutlineColor;
} Uniforms_T;

// Route/RouteDash

typedef struct
{
  float3 a_position [[attribute(0)]];
  float2 a_normal [[attribute(1)]];
  float3 a_length [[attribute(2)]];
  float4 a_color [[attribute(3)]];
} RouteVertex_T;

typedef struct
{
  float4 position [[position]];
  float3 lengthParams;
  float4 color;
} RouteFragment_T;

vertex RouteFragment_T vsRoute(const RouteVertex_T in [[stage_in]],
                               constant Uniforms_T & uniforms [[buffer(1)]])
{
  RouteFragment_T out;

  float2 transformedAxisPos = (float4(in.a_position.xy, 0.0, 1.0) * uniforms.u_modelView).xy;
  float2 len = float2(in.a_length.x, in.a_length.z);
  if (dot(in.a_normal, in.a_normal) != 0.0)
  {
    float2 norm = in.a_normal * uniforms.u_routeParams.x;
    transformedAxisPos = CalcLineTransformedAxisPos(transformedAxisPos, in.a_position.xy + norm,
                                                    uniforms.u_modelView, length(norm));
    if (uniforms.u_routeParams.y != 0.0)
      len = float2(in.a_length.x + in.a_length.y * uniforms.u_routeParams.y, in.a_length.z);
  }
  
  out.lengthParams = float3(len, uniforms.u_routeParams.z);
  out.color = in.a_color;
  float4 pos = float4(transformedAxisPos, in.a_position.z, 1.0) * uniforms.u_projection;
  out.position = ApplyPivotTransform(pos, uniforms.u_pivotTransform, 0.0);
  
  return out;
}

fragment float4 fsRoute(const RouteFragment_T in [[stage_in]],
                        constant Uniforms_T & uniforms [[buffer(1)]])
{
  if (in.lengthParams.x < in.lengthParams.z)
    discard_fragment();
  
  constexpr float kAntialiasingThreshold = 0.92;
  constexpr float kOutlineThreshold1 = 0.81;
  constexpr float kOutlineThreshold2 = 0.71;
  
  float2 fb = uniforms.u_fakeBorders;
  float2 coefs = step(in.lengthParams.xx, fb);
  coefs.y = 1.0 - coefs.y;
  float4 mainColor = mix(uniforms.u_color, uniforms.u_fakeColor, coefs.x);
  mainColor = mix(mainColor, uniforms.u_fakeColor, coefs.y);
  float4 mainOutlineColor = mix(uniforms.u_outlineColor, uniforms.u_fakeOutlineColor, coefs.x);
  mainOutlineColor = mix(mainOutlineColor, uniforms.u_fakeOutlineColor, coefs.y);
  
  float4 color = mix(mix(mainColor, float4(in.color.rgb, 1.0), in.color.a), mainColor,
                     step(uniforms.u_routeParams.w, 0.0));
  color = mix(color, mainOutlineColor, step(kOutlineThreshold1, abs(in.lengthParams.y)));
  color = mix(color, mainOutlineColor,
              smoothstep(kOutlineThreshold2, kOutlineThreshold1, abs(in.lengthParams.y)));
  color.a *= (1.0 - smoothstep(kAntialiasingThreshold, 1.0, abs(in.lengthParams.y)));
  color = float4(mix(color.rgb, uniforms.u_maskColor.rgb, uniforms.u_maskColor.a), color.a);
  return color;
}

float AlphaFromPattern(float curLen, float2 dashGapLen)
{
  float len = dashGapLen.x + dashGapLen.y;
  float offset = fract(curLen / len) * len;
  return step(offset, dashGapLen.x);
}

fragment float4 fsRouteDash(const RouteFragment_T in [[stage_in]],
                            constant Uniforms_T & uniforms [[buffer(1)]])
{
  if (in.lengthParams.x < in.lengthParams.z)
    discard_fragment();
  
  constexpr float kAntialiasingThreshold = 0.92;
  
  float2 fb = uniforms.u_fakeBorders;
  float2 coefs = step(in.lengthParams.xx, fb);
  coefs.y = 1.0 - coefs.y;
  float4 mainColor = mix(uniforms.u_color, uniforms.u_fakeColor, coefs.x);
  mainColor = mix(mainColor, uniforms.u_fakeColor, coefs.y);
  
  float4 color = mainColor + in.color;
  float a = 1.0 - smoothstep(kAntialiasingThreshold, 1.0, abs(in.lengthParams.y));
  color.a *= (a * AlphaFromPattern(in.lengthParams.x, uniforms.u_pattern));
  color = float4(mix(color.rgb, uniforms.u_maskColor.rgb, uniforms.u_maskColor.a), color.a);
  return color;
}

fragment float4 fsRouteDifficulty(const RouteFragment_T in [[stage_in]],
                                  constant Uniforms_T & uniforms [[buffer(1)]])
{
  if (in.lengthParams.x < in.lengthParams.z)
    discard_fragment();
  
  constexpr float kAntialiasingThreshold = 0.92;
  
  float2 fb = uniforms.u_fakeBorders;
  float2 coefs = step(in.lengthParams.xx, fb);
  coefs.y = 1.0 - coefs.y;
  float4 mainColor = mix(uniforms.u_color, uniforms.u_fakeColor, coefs.x);
  mainColor = mix(mainColor, uniforms.u_fakeColor, coefs.y);
  
  float a = (1.0 - smoothstep(kAntialiasingThreshold, 1.0, abs(v_length.y))) * 0.8;
  float4 color = float4(mix(mainColor.rgb, in.color.rgb, in.color.a), a);
  return color;
}

// RouteArrow

typedef struct
{
  float4 a_position [[attribute(0)]];
  float2 a_normal [[attribute(1)]];
  float2 a_texCoords [[attribute(2)]];
} RouteArrowVertex_T;

typedef struct
{
  float4 position [[position]];
  float2 texCoords;
} RouteArrowFragment_T;

typedef struct
{
  float4 color [[color(0)]];
  float depth [[depth(any)]];
} RouteArrowFragment_Output;

vertex RouteArrowFragment_T vsRouteArrow(const RouteArrowVertex_T in [[stage_in]],
                                         constant Uniforms_T & uniforms [[buffer(1)]])
{
  RouteArrowFragment_T out;
  
  float2 transformedAxisPos = (float4(in.a_position.xy, 0.0, 1.0) * uniforms.u_modelView).xy;
  if (dot(in.a_normal, in.a_normal) != 0.0)
  {
    float2 norm = in.a_normal * uniforms.u_arrowHalfWidth;
    transformedAxisPos = CalcLineTransformedAxisPos(transformedAxisPos, in.a_position.xy + norm,
                                                    uniforms.u_modelView, length(norm));
  }
  
  out.texCoords = in.a_texCoords;
  float4 pos = float4(transformedAxisPos, in.a_position.z, 1.0) * uniforms.u_projection;
  out.position = ApplyPivotTransform(pos, uniforms.u_pivotTransform, 0.0);
  
  return out;
}

fragment RouteArrowFragment_Output fsRouteArrow(const RouteArrowFragment_T in [[stage_in]],
                                                constant Uniforms_T & uniforms [[buffer(1)]],
                                                texture2d<float> u_colorTex [[texture(0)]],
                                                sampler u_colorTexSampler [[sampler(0)]])
{
  RouteArrowFragment_Output output;
  float4 color = u_colorTex.sample(u_colorTexSampler, in.texCoords);
  color.a *= uniforms.u_opacity;
  output.depth = in.position.z;
  if (color.a < 0.001)
    output.depth = 1.0;
  output.color = float4(mix(color.rgb, uniforms.u_maskColor.rgb, uniforms.u_maskColor.a), color.a);
  return output;
}

// RouteMarker

typedef struct
{
  float4 a_position [[attribute(0)]];
  float3 a_normal [[attribute(1)]];
  float4 a_color [[attribute(2)]];
} RouteMarkerVertex_T;

typedef struct
{
  float4 position [[position]];
  float4 radius;
  float4 color;
} RouteMarkerFragment_T;

typedef struct
{
  float4 color [[color(0)]];
  float depth [[depth(any)]];
} RouteMarkerFragment_Output;

vertex RouteMarkerFragment_T vsRouteMarker(const RouteMarkerVertex_T in [[stage_in]],
                                           constant Uniforms_T & uniforms [[buffer(1)]])
{
  RouteMarkerFragment_T out;
  
  float r = uniforms.u_routeParams.x * in.a_normal.z;
  float2 cs = uniforms.u_angleCosSin;
  float2 normal = float2(in.a_normal.x * cs.x - in.a_normal.y * cs.y,
                         in.a_normal.x * cs.y + in.a_normal.y * cs.x);
  float4 radius = float4(normal * r, r, in.a_position.w);
  float4 pos = float4(in.a_position.xy, 0.0, 1.0) * uniforms.u_modelView;
  float2 shiftedPos = radius.xy + pos.xy;
  pos = float4(shiftedPos, in.a_position.z, 1.0) * uniforms.u_projection;
  out.radius = radius;
  out.color = in.a_color;
  out.position = ApplyPivotTransform(pos, uniforms.u_pivotTransform, 0.0);

  return out;
}

fragment RouteMarkerFragment_Output fsRouteMarker(const RouteMarkerFragment_T in [[stage_in]],
                                                  constant Uniforms_T & uniforms [[buffer(1)]])
{
  if (uniforms.u_routeParams.y > in.radius.w)
    discard_fragment();
  
  RouteMarkerFragment_Output output;
  
  constexpr float kAntialiasingPixelsCount = 2.5;
  float4 color = in.color;
  
  float aaRadius = max(in.radius.z - kAntialiasingPixelsCount, 0.0);
  float stepValue = smoothstep(aaRadius * aaRadius, in.radius.z * in.radius.z,
                               dot(in.radius.xy, in.radius.xy));
  color.a = color.a * uniforms.u_opacity * (1.0 - stepValue);
  
  output.depth = in.position.z;
  if (color.a < 0.001)
    output.depth = 1.0;
  output.color = float4(mix(color.rgb, uniforms.u_maskColor.rgb, uniforms.u_maskColor.a), color.a);
  
  return output;
}
