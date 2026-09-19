#ifndef HYP_BVH_TRAVERSAL
#define HYP_BVH_TRAVERSAL

#include "BVH.hlsli"

#define BVH_STACK_SIZE 64
#define BVH_MISS_DISTANCE 3.402823466e+38f

float GetBVHSafeInverse(float value)
{
    return 1.0 / (abs(value) > 1e-12 ? value : (value >= 0.0 ? 1e-12 : -1e-12));
}

float IntersectBVHBounds(float3 boundsMin, float3 boundsMax, float3 origin, float3 inverseDirection, float tMin, float tMax)
{
    const float3 t0 = (boundsMin - origin) * inverseDirection;
    const float3 t1 = (boundsMax - origin) * inverseDirection;

    const float3 tNearAxis = min(t0, t1);
    const float3 tFarAxis = max(t0, t1);

    const float tNear = max(max(tNearAxis.x, tNearAxis.y), max(tNearAxis.z, tMin));
    const float tFar = min(min(tFarAxis.x, tFarAxis.y), min(tFarAxis.z, tMax));

    return tNear <= tFar ? tNear : BVH_MISS_DISTANCE;
}

bool IntersectBVHTriangle(BVHTriangle bvhTriangle, float3 origin, float3 direction, float tMin, float tMax, out float outDistance, out float2 outBarycentrics)
{
    outDistance = 0.0;
    outBarycentrics = float2(0.0, 0.0);

    const float3 edge1 = bvhTriangle.edge1.xyz;
    const float3 edge2 = bvhTriangle.edge2.xyz;

    const float3 directionCrossEdge2 = cross(direction, edge2);
    const float determinant = dot(edge1, directionCrossEdge2);

    if (abs(determinant) < 1e-20)
    {
        return false;
    }

    const float inverseDeterminant = 1.0 / determinant;

    const float3 originToPosition0 = origin - bvhTriangle.position0.xyz;
    const float u = dot(originToPosition0, directionCrossEdge2) * inverseDeterminant;

    if (u < 0.0 || u > 1.0)
    {
        return false;
    }

    const float3 originCrossEdge1 = cross(originToPosition0, edge1);
    const float v = dot(direction, originCrossEdge1) * inverseDeterminant;

    if (v < 0.0 || u + v > 1.0)
    {
        return false;
    }

    outDistance = dot(edge2, originCrossEdge1) * inverseDeterminant;
    outBarycentrics = float2(u, v);

    return outDistance > tMin && outDistance < tMax;
}

bool IntersectBVHLeaf(uint firstTriangle, uint triangleCount, float3 origin, float3 direction, float tMin, bool acceptFirstHit, inout BVHHit hit)
{
    bool foundHit = false;

    for (uint triangleIndex = firstTriangle; triangleIndex < firstTriangle + triangleCount; triangleIndex++)
    {
        float distance;
        float2 barycentrics;

        if (IntersectBVHTriangle(bvhTriangles[triangleIndex], origin, direction, tMin, hit.distance, distance, barycentrics))
        {
            hit.distance = distance;
            hit.triangleIndex = triangleIndex;
            hit.barycentrics = barycentrics;

            foundHit = true;

            if (acceptFirstHit)
            {
                break;
            }
        }
    }

    return foundHit;
}

// Leaf children are intersected as soon as their parent is visited, only interior children go through the stack.
bool TraceBVH(float3 origin, float3 direction, float tMin, float tMax, bool acceptFirstHit, out BVHHit hit)
{
    hit.distance = tMax;
    hit.triangleIndex = BVH_INVALID_INDEX;
    hit.barycentrics = float2(0.0, 0.0);

    const float3 inverseDirection = float3(GetBVHSafeInverse(direction.x), GetBVHSafeInverse(direction.y), GetBVHSafeInverse(direction.z));

    uint stack[BVH_STACK_SIZE];
    uint stackSize = 0;

    uint nodeIndex = 0;

    [loop]
    while (true)
    {
        const BVHNode node = bvhNodes[nodeIndex];

        const uint leftIndex = asuint(node.leftMinIndex.w);
        const uint leftCount = asuint(node.leftMaxCount.w);
        const uint rightIndex = asuint(node.rightMinIndex.w);
        const uint rightCount = asuint(node.rightMaxCount.w);

        const float leftDistance = IntersectBVHBounds(node.leftMinIndex.xyz, node.leftMaxCount.xyz, origin, inverseDirection, tMin, hit.distance);
        const float rightDistance = IntersectBVHBounds(node.rightMinIndex.xyz, node.rightMaxCount.xyz, origin, inverseDirection, tMin, hit.distance);

        if (leftCount != 0 && leftDistance <= hit.distance)
        {
            if (IntersectBVHLeaf(leftIndex, leftCount, origin, direction, tMin, acceptFirstHit, hit) && acceptFirstHit)
            {
                return true;
            }
        }

        if (rightCount != 0 && rightDistance <= hit.distance)
        {
            if (IntersectBVHLeaf(rightIndex, rightCount, origin, direction, tMin, acceptFirstHit, hit) && acceptFirstHit)
            {
                return true;
            }
        }

        // hits found in the leaves above can put an interior child out of range, misses are always out of range
        const bool traverseLeft = leftCount == 0 && leftDistance <= hit.distance;
        const bool traverseRight = rightCount == 0 && rightDistance <= hit.distance;

        if (traverseLeft && traverseRight)
        {
            const bool leftIsNearer = leftDistance <= rightDistance;

            if (stackSize < BVH_STACK_SIZE)
            {
                stack[stackSize++] = leftIsNearer ? rightIndex : leftIndex;
            }

            nodeIndex = leftIsNearer ? leftIndex : rightIndex;

            continue;
        }

        if (traverseLeft)
        {
            nodeIndex = leftIndex;

            continue;
        }

        if (traverseRight)
        {
            nodeIndex = rightIndex;

            continue;
        }

        if (stackSize == 0)
        {
            break;
        }

        nodeIndex = stack[--stackSize];
    }

    return hit.triangleIndex != BVH_INVALID_INDEX;
}

#endif
