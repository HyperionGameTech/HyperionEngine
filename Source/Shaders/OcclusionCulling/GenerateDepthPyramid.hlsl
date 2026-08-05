#include "../include/Defines.hlsli"

#include "../include/Shared.hlsli"
#include "./Shared.hlsli"

DECLARE_SRV(DepthPyramidDescriptorSet, InImage) Texture2D InImage;
DECLARE_UAV(DepthPyramidDescriptorSet, OutImage) RWTexture2D<float2> OutImage;
DECLARE_SAMPLER(DepthPyramidDescriptorSet, DepthPyramidSampler) SamplerState InSampler;
DECLARE_BUFFER_DYNAMIC(DepthPyramidDescriptorSet, CBuffer) cbuffer CBuffer
{
    uint2 mip_dimensions;
    uint2 prev_mip_dimensions;
    uint mip_level;
};

float2 GetDepthAtTexel(int2 texel_coord)
{
    texel_coord = clamp(texel_coord, int2(0, 0), int2(prev_mip_dimensions) - 1);

    return TEXEL_FETCH_2D_LOD(InSampler, InImage, texel_coord, 0).rg;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 coord = dispatchThreadID.xy;

    if (any(coord >= mip_dimensions))
    {
        return;
    }

    float2 depths = HYP_DEPTHS_INIT;

    if (mip_level == 0)
    {
        depths = GetDepthAtTexel(int2(coord));
        depths.y = depths.x;
    }
    else
    {
        const int2 srcMin = int2(coord) * 2;
        int2 srcMax = srcMin + int2(1, 1);

        if (coord.x == mip_dimensions.x - 1)
        {
            srcMax.x = int(prev_mip_dimensions.x) - 1;
        }

        if (coord.y == mip_dimensions.y - 1)
        {
            srcMax.y = int(prev_mip_dimensions.y) - 1;
        }

        for (int y = srcMin.y; y <= srcMax.y; y++)
        {
            for (int x = srcMin.x; x <= srcMax.x; x++)
            {
                const float2 d = GetDepthAtTexel(int2(x, y));
                depths = float2(HYP_DEPTH_CMP(depths.x, d.x), HYP_DEPTH_CMP_INV(depths.y, d.y));
            }
        }
    }

    OutImage[coord] = depths;
}
