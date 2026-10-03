#ifndef GLIMMER_SH_COMMON_HLSLI
#define GLIMMER_SH_COMMON_HLSLI

#include "../GlimmerCommon.hlsli"
#include "../GlimmerConstants.hlsli"

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

#define GLIMMER_SH_RADIANCE_CHANNEL_STRIDE (GLIMMER_SH_CASCADES * GLIMMER_SH_GRID_Y)

uint3 GlimmerSHRadianceTexel(uint3 texel, uint channel)
{
    return texel + uint3(0u, channel * GLIMMER_SH_RADIANCE_CHANNEL_STRIDE, 0u);
}

struct GlimmerSHRadiance
{
    float4 r;
    float4 g;
    float4 b;
};

float3 GlimmerSHVoxelCenter(GlimmerSHCascade cascade, int3 voxel)
{
    return (float3(voxel) + 0.5) * cascade.params.x;
}

struct GlimmerSHVoxel
{
    float3 originOffset; // in voxels
    float depths[GLIMMER_SH_AXIS_DEPTHS];
    bool isBuried;
    bool isAir;
};

#define GLIMMER_SH_STATE_TRACED 0x80000000u
#define GLIMMER_SH_STATE_BURIED 0x40000000u
#define GLIMMER_SH_STATE_AIR 0x80000000u

uint GlimmerSHWrapId(int3 voxel)
{
    const uint3 wrap = uint3(int3(voxel.x >> 5, voxel.y >> 4, voxel.z >> 5) & 63);

    return wrap.x | (wrap.y << 6) | (wrap.z << 12);
}

// x = unused
// y = origin offset (3 x 5 bit snorm of GLIMMER_SH_MAX_ORIGIN_OFFSET voxels, bits 16-30) | GLIMMER_SH_STATE_TRACED
// z = axis depths (6 x 5 bit unorm of GLIMMER_SH_DEPTH_RANGE voxels) | GLIMMER_SH_STATE_BURIED | GLIMMER_SH_STATE_AIR
// w = GlimmerSHWrapId
uint4 GlimmerSHPackVoxel(int3 voxel, GlimmerSHVoxel data)
{
    const int3 offset = int3(round(clamp(data.originOffset / GLIMMER_SH_MAX_ORIGIN_OFFSET, -1.0, 1.0) * 15.0));
    const uint packedOffset = (uint(offset.x) & 0x1Fu) | ((uint(offset.y) & 0x1Fu) << 5) | ((uint(offset.z) & 0x1Fu) << 10);

    uint packedDepths = 0u;

    [unroll]
    for (uint axisIndex = 0; axisIndex < GLIMMER_SH_AXIS_DEPTHS; axisIndex++)
    {
        packedDepths |= uint(round(saturate(data.depths[axisIndex] / GLIMMER_SH_DEPTH_RANGE) * 31.0)) << (axisIndex * 5u);
    }

    return uint4(
        0u,
        (packedOffset << 16) | GLIMMER_SH_STATE_TRACED,
        packedDepths | (data.isBuried ? GLIMMER_SH_STATE_BURIED : 0u) | (data.isAir ? GLIMMER_SH_STATE_AIR : 0u),
        GlimmerSHWrapId(voxel));
}

// false where the voxel hasn't been traced since it scrolled in
bool GlimmerSHUnpackVoxel(uint4 packed, int3 voxel, out GlimmerSHVoxel outData)
{
    outData = (GlimmerSHVoxel)0;

    if ((packed.y & GLIMMER_SH_STATE_TRACED) == 0u || packed.w != GlimmerSHWrapId(voxel))
    {
        return false;
    }

    const uint packedOffset = (packed.y >> 16) & 0x7FFFu;
    const int3 offset = int3(int(packedOffset << 27) >> 27, int(packedOffset << 22) >> 27, int(packedOffset << 17) >> 27);

    outData.originOffset = float3(offset) / 15.0 * GLIMMER_SH_MAX_ORIGIN_OFFSET;

    [unroll]
    for (uint axisIndex = 0; axisIndex < GLIMMER_SH_AXIS_DEPTHS; axisIndex++)
    {
        outData.depths[axisIndex] = float((packed.z >> (axisIndex * 5u)) & 0x1Fu) / 31.0 * GLIMMER_SH_DEPTH_RANGE;
    }

    outData.isBuried = (packed.z & GLIMMER_SH_STATE_BURIED) != 0u;
    outData.isAir = (packed.z & GLIMMER_SH_STATE_AIR) != 0u;

    return true;
}

#endif
