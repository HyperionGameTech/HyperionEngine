#include "../../Include/Defines.hlsli"

PERMUTE(MODE, CLEAR, PLACE, ALLOCATE)

#include "GlimmerSurfelTypes.hlsli"

struct GlimmerSurfelPlaceConstants
{
    GlimmerSurfels surfels;

    /// xyz = where the surfels are placed around
    /// w = how far from there
    float4 center;

    /// x = spacing (m)
    /// y = most surfels a triangle gets
    /// z = leaf area (m^2) a foliage voxel needs to be kept
    float4 placement;

    /// x = span instances
    /// y = groups along x
    /// z = seed
    uint4 dispatch;
};

DECLARE_BUFFER_DYNAMIC(GlimmerSurfelPlace, CBuffer) cbuffer CBuffer
{
    GlimmerSurfelPlaceConstants constants;
};

/// GLIMMER_SURFEL_HASH_UINTS
DECLARE_UAV(GlimmerSurfelPlace, OutSurfelHash) RWStructuredBuffer<uint> OutSurfelHash;

/// the foliage voxels' hash, then the voxels
DECLARE_UAV(GlimmerSurfelPlace, OutFoliage) RWStructuredBuffer<uint> OutFoliage;

#if defined(MODE_CLEAR)

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint entry = dispatchThreadId.x;

    if (entry < GLIMMER_SURFEL_COUNTERS)
    {
        OutSurfelHash[entry] = 0u;
    }

    if (entry < GLIMMER_FOLIAGE_HASH_ENTRIES)
    {
        OutFoliage[GlimmerFoliageHashIndex(entry)] = GLIMMER_SURFEL_HASH_EMPTY;
        OutFoliage[GlimmerFoliageHashIndex(entry) + 3u] = 0u;
    }

    if (entry >= GLIMMER_SURFEL_HASH_ENTRIES)
    {
        return;
    }

    OutSurfelHash[GlimmerSurfelHashKeyIndex(entry)] = GLIMMER_SURFEL_HASH_EMPTY;
    OutSurfelHash[GlimmerSurfelHashKeyIndex(entry) + 1u] = GLIMMER_SURFEL_NONE;
}

#elif defined(MODE_ALLOCATE)

// a thread per hash entry: the voxels with enough leaves in them get a place and their index, the rest none
[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint slot = dispatchThreadId.x;

    if (slot >= GLIMMER_FOLIAGE_HASH_ENTRIES)
    {
        return;
    }

    const uint base = GlimmerFoliageHashIndex(slot);

    if (OutFoliage[base] == GLIMMER_SURFEL_HASH_EMPTY)
    {
        return;
    }

    const float leafArea = float(OutFoliage[base + 3u]) / GLIMMER_FOLIAGE_AREA_SCALE;

    if (leafArea < constants.placement.z)
    {
        OutFoliage[base + 3u] = GLIMMER_SURFEL_NONE;

        return;
    }

    uint voxelIndex;
    InterlockedAdd(OutSurfelHash[GLIMMER_SURFEL_COUNTER_FOLIAGE], 1u, voxelIndex);

    if (voxelIndex >= GLIMMER_FOLIAGE_CAPACITY)
    {
        OutFoliage[base + 3u] = GLIMMER_SURFEL_NONE;

        InterlockedAdd(OutSurfelHash[GLIMMER_SURFEL_COUNTER_FOLIAGE_DROPPED], 1u);

        return;
    }

    const int3 cell = GlimmerUnpackFoliageCell(uint2(OutFoliage[base + 1u], OutFoliage[base + 2u]));

    OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL)] = asuint(cell.x);
    OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL + 1u)] = asuint(cell.y);
    OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL + 2u)] = asuint(cell.z);
    OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_STATE)] = 0u;
    OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_SUN_KEY_STATE)] = 0u;
    OutFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_VISIBLE)] = 0u;

    OutFoliage[base + 3u] = voxelIndex;
}

#else

DECLARE_SRV(GlimmerSurfelPlace, SpanInstancesBuffer) StructuredBuffer<GlimmerSpanInstance> spanInstances;
DECLARE_SRV(GlimmerSurfelPlace, GlimmerTrianglesBuffer) StructuredBuffer<uint> glimmerTriangles;

/// xyz = position, w = normal (GlimmerPackSurfelNormal)
DECLARE_UAV(GlimmerSurfelPlace, OutSurfels) RWStructuredBuffer<uint4> OutSurfels;
DECLARE_UAV(GlimmerSurfelPlace, OutSurfelData) RWStructuredBuffer<uint> OutSurfelData;
DECLARE_UAV(GlimmerSurfelPlace, OutSurfelStates) RWStructuredBuffer<uint2> OutSurfelStates;

