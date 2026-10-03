#ifndef GLIMMER_SH_OCCUPANCY_HLSLI
#define GLIMMER_SH_OCCUPANCY_HLSLI

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

static const int3 GlimmerSHOccupancyGridSize = int3(GLIMMER_SH_OCCUPANCY_GRID_XZ, GLIMMER_SH_OCCUPANCY_GRID_Y, GLIMMER_SH_OCCUPANCY_GRID_XZ);

uint3 GlimmerSHOccupancyTexel(uint cascadeIndex, int3 localVoxel)
{
    return uint3(localVoxel.x, cascadeIndex * GLIMMER_SH_OCCUPANCY_GRID_Y + uint(localVoxel.y), localVoxel.z);
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
};

bool GlimmerSHOccupancyContains(GlimmerSHOccupancyCascade cascade, float3 P)
{
    const int3 localVoxel = int3(floor(P * cascade.params.y)) - cascade.origin.xyz;

    return cascade.origin.w != 0 && all(localVoxel >= 0) && all(localVoxel < GlimmerSHOccupancyGridSize);
}

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

        const int3 localVoxel = int3(floor(P * cascade.params.y)) - cascade.origin.xyz;

        return glimmerSHOccupancy.Load(int4(GlimmerSHOccupancyTexel(cascadeIndex, localVoxel), 0)).a > 0.5;
    }

    return false;
}

bool GlimmerSHTraceOccupancy(GlimmerSHOccupancyParams params, float3 origin, float3 direction, float tMax, out GlimmerSHOccupancyHit outHit, out float outCoveredT)
{
    outHit.t = tMax;
    outHit.normal = -direction;
    outHit.albedo = (float3)0.0;
    outHit.spacing = params.cascades[0].params.x;

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

            const float4 occupancy = glimmerSHOccupancy.Load(int4(GlimmerSHOccupancyTexel(cascadeIndex, localVoxel), 0));

            if (occupancy.a > 0.5)
            {
                outHit.t = t;
                outHit.normal = enteredNormal;
                outHit.albedo = occupancy.rgb;
                outHit.spacing = spacing;
                outCoveredT = t;

                return true;
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
