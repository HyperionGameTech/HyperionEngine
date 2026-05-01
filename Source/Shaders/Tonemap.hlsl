#include "include/Defines.hlsli"
#include "include/Shared.hlsli"

PERMUTE(EXPOSURE, 1.0, 1.1, 1.2, 1.4, 1.6, 1.8, 2.0);

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

DECLARE_SAMPLER(Tonemap, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(Tonemap, SamplerLinear) SamplerState sampler_linear;

#include "include/Tonemap.hlsli"

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = input.texcoord;

    float4 shaded_result = SAMPLE_TEXTURE_2D(sampler_linear, DeferredResult, texcoord);
    float4 bloom_result = SAMPLE_TEXTURE_2D(sampler_linear, BloomResultTexture, texcoord);

    float4 color_with_bloom = shaded_result + bloom_result;

    float4 color_output = float4(Tonemap(color_with_bloom.rgb), 1.0);

#ifdef OUTPUT_PQ_HDR
    const float peakNits = 1000.0;
    color_output.rgb = LinearToPQ(color_output.rgb, peakNits);
#endif

    output.color_output = color_output;

    return output;
}

#endif // PIXEL_SHADER
