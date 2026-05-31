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

DECLARE_SAMPLER(BloomBlur, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(BloomBlur, SamplerNearest) SamplerState sampler_nearest;

DECLARE_SRV(BloomBlur, InputTexture) Texture2D InTexture;

DECLARE_BUFFER_DYNAMIC(BloomBlur, CBuffer) cbuffer CBuffer
{
    float2 texelSize;
    float2 padding;
};

float4 PSMain(PSInput input) : SV_Target0
{
    float4 colorSum = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    static const float Weights[5] = { 0.0545f, 0.2442f, 0.4026f, 0.2442f, 0.0545f };
    static const int Radius = 2;

    for (int x = -Radius; x <= Radius; ++x)
    {
        for (int y = -Radius; y <= Radius; ++y)
        {
            float2 offsetUV = input.texcoord + float2(x, y) * texelSize;

            if (any(offsetUV < float2(0.0f, 0.0f)) || any(offsetUV > float2(1.0f, 1.0f)))
            {
                continue;
            }

            float weight = Weights[x + Radius] * Weights[y + Radius];
            float4 sampleColor = SAMPLE_TEXTURE_2D(sampler_linear, InTexture, offsetUV);

            colorSum += sampleColor * weight;
            weightSum += weight;
        }
    }

    return colorSum / max(weightSum, 0.001f);
}

#endif // PIXEL_SHADER