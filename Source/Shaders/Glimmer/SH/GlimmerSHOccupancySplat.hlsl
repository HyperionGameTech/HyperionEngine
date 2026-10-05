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

struct GlimmerSHOccupancySplatConstants
{
    int4 origin;   // xyz = absolute voxel of the cascade's window, w = cascade
    float4 params; // x = voxel spacing, y = 1 / spacing
    uint4 counts;  // x = span instances, y = span chunks, z = groups along x
    int4 boxMin;   // xyz = absolute voxel; only voxels in the box are cleared and splatted
    int4 boxMax;   // xyz = absolute voxel, exclusive
};

DECLARE_BUFFER_DYNAMIC(GlimmerSHOccupancySplat, CBuffer) cbuffer CBuffer
{
    GlimmerSHOccupancySplatConstants constants;
};

DECLARE_UAV(GlimmerSHOccupancySplat, OutOccupancy) RWTexture3D<float4> OutOccupancy;
DECLARE_UAV(GlimmerSHOccupancySplat, OutOccupancyMask) RWStructuredBuffer<uint> OutOccupancyMask;
DECLARE_SRV(GlimmerSHOccupancySplat, SpanInstancesBuffer) StructuredBuffer<GlimmerSpanInstance> spanInstances;
DECLARE_SRV(GlimmerSHOccupancySplat, SpanChunksBuffer) StructuredBuffer<GlimmerSpanChunk> spanChunks;
DECLARE_SRV(GlimmerSHOccupancySplat, GlimmerBLASTrianglesBuffer) StructuredBuffer<uint> glimmerBLASTriangles;
DECLARE_SRV(GlimmerSHOccupancySplat, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerSHOccupancySplat, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

#include "../GlimmerMaterial.hlsli"

#define GROUP_SIZE 64
#define BACK_BIAS 0.01

float GetComponent(float3 value, uint axis)
{
    return select(axis == 0u, value.x, select(axis == 1u, value.y, value.z));
}

int GetComponent(int3 value, uint axis)
{
    return select(axis == 0u, value.x, select(axis == 1u, value.y, value.z));
}

float3 TransformPoint(GlimmerSpanInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.objectToWorld0, homogeneous), dot(instance.objectToWorld1, homogeneous), dot(instance.objectToWorld2, homogeneous));
}

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

