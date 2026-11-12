#ifndef GLIMMER_BVH_TRACE_HLSLI
#define GLIMMER_BVH_TRACE_HLSLI

#include "../../Include/RayTracing/BVH.hlsli"
#include "../GlimmerCommon.hlsli"
#include "GlimmerBVHCommon.hlsli"

#ifndef GLIMMER_BVH_TLAS_STACK_SIZE
#define GLIMMER_BVH_TLAS_STACK_SIZE 32
#endif

#ifndef GLIMMER_BVH_BLAS_STACK_SIZE
#define GLIMMER_BVH_BLAS_STACK_SIZE 32
#endif

#ifndef GLIMMER_BVH_MAX_BLAS_STEPS
#define GLIMMER_BVH_MAX_BLAS_STEPS 4096
#endif

#ifndef GLIMMER_BVH_MAX_TLAS_STEPS
#define GLIMMER_BVH_MAX_TLAS_STEPS 1024
#endif
#ifndef GLIMMER_BVH_MAX_RAY_WORK
#define GLIMMER_BVH_MAX_RAY_WORK 65536
#endif

#define GLIMMER_BVH_INVALID_INDEX 0xFFFFFFFFu

struct GlimmerBVHHit
{
    float t;
    uint instanceIndex;
    uint triangleIndex;
    float2 barycentrics;
    bool frontFace;
};

struct GlimmerBVHStats
{
    uint nodeVisits;
    uint triangleTests;
    uint instanceVisits;
    uint stackOverflows;
};

GlimmerBVHStats GlimmerMakeBVHStats()
{
    GlimmerBVHStats stats;
    stats.nodeVisits = 0;
    stats.triangleTests = 0;
    stats.instanceVisits = 0;
    stats.stackOverflows = 0;

    return stats;
}

float3 GlimmerWorldToObjectPoint(GlimmerTracedInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.worldToObject0, homogeneous), dot(instance.worldToObject1, homogeneous), dot(instance.worldToObject2, homogeneous));
}

float3 GlimmerWorldToObjectVector(GlimmerTracedInstance instance, float3 direction)
{
    return float3(dot(instance.worldToObject0.xyz, direction), dot(instance.worldToObject1.xyz, direction), dot(instance.worldToObject2.xyz, direction));
}

float3 GlimmerObjectToWorldNormal(GlimmerTracedInstance instance, float3 normal)
{
    return normalize(instance.worldToObject0.xyz * normal.x + instance.worldToObject1.xyz * normal.y + instance.worldToObject2.xyz * normal.z);
}

float3 GlimmerGetHitNormal(GlimmerBVHHit hit, float3 worldDirection)
{
    const GlimmerTracedInstance instance = glimmerTracedInstances[hit.instanceIndex];
    const BVHBLASTriangle blasTriangle = LOAD_BVH_BLAS_TRIANGLE(glimmerTriangles, hit.triangleIndex);

    float3 normal = GlimmerObjectToWorldNormal(
        instance,
        cross(
            blasTriangle.position1 - blasTriangle.position0,
            blasTriangle.position2 - blasTriangle.position0));

    return dot(normal, worldDirection) > 0.0 ? -normal : normal;
}

void GlimmerTraverseBLAS(
    uint instanceIndex,
    float3 worldOrigin,
    float3 worldDirection,
    float tMin,
    bool acceptFirstHit,
    inout GlimmerBVHHit hit,
    inout GlimmerBVHStats stats,
    inout uint workLeft)
{
    const GlimmerTracedInstance instance = glimmerTracedInstances[instanceIndex];

    const uint nodeBase = instance.data.x;
    const uint triangleBase = instance.data.y;

    const float3 origin = GlimmerWorldToObjectPoint(instance, worldOrigin);
    const float3 direction = GlimmerWorldToObjectVector(instance, worldDirection);
    const float3 inverseDirection = float3(GetBVHSafeInverse(direction.x), GetBVHSafeInverse(direction.y), GetBVHSafeInverse(direction.z));

    uint stack[GLIMMER_BVH_BLAS_STACK_SIZE];
    uint stackSize = 0;

    uint nodeIndex = 0;

    [loop]
    for (uint blasStep = 0; blasStep < GLIMMER_BVH_MAX_BLAS_STEPS && workLeft != 0u; blasStep++)
    {
        stats.nodeVisits++;
        workLeft--;

        const BVHBLASChildren children = UnpackBVHBLASNode(glimmerBLASNodes[nodeBase + nodeIndex]);

        const uint childIndices[2] = { children.refs[0] & BVH_BLAS_REF_INDEX_MASK, children.refs[1] & BVH_BLAS_REF_INDEX_MASK };
        const bool childIsLeaf[2] = { (children.refs[0] & BVH_BLAS_REF_LEAF_BIT) != 0u, (children.refs[1] & BVH_BLAS_REF_LEAF_BIT) != 0u };

        const float childDistances[2] = {
            IntersectBVHBounds(children.boundsMin[0], children.boundsMax[0], origin, inverseDirection, tMin, hit.t),
            IntersectBVHBounds(children.boundsMin[1], children.boundsMax[1], origin, inverseDirection, tMin, hit.t)
        };

        const uint nearChild = childDistances[1] < childDistances[0] ? 1u : 0u;

        // leaves are intersected as soon as their parent is visited, nearest first
        [unroll]
        for (uint order = 0; order < 2; order++)
        {
            const uint child = order == 0 ? nearChild : 1u - nearChild;

            const bool visitLeaf = childIsLeaf[child] && childDistances[child] <= hit.t;

            // a leaf's triangles run up to the one flagged as its end
            for (uint triangleIndex = triangleBase + childIndices[child]; visitLeaf && workLeft != 0u; triangleIndex++)
            {
                stats.triangleTests++;
                workLeft--;

                const BVHBLASTriangle blasTriangle = LOAD_BVH_BLAS_TRIANGLE(glimmerTriangles, triangleIndex);
                const BVHTriangle bvhTriangle = MakeBVHTriangle(blasTriangle);

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

                if (blasTriangle.isLeafEnd)
                {
                    break;
                }
            }
        }

        const bool traverseNear = !childIsLeaf[nearChild] && childDistances[nearChild] <= hit.t;
        const bool traverseFar = !childIsLeaf[1u - nearChild] && childDistances[1u - nearChild] <= hit.t;

        if (traverseNear && traverseFar)
        {
            if (stackSize < GLIMMER_BVH_BLAS_STACK_SIZE)
            {
                stack[stackSize++] = childIndices[1u - nearChild];
            }
            else
            {
                stats.stackOverflows++;
            }

            nodeIndex = childIndices[nearChild];
        }
        else if (traverseNear || traverseFar)
        {
            nodeIndex = traverseNear ? childIndices[nearChild] : childIndices[1u - nearChild];
        }
        else if (stackSize == 0)
        {
            break;
        }
        else
        {
            nodeIndex = stack[--stackSize];
        }
    }
}

