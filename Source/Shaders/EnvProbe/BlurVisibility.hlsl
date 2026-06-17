#include "../include/Defines.hlsli"

PERMUTE(KERNEL_SIZE, 3, 5, 7, 9);

#define WORKGROUP_SIZE 8

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../include/Shared.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(BlurVisibility, InputTexture) TextureCube<float2> input_texture;

DECLARE_SAMPLER(BlurVisibility, SamplerLinear) SamplerState sampler_linear;

DECLARE_UAV(BlurVisibility, OutputTexture) RWTexture2DArray<float2> output_texture;

DECLARE_BUFFER_DYNAMIC(BlurVisibility, CBuffer) cbuffer CBuffer
{
    uint2 dimensions;
};

static const float Sigma = float(KERNEL_SIZE) * 0.25;

[numthreads(WORKGROUP_SIZE, WORKGROUP_SIZE, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID)
{
    if (any(dispatchThreadID.xy >= dimensions))
    {
        return;
    }

    const uint face = dispatchThreadID.z;
    const float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(dimensions);

    const float3 D = GetCubemapCoord(face, uv);

    const bool is_even_x = (groupThreadID.x & 1) == 0;
    const bool is_even_y = (groupThreadID.y & 1) == 0;

    const float3 D_neighbor_x = QuadReadAcrossX(D);
    const float3 D_neighbor_y = QuadReadAcrossY(D);

    const float3 dDdx = is_even_x ? (D_neighbor_x - D) : (D - D_neighbor_x);
    const float3 dDdy = is_even_y ? (D_neighbor_y - D) : (D - D_neighbor_y);

    const float3 T = dDdx - dot(dDdx, D) * D;
    const float3 B = dDdy - dot(dDdy, D) * D;

    const int half_kernel = KERNEL_SIZE / 2;

    float2 sum = 0.0f;
    float total_weight = 0.0f;

    for (int dy = -half_kernel; dy <= half_kernel; dy++)
    {
        for (int dx = -half_kernel; dx <= half_kernel; dx++)
        {
            const float3 N = normalize(D + float(dx) * T + float(dy) * B);

            const float2 sample_val = input_texture.SampleLevel(sampler_linear, N, 0);

            const float weight = exp(-0.5f * (float(dx * dx) + float(dy * dy)) / (Sigma * Sigma));

            sum += sample_val * weight;
            total_weight += weight;
        }
    }

    output_texture[uint3(dispatchThreadID.xy, face)] = sum / max(total_weight, 1e-8f);

    // // TEMP: passthrough
    // output_texture[uint3(dispatchThreadID.xy, face)] = input_texture.SampleLevel(sampler_linear, D, 0);
}
