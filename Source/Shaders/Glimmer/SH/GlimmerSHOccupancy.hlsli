#ifndef GLIMMER_SH_OCCUPANCY_HLSLI
#define GLIMMER_SH_OCCUPANCY_HLSLI

#include "../../Include/Packing.hlsli"
#include "GlimmerSHCommon.hlsli"

#define GLIMMER_SH_OCCUPANCY_GRID_XZ (GLIMMER_SH_GRID_XZ * 2)
#define GLIMMER_SH_OCCUPANCY_GRID_Y (GLIMMER_SH_GRID_Y * 2)

struct GlimmerSHOccupancyCascade
{
    int4 origin;   // xyz = absolute voxel of the window's first voxel, w = 1 once built
    float4 params; // x = voxel spacing, y = 1 / spacing
};

struct GlimmerSHOccupancyParams
{
    GlimmerSHOccupancyCascade cascades[GLIMMER_SH_CASCADES];
};

#define GLIMMER_SH_OCCUPANCY_TRACED_CASCADES 2

#define GLIMMER_SH_OCCUPANCY_SUB_SHIFT 2
#define GLIMMER_SH_OCCUPANCY_SUB_VOXELS (1 << GLIMMER_SH_OCCUPANCY_SUB_SHIFT)
#define GLIMMER_SH_OCCUPANCY_MASK_WORDS 2u

static const int3 GlimmerSHOccupancyGridSize = int3(GLIMMER_SH_OCCUPANCY_GRID_XZ, GLIMMER_SH_OCCUPANCY_GRID_Y, GLIMMER_SH_OCCUPANCY_GRID_XZ);

// the voxels wrap, so a window that moves keeps what it still covers
uint3 GlimmerSHOccupancyTexel(uint cascadeIndex, int3 voxel)
{
    return uint3(
        uint(voxel.x & (GLIMMER_SH_OCCUPANCY_GRID_XZ - 1)),
        cascadeIndex * GLIMMER_SH_OCCUPANCY_GRID_Y + uint(voxel.y & (GLIMMER_SH_OCCUPANCY_GRID_Y - 1)),
        uint(voxel.z & (GLIMMER_SH_OCCUPANCY_GRID_XZ - 1)));
}

uint GlimmerSHOccupancyMaskIndex(uint cascadeIndex, int3 voxel)
{
    const uint3 texel = GlimmerSHOccupancyTexel(cascadeIndex, voxel);

    return ((texel.z * uint(GLIMMER_SH_OCCUPANCY_GRID_Y * GLIMMER_SH_CASCADES) + texel.y) * uint(GLIMMER_SH_OCCUPANCY_GRID_XZ) + texel.x) * GLIMMER_SH_OCCUPANCY_MASK_WORDS;
}

uint GlimmerSHOccupancySubBit(int3 subVoxel)
{
    return uint(subVoxel.x) | (uint(subVoxel.y) << GLIMMER_SH_OCCUPANCY_SUB_SHIFT) | (uint(subVoxel.z) << (2 * GLIMMER_SH_OCCUPANCY_SUB_SHIFT));
}

#define GLIMMER_SH_OCCUPANCY_LIGHTMAP_FACES 6u
#define GLIMMER_SH_OCCUPANCY_LIGHTMAP_BASE (uint(GLIMMER_SH_OCCUPANCY_GRID_XZ * GLIMMER_SH_OCCUPANCY_GRID_XZ * GLIMMER_SH_OCCUPANCY_GRID_Y * GLIMMER_SH_CASCADES) * GLIMMER_SH_OCCUPANCY_MASK_WORDS)

uint GlimmerSHOccupancyLightmapIndex(uint cascadeIndex, int3 voxel)
{
    const uint3 texel = GlimmerSHOccupancyTexel(cascadeIndex, voxel);

    return GLIMMER_SH_OCCUPANCY_LIGHTMAP_BASE + ((texel.z * uint(GLIMMER_SH_OCCUPANCY_GRID_Y * GLIMMER_SH_OCCUPANCY_TRACED_CASCADES) + texel.y) * uint(GLIMMER_SH_OCCUPANCY_GRID_XZ) + texel.x) * GLIMMER_SH_OCCUPANCY_LIGHTMAP_FACES;
}

bool GlimmerSHOccupancyContains(GlimmerSHOccupancyCascade cascade, float3 P)
{
    const int3 localVoxel = int3(floor(P * cascade.params.y)) - cascade.origin.xyz;

    return cascade.origin.w != 0 && all(localVoxel >= 0) && all(localVoxel < GlimmerSHOccupancyGridSize);
}

#endif // GLIMMER_SH_OCCUPANCY_HLSLI

