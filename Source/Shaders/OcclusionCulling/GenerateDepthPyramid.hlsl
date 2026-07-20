#include "../include/Defines.hlsli"

#include "../include/Shared.hlsli"
#include "./Shared.hlsli"

DECLARE_SRV(DepthPyramidDescriptorSet, InImage) Texture2D InImage;
DECLARE_UAV(DepthPyramidDescriptorSet, OutImage) RWTexture2D<float2> OutImage;
DECLARE_SAMPLER(DepthPyramidDescriptorSet, DepthPyramidSampler) SamplerState InSampler;
DECLARE_BUFFER(DepthPyramidDescriptorSet, CBuffer) cbuffer CBuffer
{
    uint2 mip_dimensions;
    uint2 prev_mip_dimensions;
    uint mip_level;
};

float2 GetDepthAtTexel(float2 texcoord, int2 offset)
{
    const int2 texel_coord = clamp(
        int2((texcoord * float2(prev_mip_dimensions)) + float2(offset)),
        int2(0, 0),
        int2(prev_mip_dimensions) - int2(1, 1)
    );

    return TEXEL_FETCH_2D_LOD(InSampler, InImage, texel_coord, 0).rg;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 coord = dispatchThreadID.xy;

    if (any(coord > mip_dimensions - uint2(1, 1))) {
        return;
    }

    const float2 texcoord = (float2(coord)) / float2(mip_dimensions);

    float2 depths = HYP_DEPTHS_INIT;

    if (mip_level == 0)
    {
       depths = GetDepthAtTexel(texcoord, int2(0, 0));
    }
    else
    {
        for (int i = 0; i < HYP_NUM_DEPTH_PYRAMID_OFFSETS; i++)
        {
            float2 d = GetDepthAtTexel(texcoord, depth_pyramid_offsets[i]);
            depths = float2(HYP_DEPTH_CMP(depths.x, d.x), HYP_DEPTH_CMP_INV(depths.y, d.y));
        }
    }

    OutImage[coord] = depths;
}
