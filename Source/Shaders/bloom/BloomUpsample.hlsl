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

DECLARE_SAMPLER(BloomUpsample, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(BloomUpsample, SamplerNearest) SamplerState sampler_nearest;

DECLARE_SRV(BloomUpsample, PrevPassTexture) Texture2D PrevPassTexture;
DECLARE_SRV(BloomUpsample, InputTexture) Texture2D InTexture;

DECLARE_BUFFER_DYNAMIC(BloomUpsample, CBuffer) cbuffer CBuffer
{
    float2 texelSize;
    float scatter; // blend factor between coarser and finer mip (0.5 = equal weight)
    float padding;
};

float4 SampleWithTentFilter(Texture2D tex, SamplerState sampler, float2 uv, float2 texelSize)
{
    // 3x3 tent (bilinear) filter. Weights sum to 1, so no normalisation needed.
    // Luminance-weighted (Karis) sampling is intentionally NOT used here — it
    // belongs only on the first downsample pass. Applying it during upsample
    // would bias the blend toward bright areas and produce uneven bloom energy.
    static const float TentWeights[9] =
    {
        0.0625f, 0.125f, 0.0625f,
        0.125f,  0.25f,  0.125f,
        0.0625f, 0.125f, 0.0625f
    };

    float4 colorSum = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    int idx = 0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float2 offsetUV = uv + float2(x, y) * texelSize;

            if (any(offsetUV < float2(0.0f, 0.0f)) || any(offsetUV > float2(1.0f, 1.0f)))
            {
                idx++;
                continue;
            }

            float weight = TentWeights[idx];
            colorSum += SAMPLE_TEXTURE_2D(sampler, tex, offsetUV) * weight;
            weightSum += weight;
            idx++;
        }
    }

    // Normalise so skipped out-of-bounds taps at image edges don't dim the result
    return colorSum / max(weightSum, 0.0001f);
}

float4 PSMain(PSInput input) : SV_Target0
{
    float2 uv = input.texcoord;

    float2 upsampleTexelSize = texelSize * 2.0f;
    float4 upsampledColor = SampleWithTentFilter(PrevPassTexture, sampler_linear, uv, upsampleTexelSize);
    float4 blendColor = SAMPLE_TEXTURE_2D(sampler_linear, InTexture, uv);

    // Lerp between the current mip (fine detail) and the accumulated upsampled result
    // (coarser, wider spread). scatter=0.5 gives equal weight to each pyramid level,
    // producing a natural falloff rather than a uniform halo.
    return lerp(blendColor, upsampledColor, scatter);
}

#endif // PIXEL_SHADER