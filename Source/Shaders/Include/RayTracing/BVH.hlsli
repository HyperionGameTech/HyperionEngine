#ifndef HYP_BVH
#define HYP_BVH

#include "../Octahedron.hlsli"

#define BVH_INVALID_INDEX 0xFFFFFFFFu
#define BVH_MISS_DISTANCE 3.402823466e+38f

struct BVHNode
{
    float4 leftMinIndex;  // w = asuint: child node index, or first triangle when the child is a leaf
    float4 leftMaxCount;  // w = asuint: leaf triangle count, 0 when the child is an interior node
    float4 rightMinIndex;
    float4 rightMaxCount;
};

struct BVHTriangle
{
    float4 position0;
    float4 edge1;
    float4 edge2;
};

struct BVHTriangleAttributes
{
    uint4 packedNormalsMaterialIndex; // xyz = octahedral snorm16x2 world space normals, w = material index
    float4 texcoord0Texcoord1;
    float4 texcoord2;
};

struct BVHHit
{
    float distance;
    uint triangleIndex;
    float2 barycentrics;
};

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

float3 UnpackBVHNormal(uint packedNormal)
{
    return UnpackOctahedralSnorm16x2(packedNormal);
}

#endif
