#ifndef HYP_BVH_TRAVERSAL
#define HYP_BVH_TRAVERSAL

#include "BVH.hlsli"

#define BVH_STACK_SIZE 64

bool IntersectBVHLeaf(
    uint firstTriangle,
    uint triangleCount,
    float3 origin,
    float3 direction,
    float tMin,
    bool acceptFirstHit,
    inout BVHHit hit)
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
bool TraceBVH(
    float3 origin,
    float3 direction,
    float tMin,
    float tMax,
    bool acceptFirstHit,
    out BVHHit hit)
{
    hit.distance = tMax;
    hit.triangleIndex = BVH_INVALID_INDEX;
    hit.barycentrics = float2(0.0, 0.0);

    const float3 inverseDirection = float3(
        GetBVHSafeInverse(direction.x),
        GetBVHSafeInverse(direction.y),
        GetBVHSafeInverse(direction.z));

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
