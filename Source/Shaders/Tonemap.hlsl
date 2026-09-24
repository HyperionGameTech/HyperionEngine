#include "include/Defines.hlsli"
#include "include/Shared.hlsli"
#include "include/Scene.hlsli"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position = position.xyz;
    output.texcoord = input.a_texcoord0;
    output.position_cs = position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color_output : SV_Target0;
};

DECLARE_SRV(Tonemap, DeferredResult) Texture2D DeferredResult;
DECLARE_SRV(Tonemap, BloomResultTexture) Texture2D BloomResultTexture;
DECLARE_SRV(Tonemap, DebugOverlayTexture) Texture2D DebugOverlayTexture;

DECLARE_SAMPLER(Tonemap, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(Tonemap, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(Tonemap, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "include/Tonemap.hlsli"

float4 PSMain(PSInput input) : SV_Target0
{
    float2 texcoord = input.texcoord;

    float4 shaded_result = SAMPLE_TEXTURE_2D(sampler_linear, DeferredResult, texcoord);
    float4 bloom_result = SAMPLE_TEXTURE_2D(sampler_linear, BloomResultTexture, texcoord);

    float4 color_with_bloom = shaded_result + bloom_result;

    const float3 graded_color = ApplyColorGrading(color_with_bloom.rgb, world_shader_data);

    float4 color_output = float4(Tonemap(graded_color, world_shader_data.tonemap_operator), 1.0);

    // debug draws are premultiplied over a transparent clear, alpha being how much of the scene they cover
    const float4 debug_overlay = SAMPLE_TEXTURE_2D(sampler_nearest, DebugOverlayTexture, texcoord);
    color_output.rgb = saturate(debug_overlay.rgb) + color_output.rgb * (1.0 - saturate(debug_overlay.a));

#ifdef OUTPUT_PQ_HDR
    const float peakNits = 1000.0;
    color_output.rgb = LinearToPQ(color_output.rgb, peakNits);
#endif

    return color_output;
}

#endif // PIXEL_SHADER
