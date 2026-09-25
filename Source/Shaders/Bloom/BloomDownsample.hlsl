#include "../include/Defines.hlsli"

#include "../include/Shared.hlsli"
#include "../include/Scene.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/Packing.hlsli"

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

DECLARE_SAMPLER(BloomDownsample, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(BloomDownsample, SamplerNearest) SamplerState sampler_nearest;

DECLARE_SRV(BloomDownsample, InputTexture) Texture2D InTexture;

struct DownsampleConstants
{
    uint2 srcDimension;
    float2 invDimension;
};

DECLARE_BUFFER_DYNAMIC(BloomDownsample, CBuffer) cbuffer CBuffer
{
    DownsampleConstants constants;
};


static const float2 s_offsets[13] =
{
    float2(0.0f, 0.0f),
    float2(-1.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, -1.0f),
    float2(0.0f, 1.0f),
    float2(-1.0f, -1.0f),
    float2(1.0f, -1.0f),
    float2(-1.0f, 1.0f),
    float2(1.0f, 1.0f),
    float2(-2.0f, 0.0f),
    float2(2.0f, 0.0f),
    float2(0.0f, -2.0f),
    float2(0.0f, 2.0f)
};

static const float s_weights[13] =
{
    0.5f,
    0.125f,
    0.125f,
    0.125f,
    0.125f,
    0.0625f,
    0.0625f,
    0.0625f,
    0.0625f,
    0.0625f,
    0.0625f,
    0.0625f,
    0.0625f
};

float4 PSMain(PSInput input) : SV_Target0
{
    const float2 srcTexelSize = constants.invDimension;
    const float2 srcPixelPos = input.texcoord * float2(constants.srcDimension);
    
    const float2 maxDim = max((float2) constants.srcDimension - 1, (float2) 0);

    float4 colorSum = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    [unroll]
    for (int i = 0; i < 13; ++i)
    {
        float2 samplePixel = clamp(srcPixelPos + s_offsets[i], (float2) 0, maxDim);
        
        float2 sampleUV = samplePixel * constants.invDimension;
        float4 sampleColor = SAMPLE_TEXTURE_2D_LOD(sampler_linear, InTexture, sampleUV, 0);

        colorSum += sampleColor * s_weights[i];
        weightSum += s_weights[i];
    }

    return colorSum / max(weightSum, 0.001f);
}

#endif // PIXEL_SHADER