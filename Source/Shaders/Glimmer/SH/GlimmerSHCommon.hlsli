#ifndef GLIMMER_SH_COMMON_HLSLI
#define GLIMMER_SH_COMMON_HLSLI

// Clipmap of GLIMMER_SH_CASCADES cascades of GRID_XZ x GRID_Y x GRID_XZ voxels around the viewer, world aligned, each cascade twice the
// spacing of the last. Stored toroidally: the state in a Texture3D of GRID_XZ x (CASCADES * GRID_Y) x GRID_XZ, and the rest in one
// Texture3D of GRID_XZ x (CASCADES * GRID_Y) x (SLABS * GRID_XZ), a GRID_XZ deep slab each (GlimmerSHSlabTexel):
//  visibility = L1 of sky visibility, cosine convolved: the fraction of the sky a surface facing n sees is v.x + dot(v.yzw, n)
//  bounce     = rgb: albedo of what blocks the sky, weighted by how much sky each blocker faces; a: the sun's share (N.L and shadow)
//               relative to that, or -1 inside the ground or a solid
//  depth x/y/z = per axis direction (+, -), the mean and mean square distance to the ground or a solid, in voxels and capped at
//                GLIMMER_SH_DEPTH_RANGE: lighting uses them to drop voxels that can't see the surface being lit (the far side of a wall)
//  state      = the absolute voxel it was traced for (GlimmerSHPackVoxel), so voxels that scrolled in and aren't traced yet read as empty

#define GLIMMER_SH_CASCADES 5
#define GLIMMER_SH_GRID_XZ 32
#define GLIMMER_SH_GRID_Y 16

// Must match GlimmerSHDataSlabs in GlimmerSHVolume.hpp
#define GLIMMER_SH_SLAB_VISIBILITY 0u
#define GLIMMER_SH_SLAB_BOUNCE 1u
#define GLIMMER_SH_SLAB_DEPTH 2u // + axis
#define GLIMMER_SH_SLABS 5u

// only blockers within about a voxel diagonal matter for interpolation
#define GLIMMER_SH_DEPTH_RANGE 2.0

// Must match GlimmerSHCascadeShaderData in GlimmerSHVolume.hpp
struct GlimmerSHCascade
{
    int4 origin;   // xyz = absolute voxel of the window's first voxel, w = 1 once the cascade has a window
    float4 params; // x = voxel spacing, y = 1 / spacing
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

// 0/1 = +x/-x, 2/3 = +y/-y, 4/5 = +z/-z
uint GlimmerSHDirectionBin(float3 direction)
{
    const float3 absDirection = abs(direction);
    const uint axis = (absDirection.x >= absDirection.y && absDirection.x >= absDirection.z) ? 0u : (absDirection.y >= absDirection.z ? 1u : 2u);

    return axis * 2u + (direction[axis] < 0.0 ? 1u : 0u);
}

// bit 16 of y marks a traced voxel, so never written (zeroed) state can't match voxel (0, 0, 0)
uint2 GlimmerSHPackVoxel(int3 voxel)
{
    return uint2((uint(voxel.x) & 0xFFFFu) | ((uint(voxel.z) & 0xFFFFu) << 16), (uint(voxel.y) & 0xFFFFu) | 0x10000u);
}

#endif
