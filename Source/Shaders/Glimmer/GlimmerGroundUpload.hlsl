#include "../Include/Defines.hlsli"

#include "GlimmerCommon.hlsli"

struct GlimmerGroundUploadConstants
{
    int4 texelMinExtent; // xy = absolute texel min, zw = extent
    uint4 info;          // x = level, y = offset into the heights buffer, z = groups along x
};

DECLARE_BUFFER_DYNAMIC(GlimmerGroundUpload, CBuffer) cbuffer CBuffer
{
    GlimmerGroundUploadConstants constants;
};

DECLARE_SRV(GlimmerGroundUpload, HeightsBuffer) StructuredBuffer<float> heights;
DECLARE_UAV(GlimmerGroundUpload, OutGround) RWTexture2DArray<float> OutGround;

[numthreads(64, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint texelIndex = (groupId.y * constants.info.z + groupId.x) * 64u + groupIndex;
    const uint2 extent = uint2(constants.texelMinExtent.zw);

    if (texelIndex >= extent.x * extent.y)
    {
        return;
    }

    const int2 texel = constants.texelMinExtent.xy + int2(texelIndex % extent.x, texelIndex / extent.x);

    OutGround[uint3(GlimmerWrapGroundTexel(texel), constants.info.x)] = heights[constants.info.y + texelIndex];
}
