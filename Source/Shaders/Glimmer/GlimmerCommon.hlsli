#ifndef GLIMMER_COMMON_HLSLI
#define GLIMMER_COMMON_HLSLI

// Shared by every Glimmer technique: the ground heightfield and albedo clipmaps, the terrain patches that fill the albedo,
// and the spans of solids and canopy splatted above the ground from the scene's instances

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

float3 GlimmerRotateByQuaternion(float4 q, float3 v)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

// Evenly spread directions over the sphere; the per frame rotation turns them into a stratified random pattern
float3 GlimmerSphericalFibonacci(uint index, uint count)
{
    const float goldenRatio = 1.6180339887;

    const float phi = 2.0 * 3.14159265 * frac(float(index) * (goldenRatio - 1.0));
    const float cosTheta = 1.0 - (2.0 * float(index) + 1.0) / float(count);
    const float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));

    return float3(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta);
}

// Must match GlimmerInstanceFlags in GlimmerTLAS.hpp
#define GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED 0x1u
#define GLIMMER_INSTANCE_FLAG_MIRRORED 0x2u
#define GLIMMER_INSTANCE_FLAG_ALPHA_TESTED 0x4u
#define GLIMMER_INSTANCE_FLAG_FOLIAGE 0x8u

// Must match GlimmerSpanInstanceShaderData in GlimmerTLAS.hpp
struct GlimmerSpanInstance
{
    float4 objectToWorld0;
    float4 objectToWorld1;
    float4 objectToWorld2;
    uint4 data; // x = BLAS triangle base, y = triangle count, z = material index, w = flags
};

// Must match GlimmerSpanLevelShaderData in GlimmerSpanCache.hpp
struct GlimmerSpanLevel
{
    int4 window;   // xy = absolute texel the spans were built from, z = 1 when built
    float4 params; // x = texel size, y = 1 / texel size
};

struct GlimmerSpanParams
{
    GlimmerSpanLevel levels[GLIMMER_GROUND_LEVELS];
};

// Per texel values in the spans buffer
#define GLIMMER_SPAN_SOLID_MIN 0
#define GLIMMER_SPAN_SOLID_MAX 1
#define GLIMMER_SPAN_CANOPY_MIN 2
#define GLIMMER_SPAN_CANOPY_MAX 3
#define GLIMMER_SPAN_LEAF_AREA 4      // leaf area index
#define GLIMMER_SPAN_CANOPY_ALBEDO 5 // 3 values
#define GLIMMER_SPAN_SOLID_ALBEDO 8  // 3 values
#define GLIMMER_SPAN_SOLID_AREA 11     // surface area / texel area, how filled the texel is
#define GLIMMER_SPAN_VALUES_PER_TEXEL 12

// areas are accumulated with integer atomics in these units
#define GLIMMER_SPAN_AREA_SCALE 256.0

uint GlimmerSpanTexelIndex(uint level, int2 texel)
{
    const uint2 wrapped = GlimmerWrapGroundTexel(texel);

    return ((level * GLIMMER_GROUND_RESOLUTION + wrapped.y) * GLIMMER_GROUND_RESOLUTION + wrapped.x) * GLIMMER_SPAN_VALUES_PER_TEXEL;
}

// what min/max atomics over GlimmerOrderedUintFromFloat() values start from
#define GLIMMER_MASK_EMPTY_MIN 0xFFFFFFFFu
#define GLIMMER_MASK_EMPTY_MAX 0u

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