#if !defined(GLIMMER_SH_OCCUPANCY_NO_TRACE) && !defined(GLIMMER_SH_OCCUPANCY_TRACE_HLSLI)
#define GLIMMER_SH_OCCUPANCY_TRACE_HLSLI

struct GlimmerSHOccupancyHit
{
    float t;
    float3 normal;
    float3 albedo;
    float spacing; // of the cascade the hit was in
    int3 voxel;
    uint cascadeIndex;
};

// true where any of the 8 sub voxels nearest P is solid
bool GlimmerSHOccupancyIsSolid(GlimmerSHOccupancyParams params, float3 P)
{
    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        const GlimmerSHOccupancyCascade cascade = params.cascades[cascadeIndex];

        if (!GlimmerSHOccupancyContains(cascade, P))
        {
            continue;
        }

        const int3 subVoxel0 = int3(floor(P * (cascade.params.y * float(GLIMMER_SH_OCCUPANCY_SUB_VOXELS)) - 0.5));

        [unroll]
        for (uint corner = 0; corner < 8; corner++)
        {
            const int3 subVoxel = subVoxel0 + int3(corner & 1u, (corner >> 1) & 1u, (corner >> 2) & 1u);
            const int3 voxel = subVoxel >> GLIMMER_SH_OCCUPANCY_SUB_SHIFT;
            const int3 localVoxel = voxel - cascade.origin.xyz;

            if (any(localVoxel < 0) || any(localVoxel >= GlimmerSHOccupancyGridSize))
            {
                continue;
            }

            const uint bit = GlimmerSHOccupancySubBit(subVoxel & (GLIMMER_SH_OCCUPANCY_SUB_VOXELS - 1));
            const uint word = glimmerSHOccupancyMask[GlimmerSHOccupancyMaskIndex(cascadeIndex, voxel) + (bit >> 5)];

            if (((word >> (bit & 31u)) & 1u) != 0u)
            {
                return true;
            }
        }

        return false;
    }

    return false;
}

float4 GlimmerSHLoadHitLightmap(GlimmerSHOccupancyHit occupancyHit, float3 direction)
{
    const uint lightmapIndex = GlimmerSHOccupancyLightmapIndex(occupancyHit.cascadeIndex, occupancyHit.voxel);

    float4 lightmap = (float4)0.0;

    [unroll]
    for (uint axis = 0; axis < 3; axis++)
    {
        const uint packed = glimmerSHOccupancyMask[lightmapIndex + axis * 2u + (direction[axis] > 0.0 ? 1u : 0u)];

        if (packed != 0u)
        {
            const float weight = direction[axis] * direction[axis];

            lightmap += float4(UnpackRGB9E5(packed) * weight, weight);
        }
    }

    return lightmap.a > 1e-4 ? float4(lightmap.rgb / lightmap.a, 1.0) : (float4)0.0;
}

bool GlimmerSHTraceOccupancyMask(
    uint cascadeIndex,
    int3 voxel,
    float spacing,
    float3 origin,
    float3 direction,
    float3 invDirection,
    int3 stepDirection,
    float tEnter,
    float tMax,
    inout float3 inOutNormal,
    out float outT)
{
    outT = tEnter;

    const uint maskIndex = GlimmerSHOccupancyMaskIndex(cascadeIndex, voxel);
    const uint2 mask = uint2(glimmerSHOccupancyMask[maskIndex], glimmerSHOccupancyMask[maskIndex + 1u]);

    const float subSpacing = spacing / float(GLIMMER_SH_OCCUPANCY_SUB_VOXELS);
    const int3 firstSubVoxel = voxel * GLIMMER_SH_OCCUPANCY_SUB_VOXELS;

    int3 subVoxel = clamp(int3(floor((origin + direction * tEnter) / subSpacing)) - firstSubVoxel, 0, GLIMMER_SH_OCCUPANCY_SUB_VOXELS - 1);

    float3 tNext = max((float3(firstSubVoxel + subVoxel + max(stepDirection, 0)) * subSpacing - origin) * invDirection, (float3)tEnter);
    const float3 tDelta = abs(subSpacing * invDirection);

    float t = tEnter;
    float3 normal = inOutNormal;

    [loop]
    for (uint stepIndex = 0; stepIndex < uint(3 * GLIMMER_SH_OCCUPANCY_SUB_VOXELS - 2); stepIndex++)
    {
        const uint bit = GlimmerSHOccupancySubBit(subVoxel);

        if ((((bit < 32u ? mask.x : mask.y) >> (bit & 31u)) & 1u) != 0u)
        {
            outT = t;
            inOutNormal = normal;

            return true;
        }

        if (tNext.x <= tNext.y && tNext.x <= tNext.z)
        {
            t = tNext.x;
            tNext.x += tDelta.x;
            subVoxel.x += stepDirection.x;
            normal = float3(-float(stepDirection.x), 0.0, 0.0);
        }
        else if (tNext.y <= tNext.z)
        {
            t = tNext.y;
            tNext.y += tDelta.y;
            subVoxel.y += stepDirection.y;
            normal = float3(0.0, -float(stepDirection.y), 0.0);
        }
        else
        {
            t = tNext.z;
            tNext.z += tDelta.z;
            subVoxel.z += stepDirection.z;
            normal = float3(0.0, 0.0, -float(stepDirection.z));
        }

        if (t >= tMax || any(subVoxel < 0) || any(subVoxel >= GLIMMER_SH_OCCUPANCY_SUB_VOXELS))
        {
            break;
        }
    }

    return false;
}