#define PLACE_GROUP_SIZE 64

// points a foliage triangle's leaf area is spread over, at most
#define MAX_FOLIAGE_POINTS 64u

float3 TransformPoint(GlimmerSpanInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.objectToWorld0, homogeneous), dot(instance.objectToWorld1, homogeneous), dot(instance.objectToWorld2, homogeneous));
}

float GlimmerSurfelRandom(inout uint state)
{
    state = GlimmerSurfelHash(state + 0x9E3779B9u);

    return float(state & 0xFFFFFFu) * (1.0 / 16777216.0);
}

float3 PointOnTriangle(float3 p0, float3 p1, float3 p2, float2 offset, uint sampleIndex)
{
    const float2 r2 = frac(offset + float(sampleIndex) * float2(0.7548776662, 0.5698402910));
    const float rootX = sqrt(r2.x);

    return p0 * (1.0 - rootX) + p1 * (rootX * (1.0 - r2.y)) + p2 * (rootX * r2.y);
}

void InsertSurfel(uint surfelIndex, int3 cell)
{
    const uint key = GlimmerSurfelCellKey(cell);

    uint slot = key & (GLIMMER_SURFEL_HASH_ENTRIES - 1u);

    for (uint probe = 0; probe < GLIMMER_SURFEL_HASH_PROBES; probe++)
    {
        uint previousKey;
        InterlockedCompareExchange(OutSurfelHash[GlimmerSurfelHashKeyIndex(slot)], GLIMMER_SURFEL_HASH_EMPTY, key, previousKey);

        if (previousKey == GLIMMER_SURFEL_HASH_EMPTY || previousKey == key)
        {
            uint previousFirst;
            InterlockedExchange(OutSurfelHash[GlimmerSurfelHashKeyIndex(slot) + 1u], surfelIndex, previousFirst);

            OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_NEXT)] = previousFirst;

            return;
        }

        slot = (slot + 1u) & (GLIMMER_SURFEL_HASH_ENTRIES - 1u);
    }

    OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_NEXT)] = GLIMMER_SURFEL_NONE;

    InterlockedAdd(OutSurfelHash[GLIMMER_SURFEL_COUNTER_DROPPED], 1u);
}

bool AddSurfel(float3 position, float3 normal)
{
    uint surfelIndex;
    InterlockedAdd(OutSurfelHash[GLIMMER_SURFEL_COUNTER_SURFELS], 1u, surfelIndex);

    if (surfelIndex >= GLIMMER_SURFEL_CAPACITY)
    {
        return false;
    }

    OutSurfels[surfelIndex] = uint4(asuint(position), GlimmerPackSurfelNormal(normal));
    OutSurfelStates[surfelIndex] = uint2(0u, 0u);
    OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_SUN_KEY_STATE)] = 0u;
    OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_COMPOSED)] = 0u;
    OutSurfelData[GlimmerSurfelDataIndex(surfelIndex, GLIMMER_SURFEL_DATA_COMPOSED) + 1u] = 0u;

    InsertSurfel(surfelIndex, GlimmerSurfelCellOf(constants.surfels, position));

    return true;
}

// two cells whose keys collide share the area; only the first to claim the entry is found again
void AddLeafArea(int3 cell, float leafArea)
{
    const uint key = GlimmerFoliageKey(cell);
    const uint2 packedCell = GlimmerPackFoliageCell(cell);
    const uint area = uint(leafArea * GLIMMER_FOLIAGE_AREA_SCALE + 0.5);

    uint slot = key & (GLIMMER_FOLIAGE_HASH_ENTRIES - 1u);

    for (uint probe = 0; probe < GLIMMER_SURFEL_HASH_PROBES; probe++)
    {
        const uint base = GlimmerFoliageHashIndex(slot);

        uint previousKey;
        InterlockedCompareExchange(OutFoliage[base], GLIMMER_SURFEL_HASH_EMPTY, key, previousKey);

        if (previousKey == GLIMMER_SURFEL_HASH_EMPTY)
        {
            OutFoliage[base + 1u] = packedCell.x;
            OutFoliage[base + 2u] = packedCell.y;
        }

        if (previousKey == GLIMMER_SURFEL_HASH_EMPTY || previousKey == key)
        {
            InterlockedAdd(OutFoliage[base + 3u], area);

            return;
        }

        slot = (slot + 1u) & (GLIMMER_FOLIAGE_HASH_ENTRIES - 1u);
    }
}

