#include "../include/Defines.hlsli"

PERMUTE(NUM_SAMPLES, 4096);
PERMUTE(LOBE_SIZE, 0.0, 0.143, 0.286, 0.429, 0.571, 0.714, 0.857, 1.0);

#define WORKGROUP_SIZE 8

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../include/Shared.hlsli"
#include "../include/Noise.hlsli"
#include "../include/Packing.hlsli"
#include "../include/Scene.hlsli"
#include "../include/BRDF.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/EnvProbes.hlsli"

DECLARE_SRV(ConvolveProbe, ColorTexture) TextureCube color_texture;

DECLARE_SAMPLER(ConvolveProbe, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(ConvolveProbe, SamplerNearest) SamplerState sampler_nearest;

DECLARE_UAV(ConvolveProbe, OutImage) RWTexture2DArray<float4> out_image;

DECLARE_BUFFER(ConvolveProbe, UniformBuffer) cbuffer UniformBuffer
{
    uint2 out_image_dimensions;
    uint2 in_image_dimensions;
};

DECLARE_SRV(ConvolveProbe, SphereSamplesBuffer) StructuredBuffer<float4> SphereSamplesBuffer;

float4 ConvolveProbe(uint2 local_coord, uint face)
{
#if NUM_SAMPLES <= 0
    return float4(0.0, 0.0, 0.0, 0.0);
#endif

    const int num_samples = NUM_SAMPLES;

    const float3 N = normalize(GetCubemapCoord(face, (float2(local_coord) + 0.5) / float2(out_image_dimensions)));

    float3 R = N;
    float3 V = R;

    const float roughness = LOBE_SIZE;

    float omegaP = 4.0 * HYP_FMATH_PI / float(6 * in_image_dimensions.x * in_image_dimensions.y);

    float4 sum_radiance = float4(0.0, 0.0, 0.0, 0.0);
    float total_weight = 0.0;

    uint seed = uint(local_coord.x) * uint(local_coord.y);

    for (int i = 0; i < num_samples; i++)
    {
        vec2 Xi = Hammersley(uint(i), uint(num_samples));
        vec3 H = SampleGGX(Xi, roughness, N);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = saturate(dot(N, L));

        if (NdotL > 0.0)
        {
            float NdotH = saturate(dot(N, H));
            float HdotV = saturate(dot(H, V));

            float pdf = GGX_PDF(NdotH, HdotV, roughness);
            float omegaS = 1.0 / (float(num_samples) * max(pdf, 0.0001));
            float mipLevel = roughness == 0.0 ? 0.0 : max(0.5 * log2(omegaS / omegaP), 0.0);

            float4 sample_color = color_texture.SampleLevel(sampler_linear, L, mipLevel);

            sum_radiance += sample_color * NdotL;
            total_weight += NdotL;
        }
    }

    if (total_weight > 0.0)
    {
        sum_radiance /= total_weight;
    }

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
