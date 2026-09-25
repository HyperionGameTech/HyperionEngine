#include "include/Defines.hlsli"
#include "include/Shared.hlsli"
#include "include/Scene.hlsli"

DECLARE_SRV(TAA, InColorTexture) Texture2D color_texture;
DECLARE_SRV(TAA, InPrevColorTexture) Texture2D prev_color_texture;
DECLARE_SRV(TAA, InVelocityTexture) Texture2D velocity_texture;
DECLARE_SRV(TAA, InDepthTexture) Texture2D depth_texture;
DECLARE_SRV(TAA, InMaterialTexture) Texture2D<uint> material_texture;

DECLARE_SAMPLER(TAA, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(TAA, SamplerNearest) SamplerState sampler_nearest;

DECLARE_UAV(TAA, OutColorImage) RWTexture2D<float4> output_image;

DECLARE_BUFFER_DYNAMIC(TAA, TAAConstants) cbuffer TAAConstants
{
    uint4 dimensions; // zw = depth
    float4 jitter;
    float2 nearFarClip;
    float feedback; // Rendering.TAA.Feedback
    float cutoutFeedback; // Rendering.TAA.CutoutFeedback
};

#define FEEDBACK feedback

#define CUTOUT_VELOCITY_REJECTION_PIXELS 16.0

// #define ADJUST_COLOR_HDR

#include "include/Temporal.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= dimensions.x || dispatchThreadID.y >= dimensions.y)
    {
        return;
    }

    const uint2 colorDimensions = dimensions.xy;
    const uint2 depthDimensions = dimensions.zw;

    uint2 pixel_coord = dispatchThreadID.xy;

    const float2 uv = (float2(pixel_coord) + 0.5) / float2(colorDimensions);

    const float2 texel_size = float2(1.0, 1.0) / float2(colorDimensions);
    const float2 depth_texel_size = float2(1.0, 1.0) / float2(depthDimensions);

    const float3 closest_fragment = ClosestFragment(depth_texture, uv, depth_texel_size);
    float2 velocity = SAMPLE_TEXTURE_2D(sampler_nearest, velocity_texture, closest_fragment.xy).rg;

    float view_space_depth;

    InitTemporalParams(
        depth_texture,
        velocity_texture,
        depthDimensions,
        uv,
        nearFarClip.x,
        nearFarClip.y,
        velocity,
        view_space_depth);

    // the closest fragment is the nearest surface around the pixel, so dithered leaves still count where this pixel was discarded
    const uint2 closestTexel = min(uint2(closest_fragment.xy * float2(depthDimensions)), depthDimensions - 1);
    const bool isCutout = ((material_texture.Load(int3(closestTexel, 0)) >> 28u) & OBJECT_MASK_CUTOUT) != 0;

    float4 result;

    if (isCutout)
    {
        result = TemporalBlendDithered(
            color_texture,
            prev_color_texture,
            uv,
            velocity,
            texel_size,
            cutoutFeedback,
            CUTOUT_VELOCITY_REJECTION_PIXELS);
    }
    else
    {
        result = TemporalBlendVarying(
            color_texture,
            prev_color_texture,
            uv,
            velocity,
            texel_size,
            view_space_depth);
    }

    const uint2 clamped_coord = clamp(pixel_coord, uint2(0, 0), colorDimensions - uint2(1, 1));

    // cheeky nan check to prevent poisoning
    output_image[clamped_coord] = any(isnan(result)) ? float4(0.0, 1.0, 0.0, 1.0) : result;
}
