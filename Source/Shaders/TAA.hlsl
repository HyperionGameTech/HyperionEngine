#include "include/Defines.hlsli"
#include "include/Shared.hlsli"
#include "include/Scene.hlsli"

DECLARE_SRV(TAA, InColorTexture) Texture2D color_texture;
DECLARE_SRV(TAA, InPrevColorTexture) Texture2D prev_color_texture;
DECLARE_SRV(TAA, InVelocityTexture) Texture2D velocity_texture;
DECLARE_SRV(TAA, InDepthTexture) Texture2D depth_texture;

DECLARE_SAMPLER(TAA, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(TAA, SamplerNearest) SamplerState sampler_nearest;

DECLARE_UAV(TAA, OutColorImage) RWTexture2D<float4> output_image;

DECLARE_BUFFER(TAA, TAAConstants) cbuffer TAAConstants
{
    uint2 dimensions;
    uint2 depth_texture_dimensions;
    float2 camera_near_far;
};

// #define ADJUST_COLOR_HDR

#include "include/Temporal.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 pixel_coord = dispatchThreadID.xy % dimensions;

    const float2 uv = (float2(pixel_coord) + 0.5) / float2(dimensions);

    const float2 texel_size = float2(1.0, 1.0) / float2(dimensions);
    const float2 depth_texel_size = float2(1.0, 1.0) / float2(depth_texture_dimensions);

    // const float3 closest_fragment = ClosestFragment(depth_texture, uv, depth_texel_size);
    // float2 velocity = SAMPLE_TEXTURE_2D(sampler_nearest, velocity_texture, closest_fragment.xy).rg;

    float2 velocity;
    float view_space_depth;

    InitTemporalParams(
        depth_texture,
        velocity_texture,
        depth_texture_dimensions,
        uv,
        camera_near_far.x,
        camera_near_far.y,
        velocity,
        view_space_depth);

    // float4 result = TemporalResolve(color_texture, prev_color_texture, uv, velocity, texel_size, view_space_depth);
    float4 result = TemporalBlendVarying(
        color_texture,
        prev_color_texture,
        uv,
        velocity,
        texel_size,
        view_space_depth);

    float4 color = SAMPLE_TEXTURE_2D(sampler_nearest, color_texture, uv);
    float4 previous_color = SAMPLE_TEXTURE_2D(sampler_nearest, prev_color_texture, uv - velocity);

#if 0

    float4 color_ycocg = AdjustColorIn(RGBToYCoCg(color));
    float4 previous_color_ycocg = RGBToYCoCg(previous_color);
    // previous_color = ClampColor_3x3(input_texture, previous_color, uv, texel_size);
    float4 mean = color_ycocg;
    float4 stddev = mean * mean;

    const float2 offsets[4] = {
        float2(-1.0, 0.0),
        float2(1.0, 0.0),
        float2(0.0, -1.0),
        float2(0.0, 1.0)
    };

    for (int i = 0; i < 4; i++) {
        float4 c = RGBToYCoCg(SAMPLE_TEXTURE_2D(sampler_nearest, color_texture, clamp(uv + (offsets[i] * texel_size), 0.0, 1.0)));
        mean += c;
        stddev += c * c;
    }

    mean /= 5.0;
    stddev = sqrt(stddev / 5.0 - (mean * mean));

    float4 clipped = ClipToAABB(color_ycocg, previous_color_ycocg, mean, stddev);
    // const float lum0 = RGBToYCoCg(color).r;
    // const float lum1 = clipped.r;
    float4 result = TemporalLuminanceResolve(color, YCoCgToRGB(clipped));

#endif

    // debugging
    result = any(isnan(result)) ? float4(0.0, 1.0, 0.0, 1.0) : result;

    const uint2 clamped_coord = clamp(pixel_coord, uint2(0, 0), dimensions - uint2(1, 1));

    output_image[clamped_coord] = result;
}
