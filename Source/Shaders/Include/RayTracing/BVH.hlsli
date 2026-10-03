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

struct BVHBLASNode
{
    uint4 header; // xyz = asuint origin, w = bits 0-17 per axis scale exponents (6 bits each, biased by 32), bits 18-31 = bits 9-22 of the right ref
    uint4 data;   // xyz = child bounds bytes (left min, left max, right min, right max), w = left ref in bits 0-22, bits 0-8 of the right ref in bits 23-31
};

#define BVH_BLAS_REF_LEAF_BIT (1u << 22)
#define BVH_BLAS_REF_INDEX_MASK 0x3FFFFFu
#define BVH_BLAS_LEAF_END_FLAG 1u

struct BVHBLASChildren
{
    float3 boundsMin[2];
    float3 boundsMax[2];
    uint refs[2]; // BVH_BLAS_REF_LEAF_BIT set for leaves, index in BVH_BLAS_REF_INDEX_MASK: child node, or first triangle of the leaf
};

struct BVHTriangle
{
    float4 position0; // w = asuint flags
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

float3 UnpackBVHBLASBytes(uint word, uint firstByte)
{
    return float3((word >> (firstByte * 8u)) & 0xFFu, (word >> ((firstByte + 1u) * 8u)) & 0xFFu, (word >> ((firstByte + 2u) * 8u)) & 0xFFu);
}

BVHBLASChildren UnpackBVHBLASNode(BVHBLASNode node)
{
    const float3 origin = asfloat(node.header.xyz);

    // scale = 2^(exponent - 32)
    // the float exponent field is exponent - 32 + 127
    const uint3 exponents = (uint3(node.header.w, node.header.w >> 6u, node.header.w >> 12u) & 0x3Fu) + 95u;
    const float3 scale = asfloat(exponents << 23u);

    BVHBLASChildren children;

    children.boundsMin[0] = origin + UnpackBVHBLASBytes(node.data.x, 0u) * scale;
    children.boundsMax[0] = origin + float3(node.data.x >> 24u, node.data.y & 0xFFu, (node.data.y >> 8u) & 0xFFu) * scale;
    children.boundsMin[1] = origin + float3((node.data.y >> 16u) & 0xFFu, node.data.y >> 24u, node.data.z & 0xFFu) * scale;
    children.boundsMax[1] = origin + UnpackBVHBLASBytes(node.data.z, 1u) * scale;

    children.refs[0] = node.data.w & 0x7FFFFFu;
    children.refs[1] = (node.data.w >> 23u) | ((node.header.w >> 18u) << 9u);

    return children;
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
