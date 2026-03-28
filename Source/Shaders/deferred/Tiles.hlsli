/**
 *  Author: Andrew J. MacDonald
 *  Date: 2026/03/26
 **/

#pragma once

#include "include/defines.inc"

float Linear01Depth(float depth, float near, float far);

#ifndef HYP_SHADER_COMPILER
#define TILE_SIZE 32
#define TILE_Z_BINS 16
#endif // HYP_SHADER_COMPILER

#ifndef TILE_SIZE
#error "TILE_SIZE must be defined before including Tiles.hlsli"
#endif

#ifndef TILE_Z_BINS
#error "TILE_Z_BINS must be defined before including Tiles.hlsli"
#endif

// x = index offset, y = lights, envprobe counts (uint16 each)
DECLARE_SRV_DYNAMIC(DeferredPass, ClusterGridBuffer) StructuredBuffer<uint2> ClusterGridBuffer;
// 16-bit ushort for indices
DECLARE_SRV_DYNAMIC(DeferredPass, ClusterIndexBuffer) ByteAddressBuffer ClusterIndexBuffer;

DECLARE_SRV(DeferredPass, LightsBuffer) StructuredBuffer<Light> LightsBuffer;
DECLARE_SRV(DeferredPass, EnvProbesBuffer) StructuredBuffer<EnvProbe> EnvProbesBuffer;

uint Cluster_LoadUInt16(uint index)
{
    uint dwordAlignedOffset = (index / 2) * /* sizeof(uint) */ 4;
    uint raw32 = ClusterIndexBuffer.Load(dwordAlignedOffset);
    
    // even index means we want the lower 16 bits, odd index means we want the upper 16 bits
    return select(index & 1, raw32 >> 16, raw32 & 0xFFFFu);
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
    const float z = max(viewSpaceZ, 0.0001); // avoid log of zero or negative
    const uint zBin = (uint)(log2(z) * scale + bias);
    
    return clamp(zBin, 0, TILE_Z_BINS - 1);
}

uint Cluster_GetGridIndex(uint2 dimensions, uint2 pixelCoord, float viewSpaceZ, float cameraNear, float cameraFar)
{
    // Constants for logarithmic Z binning.
    const float scale = TILE_Z_BINS / log2(cameraFar / cameraNear);
    const float bias = -log2(cameraNear) * scale;

    const uint zBin = Cluster_CalculateZBin(viewSpaceZ, scale, bias);
    
    const uint tilesX = (dimensions.x + TILE_SIZE - 1) / TILE_SIZE;
    const uint tilesY = (dimensions.y + TILE_SIZE - 1) / TILE_SIZE;

    const uint tileX = pixelCoord.x / TILE_SIZE;
    const uint tileY = pixelCoord.y / TILE_SIZE;
    
    return (zBin * tilesY + tileY) * tilesX + tileX;
}