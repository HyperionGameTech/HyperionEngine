#ifndef HYP_BVH
#define HYP_BVH

#include "../Octahedron.hlsli"

// Must match the layouts in Engine/Baking/PathTracer/PathTracerBVH.hpp

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

#define BVH_INVALID_INDEX 0xFFFFFFFFu

float3 UnpackBVHNormal(uint packedNormal)
{
    return UnpackOctahedralSnorm16x2(packedNormal);
}

#endif
