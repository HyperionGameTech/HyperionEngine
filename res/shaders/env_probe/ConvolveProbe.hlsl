#include "../include/defines.inc"

#define WORKGROUP_SIZE 8

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../include/shared.inc"
#include "../include/noise.inc"
#include "../include/packing.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/env_probe.inc"

DECLARE_SRV(ConvolveProbe, ColorTexture) TextureCube color_texture;

DECLARE_SAMPLER(ConvolveProbe, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(ConvolveProbe, SamplerNearest) SamplerState sampler_nearest;

DECLARE_UAV(ConvolveProbe, OutImage) RWTexture2DArray<float4> out_image;

DECLARE_BUFFER(ConvolveProbe, UniformBuffer) cbuffer UniformBuffer
{
    uint2 out_image_dimensions;
    uint2 in_image_dimensions;
    float4 world_position;
};

DECLARE_SRV(ConvolveProbe, SphereSamplesBuffer) StructuredBuffer<float4> SphereSamplesBuffer;

float4 ConvolveProbe(uint2 local_coord, uint face)
{
#if NUM_SAMPLES <= 0
    return float4(0.0, 0.0, 0.0, 0.0);
#endif

    const int num_samples = NUM_SAMPLES;
    const float lobe_size = LOBE_SIZE;

    const float3 dir = normalize(GetCubemapCoord(face, (float2(local_coord) + 0.5) / float2(out_image_dimensions)));

    float4 sum_radiance = float4(0.0, 0.0, 0.0, 0.0);

    uint seed = uint(local_coord.x) * uint(local_coord.y);

    for (int i = 0; i < num_samples; i++)
    {
        float3 offset = SphereSamplesBuffer[i % 4096].xyz;
        float3 sample_dir = normalize(dir + float3(lobe_size, lobe_size, lobe_size) * offset);

        float4 color = SAMPLE_TEXTURE_CUBE(sampler_linear, color_texture, sample_dir);

        sum_radiance += color;
    }

    sum_radiance /= float(num_samples);

    return sum_radiance;
}

[numthreads(WORKGROUP_SIZE, WORKGROUP_SIZE, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (any(dispatchThreadID.xy >= out_image_dimensions))
    {
        return;
    }

    const uint2 coord = dispatchThreadID.xy;
    float2 uv = (float2(coord) + 0.5) / float2(out_image_dimensions);

    float4 color = ConvolveProbe(coord, dispatchThreadID.z);

    out_image[uint3(coord, dispatchThreadID.z)] = color;
}
