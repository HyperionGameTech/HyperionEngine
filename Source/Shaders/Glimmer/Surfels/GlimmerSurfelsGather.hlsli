#ifndef GLIMMER_SURFELS_GATHER_HLSLI
#define GLIMMER_SURFELS_GATHER_HLSLI

#include "GlimmerSurfelTypes.hlsli"

/// NOTE expects glimmerSurfels, glimmerSurfelHash, glimmerSurfelData and glimmerFoliage to be declared

#define GLIMMER_SURFEL_MAX_PER_CELL 64u
#define GLIMMER_SURFEL_MIN_FACING 0.5
#define GLIMMER_SURFEL_PLANE_TOLERANCE 0.5 // of the radius
#define GLIMMER_SURFEL_FULL_WEIGHT 0.15

uint GlimmerFindSurfelCell(int3 cell)
{
    const uint key = GlimmerSurfelCellKey(cell);

    uint slot = key & (GLIMMER_SURFEL_HASH_ENTRIES - 1u);

    for (uint probe = 0; probe < GLIMMER_SURFEL_HASH_PROBES; probe++)
    {
        const uint slotKey = glimmerSurfelHash[GlimmerSurfelHashKeyIndex(slot)];

        if (slotKey == key)
        {
            return glimmerSurfelHash[GlimmerSurfelHashKeyIndex(slot) + 1u];
        }

        if (slotKey == GLIMMER_SURFEL_HASH_EMPTY)
        {
            return GLIMMER_SURFEL_NONE;
        }

        slot = (slot + 1u) & (GLIMMER_SURFEL_HASH_ENTRIES - 1u);
    }

    return GLIMMER_SURFEL_NONE;
}

uint GlimmerNextSurfel(uint surfelIndex)
{
    return glimmerSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_NEXT)];
}

float4 GlimmerSurfelComposedOf(uint surfelIndex)
{
    const uint base = GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_COMPOSED);

    return GlimmerUnpackSurfelComposed(uint2(glimmerSurfelData[base], glimmerSurfelData[base + 1u]));
}

int3 GlimmerFirstSurfelGatherCell(GlimmerSurfels surfels, float3 P)
{
    return int3(floor(P * surfels.params.y - 0.5));
}

int3 GlimmerSurfelGatherCell(int3 firstCell, uint corner)
{
    return firstCell + int3(corner & 1u, (corner >> 1) & 1u, (corner >> 2) & 1u);
}

/// before how much of the surfel can be seen; 0 when it doesn't reach P
float GlimmerSurfelReach(GlimmerSurfels surfels, uint4 surfel, float3 P, float3 N)
{
    const float radius = surfels.params.z;

    const float3 toPoint = P - asfloat(surfel.xyz);
    const float distanceSquared = dot(toPoint, toPoint);

    if (distanceSquared >= radius * radius)
    {
        return 0.0;
    }

    const float3 surfelNormal = GlimmerUnpackSurfelNormal(surfel.w);

    const float facing = dot(N, surfelNormal);
    const float planeTolerance = radius * GLIMMER_SURFEL_PLANE_TOLERANCE;
    const float planeDistance = abs(dot(toPoint, surfelNormal));

    if (facing <= GLIMMER_SURFEL_MIN_FACING || planeDistance >= planeTolerance)
    {
        return 0.0;
    }

    const float falloff = 1.0 - distanceSquared / (radius * radius);

    return falloff * falloff
        * (facing - GLIMMER_SURFEL_MIN_FACING) / (1.0 - GLIMMER_SURFEL_MIN_FACING)
        * (1.0 - planeDistance / planeTolerance);
}

float GlimmerSurfelCoverage(float weightSum)
{
    return saturate(weightSum / GLIMMER_SURFEL_FULL_WEIGHT);
}

//////////FOLIAGE////////////

#define GLIMMER_FOLIAGE_FULL_WEIGHT 0.5

