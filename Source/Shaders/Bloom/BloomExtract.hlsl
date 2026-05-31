#include "../include/Defines.hlsli"

PERMUTE(HALFRES);

struct BloomConstants
{
    uint2 dimension;
    float threshold;
    float intensity;
    float softKnee;
};

DECLARE_UAV(BloomExtract, OutImage) RWTexture2D<float4> out_image;

DECLARE_BUFFER_DYNAMIC(BloomExtract, CBuffer) cbuffer CBuffer
{
    BloomConstants bloomConstants;
};

DECLARE_SRV(BloomExtract, DeferredShadingTexture) Texture2D DeferredShadingTexture;

DECLARE_SAMPLER(BloomExtract, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(BloomExtract, SamplerNearest) SamplerState sampler_nearest;

#include "../include/Shared.hlsli"
#include "../include/Scene.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/Packing.hlsli"

static const float2 LUMINANCE_WEIGHTS = float2(0.2126f, 0.7152f);

// Karis average weight: 1/(1+luma) down-weights fireflies so they can't blow out bloom.
float KarisWeight(float4 c)
{
    float luma = dot(c.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    return 1.0f / (1.0f + max(luma, 1e-5f));
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 coord = dispatchThreadID.xy;

    if (any(coord >= bloomConstants.dimension.xy))
    {
        return;
    }

    // 4-tap 2x2 box filter centred on the pixel, offset by ±0.5 texel so each
    // tap lands between four source pixels and the GPU hardware bilinear filter
    // gives us a 4x4 effective footprint. This eliminates the sub-pixel aliasing
    // that causes flickering when the camera moves.
    const float2 uv = (float2(coord) + 0.5f) / float2(bloomConstants.dimension.xy);
    const float2 halfTexel = 0.5f / float2(bloomConstants.dimension.xy);

    float4 s0 = SAMPLE_TEXTURE_2D(sampler_linear, DeferredShadingTexture, uv + float2(-halfTexel.x, -halfTexel.y));
    float4 s1 = SAMPLE_TEXTURE_2D(sampler_linear, DeferredShadingTexture, uv + float2( halfTexel.x, -halfTexel.y));
    float4 s2 = SAMPLE_TEXTURE_2D(sampler_linear, DeferredShadingTexture, uv + float2(-halfTexel.x,  halfTexel.y));
    float4 s3 = SAMPLE_TEXTURE_2D(sampler_linear, DeferredShadingTexture, uv + float2( halfTexel.x,  halfTexel.y));

    // Karis-weighted average suppresses firefly bright spots before thresholding.
    float w0 = KarisWeight(s0);
    float w1 = KarisWeight(s1);
    float w2 = KarisWeight(s2);
    float w3 = KarisWeight(s3);
    float4 color = (s0 * w0 + s1 * w1 + s2 * w2 + s3 * w3) / (w0 + w1 + w2 + w3);

    float luminance = dot(color.rgb, float3(LUMINANCE_WEIGHTS.x, LUMINANCE_WEIGHTS.y, 1.0f - LUMINANCE_WEIGHTS.x - LUMINANCE_WEIGHTS.y));

    // Soft-knee threshold (UE4-style)
    float knee = bloomConstants.threshold * bloomConstants.softKnee;
    float b = clamp(luminance - bloomConstants.threshold + knee, 0.0f, 2.0f * knee);
    float contribution = (knee > 0.0f) ? (0.25f * b * b / knee) : max(0.0f, luminance - bloomConstants.threshold);
    contribution = max(contribution, luminance - bloomConstants.threshold);

    out_image[coord] = color * (contribution / max(luminance, 1e-5f)) * bloomConstants.intensity;
    
    // YUCKY YUCKY YUCK! nans showing up! TODO: track down root cause and fix instead of this band-aid
    if (any(isnan(out_image[coord])))
    {
        out_image[coord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}