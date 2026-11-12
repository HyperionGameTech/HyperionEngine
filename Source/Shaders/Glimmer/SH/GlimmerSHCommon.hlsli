#ifndef GLIMMER_SH_COMMON_HLSLI
#define GLIMMER_SH_COMMON_HLSLI

#include "../GlimmerCommon.hlsli"
#include "../GlimmerConstants.hlsli"
#include "../GlimmerLight.hlsli"

#define GLIMMER_SH_RADIANCE_SECTIONS 3
#define GLIMMER_SH_RADIANCE_COLUMNS 3
#define GLIMMER_SH_RADIANCE_ROW_STRIDE (GLIMMER_SH_CASCADES * GLIMMER_SH_GRID_Y)

#define GLIMMER_SH_LIGHT_WIDE_ENTRIES GLIMMER_LIGHT_BOUNCE_SCALARS_ENTRY

// per voxel: the light entries, then its sun bounce (GLIMMER_SUN_BOUNCE_UINTS; NOTE must match LightUints in GlimmerSHVolume.cpp)
#define GLIMMER_SH_SUN_UINT (GLIMMER_SH_LIGHT_WIDE_ENTRIES + GLIMMER_LIGHT_TRANSFER_ENTRIES + (GLIMMER_LIGHT_ENTRIES - GLIMMER_LIGHT_TRANSFER_ENTRIES) * 2)
#ifndef GLIMMER_SUN_KEYS
#define GLIMMER_SUN_KEYS 14u
#endif

// the sun bounce at each key along the sun's arc (2 each, GLIMMER_SUN_KEY_L1_UINTS), then the arc's epoch and the updates the keys hold
#define GLIMMER_SH_SUN_KEYS_UINT (GLIMMER_SH_SUN_UINT + 7)
#define GLIMMER_SH_SUN_KEY_SETTLE GLIMMER_SUN_KEYS
#define GLIMMER_SH_SUN_KEY_STATE_UINT (GLIMMER_SH_SUN_KEYS_UINT + GLIMMER_SUN_KEYS * 2u)
#define GLIMMER_SH_LIGHT_UINTS (GLIMMER_SH_SUN_KEY_STATE_UINT + 1)

struct GlimmerSHCascade
{
    /// xyz = window's first voxel pos
    /// w = cascade has a window?
    int4 origin;

    /// x = voxel spacing
    /// y = 1 / spacing
    /// z = Rendering.Glimmer.Visibility enabled?
    /// w = finest ground level its traces read
    float4 params;

    /// xyz = the window's center before snapping, in voxels
    float4 center;
};

struct GlimmerSHVolume
{
    GlimmerSHCascade cascades[GLIMMER_SH_CASCADES];
    
    /// x = number of cascades
    /// w = 1 when anything has been traced
    uint4 info;
};

uint3 GlimmerSHTexel(uint cascadeIndex, int3 voxel)
{
    return uint3(
        uint(voxel.x & (GLIMMER_SH_GRID_XZ - 1)),
        cascadeIndex * GLIMMER_SH_GRID_Y + uint(voxel.y & (GLIMMER_SH_GRID_Y - 1)),
        uint(voxel.z & (GLIMMER_SH_GRID_XZ - 1)));
}

uint3 GlimmerSHRadianceSection(uint3 texel, uint section)
{
    return texel + uint3((section % GLIMMER_SH_RADIANCE_COLUMNS) * GLIMMER_SH_GRID_XZ, (section / GLIMMER_SH_RADIANCE_COLUMNS) * GLIMMER_SH_RADIANCE_ROW_STRIDE, 0u);
}

uint3 GlimmerSHRadianceTexel(uint3 texel, uint channel)
{
    return GlimmerSHRadianceSection(texel, channel);
}

bool GlimmerSHIsWideLightEntry(uint entry)
{
    return entry < GLIMMER_SH_LIGHT_WIDE_ENTRIES || entry >= GLIMMER_LIGHT_TRANSFER_ENTRIES;
}

uint GlimmerSHSunIndex(uint3 texel)
{
    const uint voxelIndex = (texel.z * GLIMMER_SH_RADIANCE_ROW_STRIDE + texel.y) * GLIMMER_SH_GRID_XZ + texel.x;

    return voxelIndex * GLIMMER_SH_LIGHT_UINTS + GLIMMER_SH_SUN_UINT;
}

// the voxel a texel holds, in the window of its cascade
int3 GlimmerSHTexelVoxel(GlimmerSHVolume volume, uint3 texel, out uint outCascadeIndex)
{
    outCascadeIndex = texel.y / GLIMMER_SH_GRID_Y;

    const int3 origin = volume.cascades[outCascadeIndex].origin.xyz;
    const int3 wrapped = int3(texel.x, texel.y % GLIMMER_SH_GRID_Y, texel.z);
    const int3 gridMask = int3(GLIMMER_SH_GRID_XZ - 1, GLIMMER_SH_GRID_Y - 1, GLIMMER_SH_GRID_XZ - 1);

    return origin + ((wrapped - origin) & gridMask);
}

uint GlimmerSHLightIndex(uint3 texel, uint entry)
{
    const uint voxelIndex = (texel.z * GLIMMER_SH_RADIANCE_ROW_STRIDE + texel.y) * GLIMMER_SH_GRID_XZ + texel.x;

    return voxelIndex * GLIMMER_SH_LIGHT_UINTS
        + entry
        + min(entry, GLIMMER_SH_LIGHT_WIDE_ENTRIES)
        + (entry > GLIMMER_LIGHT_TRANSFER_ENTRIES ? entry - GLIMMER_LIGHT_TRANSFER_ENTRIES : 0u);
}

float4 GlimmerSHUnpackLight(uint entry, uint word0, uint word1)
{
    if (GlimmerSHIsWideLightEntry(entry))
    {
        return GlimmerUnpackLightEntry(uint2(word0, word1));
    }

    return GlimmerUnpackTransferEntry(word0, GlimmerIsScalarEntry(entry));
}

uint2 GlimmerSHPackLight(uint entry, float4 value, float4 dither)
{
    if (GlimmerSHIsWideLightEntry(entry))
    {
        return GlimmerPackLightEntry(value);
    }

    return uint2(GlimmerPackTransferEntry(value, GlimmerIsScalarEntry(entry), dither), 0u);
}

#define GLIMMER_SH_LOAD_LIGHT_FROM(buffer, texel, entry) \
    GlimmerSHUnpackLight(entry, buffer[GlimmerSHLightIndex(texel, entry)], buffer[GlimmerSHLightIndex(texel, entry) + (GlimmerSHIsWideLightEntry(entry) ? 1u : 0u)])

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
