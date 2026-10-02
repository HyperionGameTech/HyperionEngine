#ifndef GLIMMER_SWRT_HLSLI
#define GLIMMER_SWRT_HLSLI

#include "../../Include/RayTracing/BVH.hlsli"
#include "GlimmerSWRTCommon.hlsli"

#ifndef GLIMMER_SWRT_TLAS_STACK_SIZE
#define GLIMMER_SWRT_TLAS_STACK_SIZE 32
#endif

#ifndef GLIMMER_SWRT_BLAS_STACK_SIZE
#define GLIMMER_SWRT_BLAS_STACK_SIZE 32
#endif

#ifndef GLIMMER_SWRT_MAX_BLAS_STEPS
#define GLIMMER_SWRT_MAX_BLAS_STEPS 4096
#endif

#ifndef GLIMMER_SWRT_MAX_TLAS_STEPS
#define GLIMMER_SWRT_MAX_TLAS_STEPS 1024
#endif

#define GLIMMER_SWRT_INVALID_INDEX 0xFFFFFFFFu

struct GlimmerSWRTHit
{
    float t;
    uint instanceIndex;
    uint triangleIndex; // index into glimmerBLASTriangles
    float2 barycentrics;
    bool frontFace;
};

struct GlimmerSWRTStats
{
    uint nodeVisits;
    uint triangleTests;
    uint instanceVisits;
    uint stackOverflows;
};

GlimmerSWRTStats GlimmerMakeSWRTStats()
{
    GlimmerSWRTStats stats;
    stats.nodeVisits = 0;
    stats.triangleTests = 0;
    stats.instanceVisits = 0;
    stats.stackOverflows = 0;

    return stats;
}

float3 GlimmerWorldToObjectPoint(GlimmerInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.worldToObject0, homogeneous), dot(instance.worldToObject1, homogeneous), dot(instance.worldToObject2, homogeneous));
}

float3 GlimmerWorldToObjectVector(GlimmerInstance instance, float3 direction)
{
    return float3(dot(instance.worldToObject0.xyz, direction), dot(instance.worldToObject1.xyz, direction), dot(instance.worldToObject2.xyz, direction));
}

float3 GlimmerObjectToWorldNormal(GlimmerInstance instance, float3 normal)
{
    return normalize(instance.worldToObject0.xyz * normal.x + instance.worldToObject1.xyz * normal.y + instance.worldToObject2.xyz * normal.z);
}

float3 GlimmerGetHitNormal(GlimmerSWRTHit hit, float3 worldDirection)
{
    const GlimmerInstance instance = glimmerInstances[hit.instanceIndex];
    const BVHTriangle bvhTriangle = glimmerBLASTriangles[hit.triangleIndex];

    float3 normal = GlimmerObjectToWorldNormal(instance, cross(bvhTriangle.edge1.xyz, bvhTriangle.edge2.xyz));

    return dot(normal, worldDirection) > 0.0 ? -normal : normal;
}

void GlimmerTraverseBLAS(
    uint instanceIndex,
    float3 worldOrigin,
    float3 worldDirection,
    float tMin,
    bool acceptFirstHit,
    inout GlimmerSWRTHit hit,
    inout GlimmerSWRTStats stats)
{
    const GlimmerInstance instance = glimmerInstances[instanceIndex];

    const uint nodeBase = instance.data.x;
    const uint triangleBase = instance.data.y;

    const float3 origin = GlimmerWorldToObjectPoint(instance, worldOrigin);
    const float3 direction = GlimmerWorldToObjectVector(instance, worldDirection);
    const float3 inverseDirection = float3(GetBVHSafeInverse(direction.x), GetBVHSafeInverse(direction.y), GetBVHSafeInverse(direction.z));

    uint stack[GLIMMER_SWRT_BLAS_STACK_SIZE];
    uint stackSize = 0;

    uint nodeIndex = 0;

    [loop]
    for (uint blasStep = 0; blasStep < GLIMMER_SWRT_MAX_BLAS_STEPS; blasStep++)
    {
        stats.nodeVisits++;

        const BVHNode node = glimmerBLASNodes[nodeBase + nodeIndex];

        const uint childIndices[2] = { asuint(node.leftMinIndex.w), asuint(node.rightMinIndex.w) };
        const uint childCounts[2] = { asuint(node.leftMaxCount.w), asuint(node.rightMaxCount.w) };

        const float childDistances[2] = {
            IntersectBVHBounds(node.leftMinIndex.xyz, node.leftMaxCount.xyz, origin, inverseDirection, tMin, hit.t),
            IntersectBVHBounds(node.rightMinIndex.xyz, node.rightMaxCount.xyz, origin, inverseDirection, tMin, hit.t)
        };

        const uint nearChild = childDistances[1] < childDistances[0] ? 1u : 0u;

        // leaves are intersected as soon as their parent is visited, nearest first
        [unroll]
        for (uint order = 0; order < 2; order++)
        {
            const uint child = order == 0 ? nearChild : 1u - nearChild;

            if (childCounts[child] == 0u || childDistances[child] > hit.t)
            {
                continue;
            }

            const uint firstTriangle = triangleBase + childIndices[child];

            for (uint triangleIndex = firstTriangle; triangleIndex < firstTriangle + childCounts[child]; triangleIndex++)
            {
                stats.triangleTests++;

                const BVHTriangle bvhTriangle = glimmerBLASTriangles[triangleIndex];

                float distance;
                float2 barycentrics;

                if (IntersectBVHTriangle(bvhTriangle, origin, direction, tMin, hit.t, distance, barycentrics))
                {
                    const bool frontFace = dot(cross(bvhTriangle.edge1.xyz, bvhTriangle.edge2.xyz), direction) < 0.0;

                    hit.t = distance;
                    hit.instanceIndex = instanceIndex;
                    hit.triangleIndex = triangleIndex;
                    hit.barycentrics = barycentrics;
                    hit.frontFace = frontFace;

                    if (acceptFirstHit)
                    {
                        return;
                    }
                }
            }
        }

        // hits found in the leaves above can put an interior child out of range, misses are always out of range
        const bool traverseNear = childCounts[nearChild] == 0u && childDistances[nearChild] <= hit.t;
        const bool traverseFar = childCounts[1u - nearChild] == 0u && childDistances[1u - nearChild] <= hit.t;

        if (traverseNear && traverseFar)
        {
            if (stackSize < GLIMMER_SWRT_BLAS_STACK_SIZE)
            {
                stack[stackSize++] = childIndices[1u - nearChild];
            }
            else
            {
                stats.stackOverflows++;
            }

            nodeIndex = childIndices[nearChild];

            continue;
        }

        if (traverseNear || traverseFar)
        {
            nodeIndex = traverseNear ? childIndices[nearChild] : childIndices[1u - nearChild];

            continue;
        }

        if (stackSize == 0)
        {
            break;
        }

        nodeIndex = stack[--stackSize];
    }
}

