/**
 *  Author: Andrew J. MacDonald
 *  Date: 2026/03/26
 **/

#ifndef CLUSTERED_SHADING_HLSLI
#define CLUSTERED_SHADING_HLSLI

#include "include/Defines.hlsli"

// Every shader that includes this declares STATIC(TILE_SIZE, 32) / STATIC(TILE_Z_BINS, 16), but a variant
// compiled from a request that lost those statics would otherwise fail to build. The defaults have to match
// the C++ cluster grid (Rendering/Passes/DeferredPass.cpp).
#ifndef TILE_SIZE
#define TILE_SIZE 32
#endif // TILE_SIZE

#ifndef TILE_Z_BINS
#define TILE_Z_BINS 16
#endif // TILE_Z_BINS

uint Cluster_LoadUInt16(uint index)
{
    uint alignedOffset = (index / 2) * sizeof(uint);
    uint raw32 = ClusterIndexBuffer.Load(alignedOffset);

    uint2 vals = { raw32 & 0xFFFFu, raw32 >> 16};
    
    // even index means we want the lower 16 bits, odd index means we want the upper 16 bits
    return vals[index & 1];
}

uint Cluster_LoadLightIndex(uint indexOffset, uint index)
{
    return Cluster_LoadUInt16(indexOffset + index);
}

uint Cluster_LoadEnvProbeIndex(uint indexOffset, uint numLights, uint index)
{
    return Cluster_LoadUInt16(indexOffset + numLights + index);
}

uint Cluster_CalculateZBin(float viewSpaceZ, float scale, float bias)
{
    const float z = max(viewSpaceZ, 0.0001);
    const int zBinInt = (int)(log2(z) * scale + bias);

    return (uint)clamp(zBinInt, 0, TILE_Z_BINS - 1);
}

uint Cluster_GetGridIndex(uint2 dimensions, uint2 pixelCoord, float viewSpaceZ, float cameraNear, float cameraFar)
{
    // Constants for logarithmic Z binning.
    const float scale = (float)TILE_Z_BINS / log2(cameraFar / cameraNear);
    const float bias = -log2(cameraNear) * scale;

    const uint zBin = Cluster_CalculateZBin(viewSpaceZ, scale, bias);

    const uint tilesX = (dimensions.x + TILE_SIZE - 1) / TILE_SIZE;
    const uint tilesY = (dimensions.y + TILE_SIZE - 1) / TILE_SIZE;

    const uint tileX = pixelCoord.x / TILE_SIZE;
    const uint tileY = pixelCoord.y / TILE_SIZE;

    return (zBin * tilesY + tileY) * tilesX + tileX;
}

#endif // CLUSTERED_SHADING_HLSLI