void PlaceSurfels(GlimmerSpanInstance instance, uint instanceIndex, uint localTriangle, float3 p0, float3 p1, float3 p2, float3 normal, float doubleArea, inout bool ioIsFull)
{
    const bool isDoubleSided = (instance.data.w & GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED) != 0u;

    const float spacing = constants.placement.x;
    const float density = 1.0 / (spacing * spacing);

    uint rng = GlimmerSurfelHash(instanceIndex * 0x9E3779B1u ^ localTriangle * 0x85EBCA77u ^ constants.dispatch.z);

    const float expected = 0.5 * doubleArea * density;
    const uint count = min(uint(expected) + (GlimmerSurfelRandom(rng) < frac(expected) ? 1u : 0u), uint(constants.placement.y));

    const float2 r2Offset = float2(GlimmerSurfelRandom(rng), GlimmerSurfelRandom(rng));

    for (uint sampleIndex = 0; sampleIndex < count; sampleIndex++)
    {
        const float3 position = PointOnTriangle(p0, p1, p2, r2Offset, sampleIndex);

        if (!AddSurfel(position, normal) || (isDoubleSided && !AddSurfel(position, -normal)))
        {
            ioIsFull = true;

            return;
        }
    }
}

void PlaceLeaves(uint instanceIndex, uint localTriangle, float3 p0, float3 p1, float3 p2, float doubleArea)
{
    const float voxelSize = constants.surfels.foliage.x;
    const float area = 0.5 * doubleArea;

    // a few points a voxel face, so a large card spreads its leaves over the voxels it crosses
    const uint count = clamp(uint(ceil(area / (0.25 * voxelSize * voxelSize))), 1u, MAX_FOLIAGE_POINTS);

    uint rng = GlimmerSurfelHash(instanceIndex * 0x27D4EB2Fu ^ localTriangle * 0x165667B1u ^ constants.dispatch.z);

    const float2 r2Offset = float2(GlimmerSurfelRandom(rng), GlimmerSurfelRandom(rng));

    for (uint sampleIndex = 0; sampleIndex < count; sampleIndex++)
    {
        const float3 position = PointOnTriangle(p0, p1, p2, r2Offset, sampleIndex);

        AddLeafArea(int3(floor(position * constants.surfels.foliage.y)), area / float(count));
    }
}

[numthreads(PLACE_GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint instanceIndex = groupId.y * constants.dispatch.y + groupId.x;

    if (instanceIndex >= constants.dispatch.x)
    {
        return;
    }

    const GlimmerSpanInstance instance = spanInstances[instanceIndex];

    const uint flags = instance.data.w;
    const bool isFoliage = (flags & GLIMMER_INSTANCE_FLAG_FOLIAGE) != 0u;

    if (!isFoliage && instance.lightmap.x != 0xFFFFFFFFu)
    {
        return;
    }

    if (isFoliage && constants.surfels.foliage.x <= 0.0)
    {
        return;
    }

    const float facing = (flags & GLIMMER_INSTANCE_FLAG_MIRRORED) != 0u ? -1.0 : 1.0;

    const float3 center = constants.center.xyz;
    const float radiusSquared = constants.center.w * constants.center.w;

    bool isFull = false;

    for (uint localTriangle = groupIndex; localTriangle < instance.data.y && !isFull; localTriangle += PLACE_GROUP_SIZE)
    {
        const GlimmerTriangle glimmerTriangle = LOAD_GLIMMER_TRIANGLE(glimmerTriangles, instance.data.x + localTriangle);

        const float3 p0 = TransformPoint(instance, glimmerTriangle.position0);
        const float3 p1 = TransformPoint(instance, glimmerTriangle.position1);
        const float3 p2 = TransformPoint(instance, glimmerTriangle.position2);

        const float3 toCentroid = (p0 + p1 + p2) * (1.0 / 3.0) - center;

        if (dot(toCentroid, toCentroid) > radiusSquared)
        {
            continue;
        }

        const float3 crossed = cross(p1 - p0, p2 - p0);
        const float doubleArea = length(crossed);

        if (doubleArea <= 1e-10)
        {
            continue;
        }

        if (isFoliage)
        {
            PlaceLeaves(instanceIndex, localTriangle, p0, p1, p2, doubleArea);
        }
        else
        {
            PlaceSurfels(instance, instanceIndex, localTriangle, p0, p1, p2, crossed / doubleArea * facing, doubleArea, isFull);
        }
    }
}

#endif