void SplatTriangle(GlimmerSpanInstance instance, uint localTriangle, uint cascadeIndex)
{
    const float4 albedoAlpha = GlimmerGetMaterialAverageAlbedoAlpha(instance.data.z);

    // mostly cut out alpha tested cards (chains, grilles) let most light through
    if ((instance.data.w & GLIMMER_INSTANCE_FLAG_ALPHA_TESTED) != 0u && albedoAlpha.a < 0.5)
    {
        return;
    }

    const BVHBLASTriangle bvhTriangle = LOAD_BVH_BLAS_TRIANGLE(glimmerBLASTriangles, instance.data.x + localTriangle);

    float3 p0 = TransformPoint(instance, bvhTriangle.position0);
    float3 p1 = TransformPoint(instance, bvhTriangle.position1);
    float3 p2 = TransformPoint(instance, bvhTriangle.position2);

    const float3 normal = cross(p1 - p0, p2 - p0);

    if (dot(normal, normal) < 1e-12)
    {
        return;
    }

    const float spacing = constants.params.x / float(GLIMMER_SH_OCCUPANCY_SUB_VOXELS);
    const float invSpacing = constants.params.y * float(GLIMMER_SH_OCCUPANCY_SUB_VOXELS);

    if ((instance.data.w & GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED) == 0u)
    {
        const float facing = (instance.data.w & GLIMMER_INSTANCE_FLAG_MIRRORED) != 0u ? -1.0 : 1.0;
        const float3 behind = normalize(normal) * (-facing * BACK_BIAS * spacing);

        p0 += behind;
        p1 += behind;
        p2 += behind;
    }

    const int3 windowMin = constants.origin.xyz;
    const int3 boxMin = max(windowMin, constants.boxMin.xyz) * GLIMMER_SH_OCCUPANCY_SUB_VOXELS;
    const int3 boxMax = min(windowMin + GlimmerSHOccupancyGridSize, constants.boxMax.xyz) * GLIMMER_SH_OCCUPANCY_SUB_VOXELS - 1;

    const int3 voxelMin = max(int3(floor(min(p0, min(p1, p2)) * invSpacing)), boxMin);
    const int3 voxelMax = min(int3(floor(max(p0, max(p1, p2)) * invSpacing)), boxMax);

    if (any(voxelMax < voxelMin))
    {
        return;
    }

    const float4 value = float4(saturate(albedoAlpha.rgb), 1.0);
    const float3 halfExtent = (float3)(0.5 * spacing);

    const float3 absNormal = abs(normal);
    const uint axisW = select(absNormal.x >= absNormal.y, select(absNormal.x >= absNormal.z, 0u, 2u), select(absNormal.y >= absNormal.z, 1u, 2u));
    const uint axisU = (axisW + 1u) % 3u;
    const uint axisV = (axisW + 2u) % 3u;

    const float normalU = GetComponent(normal, axisU);
    const float normalV = GetComponent(normal, axisV);
    const float normalW = GetComponent(normal, axisW);

    const float planeU = GetComponent(p0, axisU);
    const float planeV = GetComponent(p0, axisV);
    const float planeW = GetComponent(p0, axisW);

    const float triangleMinW = GetComponent(min(p0, min(p1, p2)), axisW);
    const float triangleMaxW = GetComponent(max(p0, max(p1, p2)), axisW);

    const int minW = GetComponent(voxelMin, axisW);
    const int maxW = GetComponent(voxelMax, axisW);

    for (int v = GetComponent(voxelMin, axisV); v <= GetComponent(voxelMax, axisV); v++)
    {
        for (int u = GetComponent(voxelMin, axisU); u <= GetComponent(voxelMax, axisU); u++)
        {
            float columnMinW = 1e30;
            float columnMaxW = -1e30;

            [unroll]
            for (uint corner = 0; corner < 4; corner++)
            {
                const float cornerU = float(u + int(corner & 1u)) * spacing;
                const float cornerV = float(v + int(corner >> 1)) * spacing;
                const float cornerW = planeW - (normalU * (cornerU - planeU) + normalV * (cornerV - planeV)) / normalW;

                columnMinW = min(columnMinW, cornerW);
                columnMaxW = max(columnMaxW, cornerW);
            }

            const int firstW = max(int(floor(max(columnMinW, triangleMinW) * invSpacing)), minW);
            const int lastW = min(int(floor(min(columnMaxW, triangleMaxW) * invSpacing)), maxW);

            for (int w = firstW; w <= lastW; w++)
            {
                int coords[3];
                coords[axisU] = u;
                coords[axisV] = v;
                coords[axisW] = w;

                const int3 voxel = int3(coords[0], coords[1], coords[2]);
                const float3 center = (float3(voxel) + 0.5) * spacing;

                if (TriangleOverlapsBox(p0, p1, p2, normal, center, halfExtent))
                {
                    const int3 solidVoxel = voxel >> GLIMMER_SH_OCCUPANCY_SUB_SHIFT;
                    const uint bit = GlimmerSHOccupancySubBit(voxel & (GLIMMER_SH_OCCUPANCY_SUB_VOXELS - 1));

                    uint previous;
                    InterlockedOr(OutOccupancyMask[GlimmerSHOccupancyMaskIndex(cascadeIndex, solidVoxel) + (bit >> 5)], 1u << (bit & 31u), previous);

                    OutOccupancy[GlimmerSHOccupancyTexel(cascadeIndex, solidVoxel)] = value;
                }
            }
        }
    }
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint threadIndex = (groupId.y * constants.counts.z + groupId.x) * GROUP_SIZE + groupIndex;
    const uint cascadeIndex = uint(constants.origin.w);

#if defined(MODE_CLEAR)
    const uint3 boxExtent = uint3(constants.boxMax.xyz - constants.boxMin.xyz);

    if (threadIndex >= boxExtent.x * boxExtent.y * boxExtent.z)
    {
        return;
    }

    const int3 voxel = constants.boxMin.xyz + int3(
        threadIndex % boxExtent.x,
        (threadIndex / boxExtent.x) % boxExtent.y,
        threadIndex / (boxExtent.x * boxExtent.y));

    const uint maskIndex = GlimmerSHOccupancyMaskIndex(cascadeIndex, voxel);

    OutOccupancy[GlimmerSHOccupancyTexel(cascadeIndex, voxel)] = (float4)0.0;

    [unroll]
    for (uint wordIndex = 0; wordIndex < GLIMMER_SH_OCCUPANCY_MASK_WORDS; wordIndex++)
    {
        OutOccupancyMask[maskIndex + wordIndex] = 0u;
    }
#elif defined(MODE_SPLAT)
    const uint chunkIndex = groupId.y * constants.counts.z + groupId.x;

    if (chunkIndex >= constants.counts.y)
    {
        return;
    }

    const GlimmerSpanChunk chunk = spanChunks[chunkIndex];

    // the whole chunk goes when its instance misses the box
    const float3 windowMinPosition = float3(max(constants.origin.xyz, constants.boxMin.xyz)) * constants.params.x;
    const float3 windowMaxPosition = float3(min(constants.origin.xyz + GlimmerSHOccupancyGridSize, constants.boxMax.xyz)) * constants.params.x;

    if (any(chunk.boundsMax.xyz < windowMinPosition) || any(chunk.boundsMin.xyz > windowMaxPosition))
    {
        return;
    }

    const GlimmerSpanInstance instance = spanInstances[asuint(chunk.boundsMin.w)];

    // leaves are the canopy spans' business.
    // they dim light rather than stop it.
    if ((instance.data.w & GLIMMER_INSTANCE_FLAG_FOLIAGE) != 0u)
    {
        return;
    }

    for (uint chunkTriangle = groupIndex; chunkTriangle < GLIMMER_SPAN_CHUNK_TRIANGLES; chunkTriangle += GROUP_SIZE)
    {
        const uint localTriangle = asuint(chunk.boundsMax.w) + chunkTriangle;

        if (localTriangle >= instance.data.y)
        {
            break;
        }

        SplatTriangle(instance, localTriangle, cascadeIndex);
    }
#endif
}