bool GlimmerSHTraceOccupancy(GlimmerSHOccupancyParams params, float3 origin, float3 direction, float tMax, out GlimmerSHOccupancyHit outHit, out float outCoveredT)
{
    outHit.t = tMax;
    outHit.normal = -direction;
    outHit.albedo = (float3)0.0;
    outHit.spacing = params.cascades[0].params.x;
    outHit.voxel = (int3)0;
    outHit.cascadeIndex = 0u;

    float t = 0.0;

    float3 safeDirection = direction;

    [unroll]
    for (uint axis = 0; axis < 3; axis++)
    {
        if (abs(safeDirection[axis]) < 1e-6)
        {
            safeDirection[axis] = 1e-6;
        }
    }

    const float3 invDirection = 1.0 / safeDirection;
    const int3 stepDirection = int3(sign(safeDirection));

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_OCCUPANCY_TRACED_CASCADES && t < tMax; cascadeIndex++)
    {
        const GlimmerSHOccupancyCascade cascade = params.cascades[cascadeIndex];

        // a cascade that doesn't cover where the ray is now leaves it to a coarser one
        if (!GlimmerSHOccupancyContains(cascade, origin + direction * t))
        {
            continue;
        }

        const float spacing = cascade.params.x;

        int3 voxel = int3(floor((origin + direction * t) * cascade.params.y));

        // distance along the ray to the next voxel boundary on each axis, and between boundaries
        const float3 nextBoundary = (float3(voxel + max(stepDirection, 0)) * spacing - origin) * invDirection;
        float3 tNext = max(nextBoundary, (float3)t);
        const float3 tDelta = abs(spacing * invDirection);

        float3 enteredNormal = -direction;

        [loop]
        for (uint stepIndex = 0; stepIndex < uint(GLIMMER_SH_OCCUPANCY_GRID_XZ * 2 + GLIMMER_SH_OCCUPANCY_GRID_Y); stepIndex++)
        {
            const int3 localVoxel = voxel - cascade.origin.xyz;

            if (any(localVoxel < 0) || any(localVoxel >= GlimmerSHOccupancyGridSize))
            {
                break;
            }

            const float4 occupancy = glimmerSHOccupancy.Load(int4(GlimmerSHOccupancyTexel(cascadeIndex, voxel), 0));

            if (occupancy.a > 0.5)
            {
                float3 hitNormal = enteredNormal;
                float hitT;

                if (GlimmerSHTraceOccupancyMask(cascadeIndex, voxel, spacing, origin, direction, invDirection, stepDirection, t, tMax, hitNormal, hitT))
                {
                    outHit.t = hitT;
                    outHit.normal = hitNormal;
                    outHit.albedo = occupancy.rgb;
                    outHit.spacing = spacing;
                    outHit.voxel = voxel;
                    outHit.cascadeIndex = cascadeIndex;
                    outCoveredT = hitT;

                    return true;
                }
            }

            // step along the axis whose boundary is nearest
            if (tNext.x <= tNext.y && tNext.x <= tNext.z)
            {
                t = tNext.x;
                tNext.x += tDelta.x;
                voxel.x += stepDirection.x;
                enteredNormal = float3(-float(stepDirection.x), 0.0, 0.0);
            }
            else if (tNext.y <= tNext.z)
            {
                t = tNext.y;
                tNext.y += tDelta.y;
                voxel.y += stepDirection.y;
                enteredNormal = float3(0.0, -float(stepDirection.y), 0.0);
            }
            else
            {
                t = tNext.z;
                tNext.z += tDelta.z;
                voxel.z += stepDirection.z;
                enteredNormal = float3(0.0, 0.0, -float(stepDirection.z));
            }

            if (t >= tMax)
            {
                break;
            }
        }
    }

    outCoveredT = min(t, tMax);

    return false;
}

#endif // GLIMMER_SH_OCCUPANCY_TRACE_HLSLI
