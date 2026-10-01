#ifndef GLIMMER_SH_COMMON_HLSLI
#define GLIMMER_SH_COMMON_HLSLI

#include "../GlimmerCommon.hlsli"

#define GLIMMER_SH_CASCADES 5
#define GLIMMER_SH_GRID_XZ 32
#define GLIMMER_SH_GRID_Y 16

#define GLIMMER_SH_VISIBILITY_RES 8
#define GLIMMER_SH_VISIBILITY_TEXELS (GLIMMER_SH_VISIBILITY_RES * GLIMMER_SH_VISIBILITY_RES)

#define GLIMMER_SH_SLAB_VISIBILITY 0u
#define GLIMMER_SH_SLAB_BOUNCE 1u
#define GLIMMER_SH_SLAB_DEPTH 2u // + depth map texel / 2
#define GLIMMER_SH_SLABS (2u + GLIMMER_SH_VISIBILITY_TEXELS / 2u)

#define GLIMMER_SH_DEPTH_RANGE 2.0

struct GlimmerSHCascade
{
    int4 origin;   // xyz = absolute voxel of the window's first voxel, w = 1 once the cascade has a window
    float4 params; // x = voxel spacing, y = 1 / spacing, z = how much visibility counts
};

struct GlimmerSHVolume
{
    GlimmerSHCascade cascades[GLIMMER_SH_CASCADES];
    uint4 info; // x = number of cascades, w = 1 when anything has been traced
};

uint3 GlimmerSHTexel(uint cascadeIndex, int3 voxel)
{
    return uint3(
        uint(voxel.x & (GLIMMER_SH_GRID_XZ - 1)),
        cascadeIndex * GLIMMER_SH_GRID_Y + uint(voxel.y & (GLIMMER_SH_GRID_Y - 1)),
        uint(voxel.z & (GLIMMER_SH_GRID_XZ - 1)));
}

uint3 GlimmerSHSlabTexel(uint3 texel, uint slab)
{
    return uint3(texel.xy, slab * GLIMMER_SH_GRID_XZ + texel.z);
}

float3 GlimmerSHVoxelCenter(GlimmerSHCascade cascade, int3 voxel)
{
    return (float3(voxel) + 0.5) * cascade.params.x;
}

// how far in voxels, per axis the rays' origin can sit off the voxel's center
#define GLIMMER_SH_MAX_ORIGIN_OFFSET 0.5

// x = voxel x and z (16 bits each)
// y = voxel y (bits 0-15) | traced (bit 16, so never written (zeroed) state can't match voxel (0, 0, 0)) | the rays' origin off the voxel's center (bits 17-31, 3 x 5 bit snorm of GLIMMER_SH_MAX_ORIGIN_OFFSET voxels)
uint2 GlimmerSHPackState(int3 voxel, float3 originOffset)
{
    const int3 quantized = int3(round(clamp(originOffset / GLIMMER_SH_MAX_ORIGIN_OFFSET, -1.0, 1.0) * 15.0));
    const uint packedOffset = (uint(quantized.x) & 0x1Fu) | ((uint(quantized.y) & 0x1Fu) << 5) | ((uint(quantized.z) & 0x1Fu) << 10);

    return uint2((uint(voxel.x) & 0xFFFFu) | ((uint(voxel.z) & 0xFFFFu) << 16), (uint(voxel.y) & 0xFFFFu) | 0x10000u | (packedOffset << 17));
}

bool GlimmerSHStateMatches(uint2 state, int3 voxel)
{
    const uint2 expected = GlimmerSHPackState(voxel, (float3)0.0);

    return state.x == expected.x && (state.y & 0x1FFFFu) == expected.y;
}

// in voxels
float3 GlimmerSHStateOffset(uint2 state)
{
    const uint packedOffset = state.y >> 17;

    // sign extend each 5 bit field
    const int3 quantized = int3(int(packedOffset << 27) >> 27, int(packedOffset << 22) >> 27, int(packedOffset << 17) >> 27);

    return float3(quantized) / 15.0 * GLIMMER_SH_MAX_ORIGIN_OFFSET;
}

#endif
