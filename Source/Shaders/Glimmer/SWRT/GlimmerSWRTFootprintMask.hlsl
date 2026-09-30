#include "../../Include/Defines.hlsli"

#include "GlimmerSWRTCommon.hlsli"

PERMUTE(MODE, CLEAR, RASTERIZE, REDUCE)

struct GlimmerFootprintMaskConstants
{
    GlimmerFootprintMaskParams mask;
    uint4 passInfo; // x = number of instances, y = target level (REDUCE), z = total cells (CLEAR), w = groups along x
};

DECLARE_BUFFER_DYNAMIC(GlimmerFootprintMask, CBuffer) cbuffer CBuffer
{
    GlimmerFootprintMaskConstants constants;
};

DECLARE_UAV(GlimmerFootprintMask, FootprintMaskBuffer) RWStructuredBuffer<uint> footprintMask;
DECLARE_SRV(GlimmerFootprintMask, GlimmerInstanceBoundsBuffer) StructuredBuffer<GlimmerInstanceBounds> glimmerInstanceBounds;

#define GROUP_SIZE 64

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint flatGroupIndex = groupId.y * constants.passInfo.w + groupId.x;

#if defined(MODE_CLEAR)
    const uint cellIndex = flatGroupIndex * GROUP_SIZE + groupIndex;

    if (cellIndex >= constants.passInfo.z)
    {
        return;
    }

    footprintMask[cellIndex * 2u] = GLIMMER_MASK_EMPTY_MIN;
    footprintMask[cellIndex * 2u + 1u] = GLIMMER_MASK_EMPTY_MAX;
#elif defined(MODE_RASTERIZE)
    // one group per instance, the group's threads split its footprint
    const uint instanceIndex = flatGroupIndex;

    if (instanceIndex >= constants.passInfo.x)
    {
        return;
    }

    const GlimmerInstanceBounds bounds = glimmerInstanceBounds[instanceIndex];

    const float cellSize = constants.mask.originCellSize.z;
    const int resolution = int(constants.mask.info.x);

    const int2 minCell = max(int2(floor((bounds.boundsMin.xz - constants.mask.originCellSize.xy) / cellSize)), int2(0, 0));
    const int2 maxCell = min(int2(floor((bounds.boundsMax.xz - constants.mask.originCellSize.xy) / cellSize)), int2(resolution - 1, resolution - 1));

    if (any(maxCell < minCell))
    {
        return;
    }

    const uint2 extent = uint2(maxCell - minCell) + 1u;
    const uint numCells = extent.x * extent.y;

    const uint encodedMinY = GlimmerOrderedUintFromFloat(bounds.boundsMin.y);
    const uint encodedMaxY = GlimmerOrderedUintFromFloat(bounds.boundsMax.y);

    for (uint localIndex = groupIndex; localIndex < numCells; localIndex += GROUP_SIZE)
    {
        const uint2 cell = uint2(minCell) + uint2(localIndex % extent.x, localIndex / extent.x);
        const uint index = GlimmerMaskCellIndex(constants.mask, 0u, cell);

        uint previous;
        InterlockedMin(footprintMask[index], encodedMinY, previous);
        InterlockedMax(footprintMask[index + 1u], encodedMaxY, previous);
    }
#elif defined(MODE_REDUCE)
    const uint level = constants.passInfo.y;
    const uint resolution = GlimmerMaskLevelResolution(constants.mask, level);
    const uint sourceResolution = GlimmerMaskLevelResolution(constants.mask, level - 1u);

    const uint cellIndex = flatGroupIndex * GROUP_SIZE + groupIndex;

    if (cellIndex >= resolution * resolution)
    {
        return;
    }

    const uint2 cell = uint2(cellIndex % resolution, cellIndex / resolution);

    uint minY = GLIMMER_MASK_EMPTY_MIN;
    uint maxY = GLIMMER_MASK_EMPTY_MAX;

    [unroll]
    for (uint offsetIndex = 0; offsetIndex < 4; offsetIndex++)
    {
        const uint2 sourceCell = min(cell * 2u + uint2(offsetIndex & 1u, offsetIndex >> 1u), uint2(sourceResolution - 1u, sourceResolution - 1u));
        const uint sourceIndex = GlimmerMaskCellIndex(constants.mask, level - 1u, sourceCell);

        minY = min(minY, footprintMask[sourceIndex]);
        maxY = max(maxY, footprintMask[sourceIndex + 1u]);
    }

    const uint index = GlimmerMaskCellIndex(constants.mask, level, cell);

    footprintMask[index] = minY;
    footprintMask[index + 1u] = maxY;
#endif
}