bool TraceGlimmerSWRT(
    float3 origin,
    float3 direction,
    float tMin,
    float tMax,
    uint numInstances,
    bool acceptFirstHit,
    out GlimmerSWRTHit hit,
    inout GlimmerSWRTStats stats)
{
    hit.t = tMax;
    hit.instanceIndex = GLIMMER_SWRT_INVALID_INDEX;
    hit.triangleIndex = GLIMMER_SWRT_INVALID_INDEX;
    hit.barycentrics = float2(0.0, 0.0);
    hit.frontFace = true;

    if (numInstances == 0u)
    {
        return false;
    }

    const float3 inverseDirection = float3(GetBVHSafeInverse(direction.x), GetBVHSafeInverse(direction.y), GetBVHSafeInverse(direction.z));

    uint stack[GLIMMER_SWRT_TLAS_STACK_SIZE];
    uint stackSize = 0;

    uint nodeIndex = 0;

    [loop]
    for (uint tlasStep = 0; tlasStep < GLIMMER_SWRT_MAX_TLAS_STEPS; tlasStep++)
    {
        stats.nodeVisits++;

        const BVHNode node = glimmerTLASNodes[nodeIndex];

        const uint childIndices[2] = { asuint(node.leftMinIndex.w), asuint(node.rightMinIndex.w) };
        const uint childCounts[2] = { asuint(node.leftMaxCount.w), asuint(node.rightMaxCount.w) };

        const float childDistances[2] = {
            IntersectBVHBounds(node.leftMinIndex.xyz, node.leftMaxCount.xyz, origin, inverseDirection, tMin, hit.t),
            IntersectBVHBounds(node.rightMinIndex.xyz, node.rightMaxCount.xyz, origin, inverseDirection, tMin, hit.t)
        };

        const uint nearChild = childDistances[1] < childDistances[0] ? 1u : 0u;

        [loop]
        for (uint order = 0; order < 2; order++)
        {
            const uint child = order == 0 ? nearChild : 1u - nearChild;

            if (childCounts[child] == 0u || childDistances[child] > hit.t)
            {
                continue;
            }

            for (uint instanceIndex = childIndices[child]; instanceIndex < childIndices[child] + childCounts[child]; instanceIndex++)
            {
                stats.instanceVisits++;

                GlimmerTraverseBLAS(instanceIndex, origin, direction, tMin, acceptFirstHit, hit, stats);

                if (acceptFirstHit && hit.instanceIndex != GLIMMER_SWRT_INVALID_INDEX)
                {
                    return true;
                }
            }
        }

        const bool traverseNear = childCounts[nearChild] == 0u && childDistances[nearChild] <= hit.t;
        const bool traverseFar = childCounts[1u - nearChild] == 0u && childDistances[1u - nearChild] <= hit.t;

        if (traverseNear && traverseFar)
        {
            if (stackSize < GLIMMER_SWRT_TLAS_STACK_SIZE)
            {
                stack[stackSize++] = childIndices[1u - nearChild];
            }
            else
            {
                stats.stackOverflows++;
            }

            nodeIndex = childIndices[nearChild];

            continue;
        }

        if (traverseNear || traverseFar)
        {
            nodeIndex = traverseNear ? childIndices[nearChild] : childIndices[1u - nearChild];

            continue;
        }

        if (stackSize == 0)
        {
            break;
        }

        nodeIndex = stack[--stackSize];
    }

    return hit.instanceIndex != GLIMMER_SWRT_INVALID_INDEX;
}

#endif
