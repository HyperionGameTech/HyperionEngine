#ifndef GLIMMER_SWRT_COMMON_HLSLI
#define GLIMMER_SWRT_COMMON_HLSLI

#include "../GlimmerCommon.hlsli"

// Must match GlimmerInstanceShaderData in GlimmerTLAS.hpp
struct GlimmerInstance
{
    float4 worldToObject0;
    float4 worldToObject1;
    float4 worldToObject2;
    uint4 data; // x = BLAS node base, y = BLAS triangle base, z = material index, w = flags
};

// Must match GlimmerInstanceBoundsShaderData in GlimmerTLAS.hpp
struct GlimmerInstanceBounds
{
    float4 boundsMin;
    float4 boundsMax;
};

// Must match GlimmerFootprintMaskShaderData in GlimmerFootprintMask.hpp
struct GlimmerFootprintMaskParams
{
    float4 originCellSize; // xy = world xz of cell (0, 0)'s corner, z = cell size, w = 1 when the mask is valid
    uint4 info;            // x = resolution of level 0, y = number of levels
};

uint GlimmerMaskLevelResolution(GlimmerFootprintMaskParams params, uint level)
{
    return max(params.info.x >> level, 1u);
}

uint GlimmerMaskLevelOffset(GlimmerFootprintMaskParams params, uint level)
{
    uint offset = 0;

    for (uint levelIndex = 0; levelIndex < level; levelIndex++)
    {
        const uint resolution = GlimmerMaskLevelResolution(params, levelIndex);
        offset += resolution * resolution;
    }

    return offset;
}

// Index of the cell's minY; maxY follows it
uint GlimmerMaskCellIndex(GlimmerFootprintMaskParams params, uint level, uint2 cell)
{
    const uint resolution = GlimmerMaskLevelResolution(params, level);

    return (GlimmerMaskLevelOffset(params, level) + cell.y * resolution + cell.x) * 2u;
}

float GlimmerMaskLevelCellSize(GlimmerFootprintMaskParams params, uint level)
{
    return params.originCellSize.z * float(1u << level);
}

// Returns false outside the mask
bool GlimmerMaskWorldToCell(GlimmerFootprintMaskParams params, uint level, float2 worldXZ, out uint2 outCell)
{
    const float2 cellCoord = (worldXZ - params.originCellSize.xy) / GlimmerMaskLevelCellSize(params, level);
    const float resolution = float(GlimmerMaskLevelResolution(params, level));

    outCell = uint2(clamp(cellCoord, 0.0, resolution - 1.0));

    return all(cellCoord >= 0.0) && all(cellCoord < resolution);
}

#endif
