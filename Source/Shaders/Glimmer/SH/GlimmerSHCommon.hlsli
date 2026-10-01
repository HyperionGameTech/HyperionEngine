#ifndef GLIMMER_SH_COMMON_HLSLI
#define GLIMMER_SH_COMMON_HLSLI

#include "../GlimmerCommon.hlsli"

// Clipmap of GLIMMER_SH_CASCADES cascades of GRID_XZ x GRID_Y x GRID_XZ voxels around the viewer, world aligned, each cascade twice the
// spacing of the last. Stored toroidally: the state in a Texture3D of GRID_XZ x (CASCADES * GRID_Y) x GRID_XZ, and the rest in one
// Texture3D of GRID_XZ x (CASCADES * GRID_Y) x (SLABS * GRID_XZ), a GRID_XZ deep slab each (GlimmerSHSlabTexel):
//  visibility = L1 of sky visibility, cosine convolved: the fraction of the sky a surface facing n sees is v.x + dot(v.yzw, n)
//  bounce     = rgb: albedo of what blocks the sky, weighted by how much sky each blocker faces; a: the sun's share (N.L and shadow)
//               relative to that, or -1 inside the ground or a solid
//  depth      = an octahedral map (GlimmerOctahedralEncode) of GLIMMER_SH_VISIBILITY_RES^2 texels, two per slab (xy, zw): the mean and
//               mean square distance to the ground or a solid around each direction, in voxels and capped at GLIMMER_SH_DEPTH_RANGE.
//               Lighting uses them to drop voxels that can't see the surface being lit (the far side of a wall)
//  state      = the absolute voxel it was traced for (GlimmerSHPackState), so voxels that scrolled in and aren't traced yet read as empty,
//               and where in it the rays started (GlimmerSHStateOffset): off the centre when the centre is in a solid

#define GLIMMER_SH_CASCADES 5
#define GLIMMER_SH_GRID_XZ 32
#define GLIMMER_SH_GRID_Y 16

#define GLIMMER_SH_VISIBILITY_RES 8
#define GLIMMER_SH_VISIBILITY_TEXELS (GLIMMER_SH_VISIBILITY_RES * GLIMMER_SH_VISIBILITY_RES)

// Must match GlimmerSHDataSlabs in GlimmerSHVolume.hpp
#define GLIMMER_SH_SLAB_VISIBILITY 0u
#define GLIMMER_SH_SLAB_BOUNCE 1u
#define GLIMMER_SH_SLAB_DEPTH 2u // + depth map texel / 2
#define GLIMMER_SH_SLABS (2u + GLIMMER_SH_VISIBILITY_TEXELS / 2u)

// only blockers within about a voxel diagonal matter for interpolation
#define GLIMMER_SH_DEPTH_RANGE 2.0

// Must match GlimmerSHCascadeShaderData in GlimmerSHVolume.hpp
struct GlimmerSHCascade
{
    int4 origin;   // xyz = absolute voxel of the window's first voxel, w = 1 once the cascade has a window
    float4 params; // x = voxel spacing, y = 1 / spacing, z = how much visibility counts
};

// Must match GlimmerSHVolumeShaderData in GlimmerSHVolume.hpp
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

// where a voxel's texel (GlimmerSHTexel) is in the given slab of the data texture
uint3 GlimmerSHSlabTexel(uint3 texel, uint slab)
{
    return uint3(texel.xy, slab * GLIMMER_SH_GRID_XZ + texel.z);
}

float3 GlimmerSHVoxelCenter(GlimmerSHCascade cascade, int3 voxel)
{
    return (float3(voxel) + 0.5) * cascade.params.x;
}

// how far (in voxels, per axis) the rays' origin can sit off the voxel's centre
#define GLIMMER_SH_MAX_ORIGIN_OFFSET 0.5

// x = voxel x and z (16 bits each), y = voxel y (bits 0-15) | traced (bit 16, so never written (zeroed) state can't match voxel (0, 0, 0))
// | the rays' origin off the voxel's centre (bits 17-31, 3 x 5 bit snorm of GLIMMER_SH_MAX_ORIGIN_OFFSET voxels)
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