// where the ray leaves the traced instances' bounds, so the BVH owns the solids up to there and the spans past it; 0 when it starts outside them
float GlimmerBVHWindowExit(float3 origin, float3 direction, uint numInstances)
{
    if (numInstances == 0u)
    {
        return 0.0;
    }

    const BVHNode root = glimmerTLASNodes[0];

    const float3 boundsMin = min(root.leftMinIndex.xyz, root.rightMinIndex.xyz);
    const float3 boundsMax = max(root.leftMaxCount.xyz, root.rightMaxCount.xyz);

    if (any(origin < boundsMin) || any(origin > boundsMax))
    {
        return 0.0;
    }

    const float3 safeDirection = select(abs(direction) < 1e-8, (float3)1e-8, direction);
    const float3 exits = max((boundsMin - origin) / safeDirection, (boundsMax - origin) / safeDirection);

    return min(exits.x, min(exits.y, exits.z));
}

bool TraceGlimmerBVH(
    float3 origin,
    float3 direction,
    float tMin,
    float tMax,
    uint numInstances,
    bool acceptFirstHit,
    out GlimmerBVHHit hit,
    inout GlimmerBVHStats stats)
{
    hit.t = tMax;
    hit.instanceIndex = GLIMMER_BVH_INVALID_INDEX;
    hit.triangleIndex = GLIMMER_BVH_INVALID_INDEX;
    hit.barycentrics = float2(0.0, 0.0);
    hit.frontFace = true;

    if (numInstances == 0u)
    {
        return false;
    }

    uint workLeft = GLIMMER_BVH_MAX_RAY_WORK;

    const float3 inverseDirection = float3(GetBVHSafeInverse(direction.x), GetBVHSafeInverse(direction.y), GetBVHSafeInverse(direction.z));

    uint stack[GLIMMER_BVH_TLAS_STACK_SIZE];
    uint stackSize = 0;

    uint nodeIndex = 0;

    [loop]
    for (uint tlasStep = 0; tlasStep < GLIMMER_BVH_MAX_TLAS_STEPS && workLeft != 0u; tlasStep++)
    {
        stats.nodeVisits++;
        workLeft--;

        const BVHNode node = glimmerTLASNodes[nodeIndex];

        const uint childIndices[2] = { asuint(node.leftMinIndex.w), asuint(node.rightMinIndex.w) };
        const uint childCounts[2] = { asuint(node.leftMaxCount.w), asuint(node.rightMaxCount.w) };

        const float childDistances[2] = {
            IntersectBVHBounds(node.leftMinIndex.xyz, node.leftMaxCount.xyz, origin, inverseDirection, tMin, hit.t),
            IntersectBVHBounds(node.rightMinIndex.xyz, node.rightMaxCount.xyz, origin, inverseDirection, tMin, hit.t)
        };

        const uint nearChild = childDistances[1] < childDistances[0] ? 1u : 0u;
        
        for (uint order = 0; order < 2; order++)
        {
            const uint child = order == 0 ? nearChild : 1u - nearChild;

            const bool visitLeaf = childCounts[child] != 0u && childDistances[child] <= hit.t;

            for (uint instanceIndex = childIndices[child]; visitLeaf && instanceIndex < childIndices[child] + childCounts[child] && workLeft != 0u; instanceIndex++)
            {
                stats.instanceVisits++;

                GlimmerTraverseBLAS(instanceIndex, origin, direction, tMin, acceptFirstHit, hit, stats, workLeft);

                if (acceptFirstHit && hit.instanceIndex != GLIMMER_BVH_INVALID_INDEX)
                {
                    return true;
                }
            }
        }

        const bool traverseNear = childCounts[nearChild] == 0u && childDistances[nearChild] <= hit.t;
        const bool traverseFar = childCounts[1u - nearChild] == 0u && childDistances[1u - nearChild] <= hit.t;

        if (traverseNear && traverseFar)
        {
            if (stackSize < GLIMMER_BVH_TLAS_STACK_SIZE)
            {
                stack[stackSize++] = childIndices[1u - nearChild];
            }
            else
            {
                stats.stackOverflows++;
            }

            nodeIndex = childIndices[nearChild];
        }
        else if (traverseNear || traverseFar)
        {
            nodeIndex = traverseNear ? childIndices[nearChild] : childIndices[1u - nearChild];
        }
        else if (stackSize == 0)
        {
            break;
        }
        else
        {
            nodeIndex = stack[--stackSize];
        }
    }

    return hit.instanceIndex != GLIMMER_BVH_INVALID_INDEX;
}

#endif
