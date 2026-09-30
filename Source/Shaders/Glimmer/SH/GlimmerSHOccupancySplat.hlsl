#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/RayTracing/BVH.hlsli"
#include "../GlimmerCommon.hlsli"

#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "GlimmerSHOccupancy.hlsli"

PERMUTE(MODE, CLEAR, SPLAT)

// Must match GlimmerSHOccupancySplatConstants in GlimmerSHOccupancy.cpp
struct GlimmerSHOccupancySplatConstants
{
    int4 origin;   // xyz = absolute voxel of the cascade's window, w = cascade
    float4 params; // x = voxel spacing, y = 1 / spacing
    uint4 counts;  // x = span instances, y = span triangles, z = groups along x
};

DECLARE_BUFFER_DYNAMIC(GlimmerSHOccupancySplat, CBuffer) cbuffer CBuffer
{
    GlimmerSHOccupancySplatConstants constants;
};

DECLARE_UAV(GlimmerSHOccupancySplat, OutOccupancy) RWTexture3D<float4> OutOccupancy;
DECLARE_SRV(GlimmerSHOccupancySplat, SpanInstancesBuffer) StructuredBuffer<GlimmerSpanInstance> spanInstances;
DECLARE_SRV(GlimmerSHOccupancySplat, SpanTriangleOffsetsBuffer) StructuredBuffer<uint> spanTriangleOffsets;
DECLARE_SRV(GlimmerSHOccupancySplat, GlimmerBLASTrianglesBuffer) StructuredBuffer<BVHTriangle> glimmerBLASTriangles;
DECLARE_SRV(GlimmerSHOccupancySplat, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerSHOccupancySplat, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

#include "../GlimmerMaterial.hlsli"

#define GROUP_SIZE 64

// a triangle crossing more voxels than this (a huge floor or wall at a fine cascade) is only filled this far
#define MAX_VOXELS_PER_TRIANGLE 8192

float3 TransformPoint(GlimmerSpanInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.objectToWorld0, homogeneous), dot(instance.objectToWorld1, homogeneous), dot(instance.objectToWorld2, homogeneous));
}

uint FindSpanInstance(uint triangleIndex)
{
    uint low = 0;
    uint high = constants.counts.x;

    // last instance whose first triangle is at or before triangleIndex
    while (high - low > 1u)
    {
        const uint middle = (low + high) / 2u;

        if (spanTriangleOffsets[middle] <= triangleIndex)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }

    return low;
}

// Conservative enough triangle / box overlap: the box straddles the triangle's plane, and its centre lies inside every edge
// once the edge is pushed out by the box's extent along the edge's in-plane normal
bool TriangleOverlapsBox(float3 p0, float3 p1, float3 p2, float3 normal, float3 center, float3 halfExtent)
{
    if (abs(dot(normal, center - p0)) > dot(abs(normal), halfExtent))
    {
        return false;
    }

    const float3 corners[3] = { p0, p1, p2 };

    [unroll]
    for (uint edgeIndex = 0; edgeIndex < 3; edgeIndex++)
    {
        const float3 a = corners[edgeIndex];
        const float3 b = corners[(edgeIndex + 1) % 3];
        const float3 opposite = corners[(edgeIndex + 2) % 3];

        float3 inward = cross(normal, b - a);
        inward *= dot(inward, opposite - a) < 0.0 ? -1.0 : 1.0;

        if (dot(inward, center - a) < -dot(abs(inward), halfExtent))
        {
            return false;
        }
    }

    return true;
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint threadIndex = (groupId.y * constants.counts.z + groupId.x) * GROUP_SIZE + groupIndex;
    const uint cascadeIndex = uint(constants.origin.w);

#if defined(MODE_CLEAR)
    const uint voxelsPerSlab = GLIMMER_SH_OCCUPANCY_GRID_XZ * GLIMMER_SH_OCCUPANCY_GRID_Y * GLIMMER_SH_OCCUPANCY_GRID_XZ;

    if (threadIndex >= voxelsPerSlab)
    {
        return;
    }

    const int3 localVoxel = int3(
        threadIndex % GLIMMER_SH_OCCUPANCY_GRID_XZ,
        (threadIndex / GLIMMER_SH_OCCUPANCY_GRID_XZ) % GLIMMER_SH_OCCUPANCY_GRID_Y,
        threadIndex / (GLIMMER_SH_OCCUPANCY_GRID_XZ * GLIMMER_SH_OCCUPANCY_GRID_Y));

    OutOccupancy[GlimmerSHOccupancyTexel(cascadeIndex, localVoxel)] = (float4)0.0;
#elif defined(MODE_SPLAT)
    if (threadIndex >= constants.counts.y)
    {
        return;
    }

    const uint instanceIndex = FindSpanInstance(threadIndex);
    const GlimmerSpanInstance instance = spanInstances[instanceIndex];

    // leaves are the canopy spans' business: they dim light rather than stop it
    if ((instance.data.w & GLIMMER_INSTANCE_FLAG_FOLIAGE) != 0u)
    {
        return;
    }

    const uint localTriangle = threadIndex - spanTriangleOffsets[instanceIndex];

    if (localTriangle >= instance.data.y)
    {
        return;
    }

    const float4 albedoAlpha = GlimmerGetMaterialAverageAlbedoAlpha(instance.data.z);

    // mostly cut out alpha tested cards (chains, grilles) let most light through
    if ((instance.data.w & GLIMMER_INSTANCE_FLAG_ALPHA_TESTED) != 0u && albedoAlpha.a < 0.5)
    {
        return;
    }

    const BVHTriangle bvhTriangle = glimmerBLASTriangles[instance.data.x + localTriangle];

    const float3 p0 = TransformPoint(instance, bvhTriangle.position0.xyz);
    const float3 p1 = TransformPoint(instance, bvhTriangle.position0.xyz + bvhTriangle.edge1.xyz);
    const float3 p2 = TransformPoint(instance, bvhTriangle.position0.xyz + bvhTriangle.edge2.xyz);

    const float3 normal = cross(p1 - p0, p2 - p0);

    if (dot(normal, normal) < 1e-12)
    {
        return;
    }

    const float spacing = constants.params.x;
    const float invSpacing = constants.params.y;

    const int3 windowMin = constants.origin.xyz;
    const int3 windowMax = windowMin + GlimmerSHOccupancyGridSize - 1;

    const int3 voxelMin = max(int3(floor(min(p0, min(p1, p2)) * invSpacing)), windowMin);
    const int3 voxelMax = min(int3(floor(max(p0, max(p1, p2)) * invSpacing)), windowMax);

    if (any(voxelMax < voxelMin))
    {
        return;
    }

    const float4 value = float4(saturate(albedoAlpha.rgb), 1.0);
    const float3 halfExtent = (float3)(0.5 * spacing);

    uint numVisited = 0;

    for (int z = voxelMin.z; z <= voxelMax.z && numVisited < MAX_VOXELS_PER_TRIANGLE; z++)
    {
        for (int y = voxelMin.y; y <= voxelMax.y && numVisited < MAX_VOXELS_PER_TRIANGLE; y++)
        {
            for (int x = voxelMin.x; x <= voxelMax.x && numVisited < MAX_VOXELS_PER_TRIANGLE; x++)
            {
                numVisited++;

                const int3 voxel = int3(x, y, z);
                const float3 center = (float3(voxel) + 0.5) * spacing;

                if (TriangleOverlapsBox(p0, p1, p2, normal, center, halfExtent))
                {
                    OutOccupancy[GlimmerSHOccupancyTexel(cascadeIndex, voxel - windowMin)] = value;
                }
            }
        }
    }
#endif
}
