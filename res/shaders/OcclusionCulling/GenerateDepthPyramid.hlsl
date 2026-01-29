#include "../include/defines.inc"

#include "../include/shared.inc"
#include "./Shared.inc"

DECLARE_SRV(DepthPyramidDescriptorSet, InImage) Texture2D mip_in;
DECLARE_UAV(DepthPyramidDescriptorSet, OutImage) RWTexture2D<float4> mip_out;
DECLARE_SAMPLER(DepthPyramidDescriptorSet, DepthPyramidSampler) SamplerState depth_pyramid_sampler;
DECLARE_BUFFER(DepthPyramidDescriptorSet, UniformBuffer) cbuffer DepthPyramidUniforms
{
    uint2 mip_dimensions;
    uint2 prev_mip_dimensions;
    uint mip_level;
};

float GetDepthAtTexel(float2 texcoord, int2 offset)
{
    const int2 texel_coord = clamp(
        int2((texcoord * float2(prev_mip_dimensions)) + float2(offset)),
        int2(0, 0),
        int2(prev_mip_dimensions) - int2(1, 1)
    );

    return TEXEL_FETCH_2D_LOD(depth_pyramid_sampler, mip_in, texel_coord, 0).r;
}

[numthreads(32, 32, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 coord = dispatchThreadID.xy;

    if (any(coord > mip_dimensions - uint2(1, 1))) {
        return;
    }

    const float2 texcoord = float2(coord) / float2(mip_dimensions);

    float depth = 0.0;

    if (mip_level == 0)
    {
       depth = GetDepthAtTexel(texcoord, int2(0, 0));
    }
    else
    {
        for (int i = 0; i < HYP_NUM_DEPTH_PYRAMID_OFFSETS; i++)
        {
            depth = HYP_DEPTH_CMP(depth, GetDepthAtTexel(texcoord, depth_pyramid_offsets[i]));
        }
    }

    mip_out[coord] = depth.xxxx;
}
