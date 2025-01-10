#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

typedef struct
{
  float2 a_position [[attribute(0)]];
  float2 a_texCoords [[attribute(1)]];
  float4 a_color [[attribute(2)]];
} Vertex_T;

typedef struct
{
  float4 position [[position]];
  float2 texCoords;
  float4 color;
} Fragment_T;

typedef struct
{
  float4x4 u_projection;
} Uniforms_T;

vertex Fragment_T vsImGui(const Vertex_T in [[stage_in]],
                          constant Uniforms_T & uniforms [[buffer(1)]])
{
  Fragment_T out;
  out.position = float4(in.a_position, 0.0, 1.0) * uniforms.u_projection;
  out.texCoords = in.a_texCoords;
  out.color = in.a_color;
  return out;
}

fragment float4 fsImGui(const Fragment_T in [[stage_in]],
                        texture2d<float> u_colorTex [[texture(0)]],
                        sampler u_colorTexSampler [[sampler(0)]])
{
  return in.color * u_colorTex.sample(u_colorTexSampler, in.texCoords);
}
