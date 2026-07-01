#include "include/Defines.hlsli"
#include "include/Shared.hlsli"
#include "include/Scene.hlsli"

STATIC(FEEDBACK, 0.8)

DECLARE_SRV(TAA, InColorTexture) Texture2D color_texture;
DECLARE_SRV(TAA, InPrevColorTexture) Texture2D prev_color_texture;
DECLARE_SRV(TAA, InVelocityTexture) Texture2D velocity_texture;
DECLARE_SRV(TAA, InDepthTexture) Texture2D depth_texture;

DECLARE_SAMPLER(TAA, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(TAA, SamplerNearest) SamplerState sampler_nearest;

DECLARE_UAV(TAA, OutColorImage) RWTexture2D<float4> output_image;

DECLARE_BUFFER_DYNAMIC(TAA, TAAConstants) cbuffer TAAConstants
{
    uint4 dimensions; // zw = depth
    float4 jitter;
    float2 nearFarClip;
};

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

    float4 result = TemporalBlendVarying(
        color_texture,
        prev_color_texture,
        uv,
        velocity,
        texel_size,
        view_space_depth);

    const uint2 clamped_coord = clamp(pixel_coord, uint2(0, 0), colorDimensions - uint2(1, 1));

    output_image[clamped_coord] = result;
}