uint GlimmerFindFoliageVoxel(int3 cell)
{
    const uint key = GlimmerFoliageKey(cell);
    const uint2 packedCell = GlimmerPackFoliageCell(cell);

    uint slot = key & (GLIMMER_FOLIAGE_HASH_ENTRIES - 1u);

    for (uint probe = 0; probe < GLIMMER_SURFEL_HASH_PROBES; probe++)
    {
        const uint base = GlimmerFoliageHashIndex(slot);
        const uint slotKey = glimmerFoliage[base];

        if (slotKey == GLIMMER_SURFEL_HASH_EMPTY)
        {
            return GLIMMER_SURFEL_NONE;
        }

        if (slotKey == key && glimmerFoliage[base + 1u] == packedCell.x && glimmerFoliage[base + 2u] == packedCell.y)
        {
            return glimmerFoliage[base + 3u];
        }

        slot = (slot + 1u) & (GLIMMER_FOLIAGE_HASH_ENTRIES - 1u);
    }

    return GLIMMER_SURFEL_NONE;
}

struct GlimmerFoliageTaps
{
    uint indices[8];
    float weights[8];
    float weightSum;
};

// trilinear over the voxels around P that have been traced; returns their coverage
float GlimmerGatherFoliage(GlimmerSurfels surfels, float3 P, out GlimmerFoliageTaps outTaps)
{
    outTaps.indices = (uint[8])0;
    outTaps.weights = (float[8])0.0;
    outTaps.weightSum = 0.0;

    if (surfels.info.w == 0u || surfels.foliage.x <= 0.0)
    {
        return 0.0;
    }

    const float3 gridCoord = P * surfels.foliage.y - 0.5;
    const int3 cell0 = int3(floor(gridCoord));
    const float3 fraction = gridCoord - float3(cell0);

    [unroll]
    for (uint corner = 0; corner < 8; corner++)
    {
        const int3 offset = int3(corner & 1u, (corner >> 1) & 1u, (corner >> 2) & 1u);
        const uint voxelIndex = GlimmerFindFoliageVoxel(cell0 + offset);

        [branch]
        if (voxelIndex == GLIMMER_SURFEL_NONE || glimmerFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_STATE)] == 0u)
        {
            continue;
        }

        const float3 trilinear = lerp(1.0 - fraction, fraction, float3(offset));
        const float weight = trilinear.x * trilinear.y * trilinear.z * asfloat(glimmerFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_VISIBLE)]);

        outTaps.indices[corner] = voxelIndex;
        outTaps.weights[corner] = weight;
        outTaps.weightSum += weight;
    }

    return saturate(outTaps.weightSum / GLIMMER_FOLIAGE_FULL_WEIGHT);
}

// the L1 irradiance / pi per colour channel; returns the coverage
float GlimmerSampleFoliageRadiance(GlimmerSurfels surfels, float3 P, out float4 outR, out float4 outG, out float4 outB)
{
    outR = (float4)0.0;
    outG = (float4)0.0;
    outB = (float4)0.0;

    GlimmerFoliageTaps taps;
    const float coverage = GlimmerGatherFoliage(surfels, P, taps);

    if (taps.weightSum <= 1e-5)
    {
        return 0.0;
    }

    [unroll]
    for (uint tap = 0; tap < 8; tap++)
    {
        [branch]
        if (taps.weights[tap] > 0.0)
        {
            const float weight = taps.weights[tap] / taps.weightSum;

            outR += GLIMMER_LOAD_FOLIAGE_COMPOSED(glimmerFoliage, taps.indices[tap], 0u) * weight;
            outG += GLIMMER_LOAD_FOLIAGE_COMPOSED(glimmerFoliage, taps.indices[tap], 1u) * weight;
            outB += GLIMMER_LOAD_FOLIAGE_COMPOSED(glimmerFoliage, taps.indices[tap], 2u) * weight;
        }
    }

    return coverage;
}

#endif
