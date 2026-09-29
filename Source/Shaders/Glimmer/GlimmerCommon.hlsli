#ifndef GLIMMER_COMMON_HLSLI
#define GLIMMER_COMMON_HLSLI

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

#define GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED 0x1u
#define GLIMMER_INSTANCE_FLAG_MIRRORED 0x2u
#define GLIMMER_INSTANCE_FLAG_ALPHA_TESTED 0x4u

// Must match GlimmerFootprintMaskShaderData in GlimmerFootprintMask.hpp
struct GlimmerFootprintMaskParams
{
    float4 originCellSize; // xy = world xz of cell (0, 0)'s corner, z = cell size, w = 1 when the mask is valid
    uint4 info;            // x = resolution of level 0, y = number of levels
};

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

uint2 GlimmerWrapGroundTexel(int2 texel)
{
    return uint2(texel & (GLIMMER_GROUND_RESOLUTION - 1));
}

// Must match GlimmerSpanInstanceShaderData in GlimmerTLAS.hpp
struct GlimmerSpanInstance
{
    float4 objectToWorld0;
    float4 objectToWorld1;
    float4 objectToWorld2;
    uint4 data; // x = BLAS triangle base, y = triangle count, z = material index, w = flags
};

#define GLIMMER_INSTANCE_FLAG_FOLIAGE 0x8u

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
