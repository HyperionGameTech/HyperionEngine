#ifndef GLIMMER_COMMON_HLSLI
#define GLIMMER_COMMON_HLSLI

// Shared by every Glimmer technique: the ground heightfield and albedo clipmaps and the terrain patches that fill the albedo

#define GLIMMER_GROUND_LEVELS 4
#define GLIMMER_GROUND_RESOLUTION 256
#define GLIMMER_NO_GROUND_HEIGHT -60000.0

// Must match GlimmerGroundLevelShaderData in GlimmerSurfaceCache.hpp
struct GlimmerGroundLevel
{
    int4 validRect; // absolute texels, xy = min, zw = max (exclusive)
    float4 params;  // x = texel size, y = 1 / texel size
};

struct GlimmerGroundParams
{
    GlimmerGroundLevel levels[GLIMMER_GROUND_LEVELS];
};

// Must match GlimmerTerrainPatchShaderData in GlimmerSurfaceCache.hpp
struct GlimmerTerrainPatch
{
    float4 worldToObject0;
    float4 worldToObject2;
    float4 boundsXZ; // xy = world xz min, zw = max
    uint4 data;      // x = material index
};

uint2 GlimmerWrapGroundTexel(int2 texel)
{
    return uint2(texel & (GLIMMER_GROUND_RESOLUTION - 1));
}

// Monotonic float <-> uint mapping so heights can be min/maxed with integer atomics
uint GlimmerOrderedUintFromFloat(float value)
{
    const uint bits = asuint(value);

    return (bits & 0x80000000u) != 0u ? ~bits : (bits | 0x80000000u);
}

float GlimmerFloatFromOrderedUint(uint value)
{
    return asfloat((value & 0x80000000u) != 0u ? (value & 0x7FFFFFFFu) : ~value);
}

#endif
