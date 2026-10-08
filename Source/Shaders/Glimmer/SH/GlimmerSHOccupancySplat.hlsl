#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Aabb.hlsli"
#include "../../Include/Lightmap.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/RayTracing/BVH.hlsli"
#include "../GlimmerCommon.hlsli"

#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "GlimmerSHOccupancy.hlsli"

PERMUTE(MODE, CLEAR, SPLAT, RESOLVE)

struct GlimmerSHOccupancySplatConstants
{
    int4 origin;   // xyz = absolute voxel of the cascade's window, w = cascade
    
    float4 params; // x = voxel spacing, y = 1 / spacing
    
    uint4 counts;  // x = span instances, y = span chunks, z = groups along x
    
    int4 boxMin;   // xyz = absolute voxel; only voxels in the box are cleared and splatted
    int4 boxMax;   // xyz = absolute voxel, exclusive

    uint4 lightmapStencils;
};

DECLARE_BUFFER_DYNAMIC(GlimmerSHOccupancySplat, CBuffer) cbuffer CBuffer
{
    GlimmerSHOccupancySplatConstants constants;
};

DECLARE_UAV(GlimmerSHOccupancySplat, OutOccupancy) RWTexture3D<float4> OutOccupancy;
DECLARE_UAV(GlimmerSHOccupancySplat, OutOccupancyMask) RWStructuredBuffer<uint> OutOccupancyMask;
DECLARE_UAV(GlimmerSHOccupancySplat, OutAlbedoSums) RWStructuredBuffer<uint> OutAlbedoSums;
DECLARE_SRV(GlimmerSHOccupancySplat, SpanInstancesBuffer) StructuredBuffer<GlimmerSpanInstance> spanInstances;
DECLARE_SRV(GlimmerSHOccupancySplat, SpanChunksBuffer) StructuredBuffer<GlimmerSpanChunk> spanChunks;
DECLARE_SRV(GlimmerSHOccupancySplat, GlimmerBLASTrianglesBuffer) StructuredBuffer<uint> glimmerBLASTriangles;
DECLARE_SRV(GlimmerSHOccupancySplat, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerSHOccupancySplat, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerSHOccupancySplat, GlimmerBLASLightmapUVsBuffer) StructuredBuffer<uint> glimmerBLASLightmapUVs;

DECLARE_SRV(GlimmerSHOccupancySplat, LightmapVolumeIrradianceTexture0) Texture2D LightmapVolumeIrradianceTexture0;
DECLARE_SRV(GlimmerSHOccupancySplat, LightmapVolumeIrradianceTexture1) Texture2D LightmapVolumeIrradianceTexture1;
DECLARE_SRV(GlimmerSHOccupancySplat, LightmapVolumeIrradianceTexture2) Texture2D LightmapVolumeIrradianceTexture2;
DECLARE_SRV(GlimmerSHOccupancySplat, LightmapVolumeIrradianceTexture3) Texture2D LightmapVolumeIrradianceTexture3;

#include "../GlimmerMaterial.hlsli"

#define GROUP_SIZE 64

#define ALBEDO_SUM_WORDS 4u
#define ALBEDO_WEIGHT_SCALE 1024.0

// per face : weighted irradiance (3 values), weight
#define LIGHTMAP_SUM_WORDS (GLIMMER_SH_OCCUPANCY_LIGHTMAP_FACES * 4u)
#define VOXEL_SUM_WORDS (ALBEDO_SUM_WORDS + LIGHTMAP_SUM_WORDS)
#define LIGHTMAP_MAX_IRRADIANCE 32.0
#define LIGHTMAP_IRRADIANCE_SCALE 16.0

uint AlbedoSumIndex(int3 voxel)
{
    const uint3 texel = GlimmerSHOccupancyTexel(0u, voxel);

    return ((texel.z * uint(GLIMMER_SH_OCCUPANCY_GRID_Y) + texel.y) * uint(GLIMMER_SH_OCCUPANCY_GRID_XZ) + texel.x) * VOXEL_SUM_WORDS;
}

uint LightmapSumIndex(int3 voxel)
{
    return AlbedoSumIndex(voxel) + ALBEDO_SUM_WORDS;
}

float2 LoadLightmapUV(uint uvBase, uint corner)
{
    const uint packed = glimmerBLASLightmapUVs[uvBase + corner];

    return float2(packed & 0xFFFFu, packed >> 16u) / 65535.0;
}

float3 SampleLightmapPage(uint page, float2 atlasUV)
{
    float3 irradiance;

    if (page == 0u)
    {
        irradiance = LightmapVolumeIrradianceTexture0.SampleLevel(glimmerMaterialSampler, atlasUV, 0.0).rgb;
    }
    else if (page == 1u)
    {
        irradiance = LightmapVolumeIrradianceTexture1.SampleLevel(glimmerMaterialSampler, atlasUV, 0.0).rgb;
    }
    else if (page == 2u)
    {
        irradiance = LightmapVolumeIrradianceTexture2.SampleLevel(glimmerMaterialSampler, atlasUV, 0.0).rgb;
    }
    else
    {
        irradiance = LightmapVolumeIrradianceTexture3.SampleLevel(glimmerMaterialSampler, atlasUV, 0.0).rgb;
    }

    return min(max(irradiance, (float3)0.0), (float3)LIGHTMAP_MAX_IRRADIANCE);
}

#define BACK_BIAS 0.01

float3 TransformPoint(GlimmerSpanInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.objectToWorld0, homogeneous), dot(instance.objectToWorld1, homogeneous), dot(instance.objectToWorld2, homogeneous));
}

struct TriangleLightmap
{
    bool isValid;
    
    uint page;
    
    uint rectOffset;
    uint rectSize;
    
    float2 uv0;
    
    float2 uvEdge1;
    float2 uvEdge2;

    float3 p0;
    float3 edge1;
    float3 edge2;
};

TriangleLightmap LoadTriangleLightmap(GlimmerSpanInstance instance, uint localTriangle, uint cascadeIndex, float3 p0, float3 p1, float3 p2)
{
    TriangleLightmap lightmap = (TriangleLightmap)0;
    lightmap.rectOffset = instance.lightmap.y;
    lightmap.rectSize = instance.lightmap.z;
    lightmap.p0 = p0;
    lightmap.edge1 = p1 - p0;
    lightmap.edge2 = p2 - p0;

    if (cascadeIndex >= uint(GLIMMER_SH_OCCUPANCY_TRACED_CASCADES) || instance.lightmap.x == 0xFFFFFFFFu)
    {
        return lightmap;
    }

    const uint stencilValue = GetLightmapStencilValue(lightmap.rectOffset);

    [unroll]
    for (uint page = 0; page < 4; page++)
    {
        if (!lightmap.isValid && stencilValue != 0u && constants.lightmapStencils[page] == stencilValue)
        {
            lightmap.isValid = true;
            lightmap.page = page;
        }
    }

    if (lightmap.isValid)
    {
        const uint uvBase = (instance.lightmap.x + localTriangle) * 3u;

        lightmap.uv0 = LoadLightmapUV(uvBase, 0u);
        lightmap.uvEdge1 = LoadLightmapUV(uvBase, 1u) - lightmap.uv0;
        lightmap.uvEdge2 = LoadLightmapUV(uvBase, 2u) - lightmap.uv0;
    }

    return lightmap;
}

// the baked irradiance at the point of the triangle nearest position
float3 SampleTriangleLightmap(TriangleLightmap lightmap, float3 position)
{
    const float edge11 = dot(lightmap.edge1, lightmap.edge1);
    const float edge12 = dot(lightmap.edge1, lightmap.edge2);
    const float edge22 = dot(lightmap.edge2, lightmap.edge2);

    const float3 toPosition = position - lightmap.p0;
    const float toPosition1 = dot(toPosition, lightmap.edge1);
    const float toPosition2 = dot(toPosition, lightmap.edge2);

    float2 barycentrics = float2(edge22 * toPosition1 - edge12 * toPosition2, edge11 * toPosition2 - edge12 * toPosition1) / max(edge11 * edge22 - edge12 * edge12, 1e-20);
    barycentrics = max(barycentrics, (float2)0.0);
    barycentrics /= max(barycentrics.x + barycentrics.y, 1.0);

    const float2 uv = lightmap.uv0 + lightmap.uvEdge1 * barycentrics.x + lightmap.uvEdge2 * barycentrics.y;

    return SampleLightmapPage(lightmap.page, GetLightmapAtlasUV(lightmap.rectOffset, lightmap.rectSize, uv));
}

void SplatLightmapIrradiance(int3 solidVoxel, float3 irradiance, float coveredArea, float3 facingNormal, bool isDoubleSided)
{
    // texels nothing was baked into are black
    if (!any(irradiance > 0.0))
    {
        return;
    }

    const uint lightmapSumIndex = LightmapSumIndex(solidVoxel);

    [unroll]
    for (uint axis = 0; axis < 3; axis++)
    {
        const float facing = GetComponent(facingNormal, axis);
        const uint faceWeight = uint(coveredArea * facing * facing * ALBEDO_WEIGHT_SCALE + 0.5);
        const uint3 weightedIrradiance = uint3(irradiance * (float(faceWeight) * LIGHTMAP_IRRADIANCE_SCALE) + 0.5);

        [unroll]
        for (uint side = 0; side < 2; side++)
        {
            if (faceWeight != 0u && (isDoubleSided || (facing < 0.0) == (side == 1u)))
            {
                const uint faceIndex = lightmapSumIndex + (axis * 2u + side) * 4u;

                uint previous;
                InterlockedAdd(OutAlbedoSums[faceIndex + 0u], weightedIrradiance.r, previous);
                InterlockedAdd(OutAlbedoSums[faceIndex + 1u], weightedIrradiance.g, previous);
                InterlockedAdd(OutAlbedoSums[faceIndex + 2u], weightedIrradiance.b, previous);
                InterlockedAdd(OutAlbedoSums[faceIndex + 3u], faceWeight, previous);
            }
        }
    }
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

    const bool isDoubleSided = (instance.data.w & GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED) != 0u;
    const float3 facingNormal = normalize(normal) * ((instance.data.w & GLIMMER_INSTANCE_FLAG_MIRRORED) != 0u ? -1.0 : 1.0);

    const TriangleLightmap lightmap = LoadTriangleLightmap(instance, localTriangle, cascadeIndex, p0, p1, p2);

    if (!isDoubleSided)
    {
        const float3 behind = facingNormal * (-BACK_BIAS * spacing);

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

    const float coveredArea = min(0.5 * length(normal) * invSpacing * invSpacing, 1.0);
    const uint weight = max(uint(coveredArea * ALBEDO_WEIGHT_SCALE + 0.5), 1u);
    const uint3 weightedAlbedo = uint3(saturate(albedoAlpha.rgb) * float(weight) + 0.5);
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

                    const uint sumIndex = AlbedoSumIndex(solidVoxel);

                    InterlockedAdd(OutAlbedoSums[sumIndex + 0u], weightedAlbedo.r, previous);
                    InterlockedAdd(OutAlbedoSums[sumIndex + 1u], weightedAlbedo.g, previous);
                    InterlockedAdd(OutAlbedoSums[sumIndex + 2u], weightedAlbedo.b, previous);
                    InterlockedAdd(OutAlbedoSums[sumIndex + 3u], weight, previous);

                    if (lightmap.isValid)
                    {
                        SplatLightmapIrradiance(solidVoxel, SampleTriangleLightmap(lightmap, center), coveredArea, facingNormal, isDoubleSided);
                    }
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

#if defined(MODE_CLEAR) || defined(MODE_RESOLVE)
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
    const uint sumIndex = AlbedoSumIndex(voxel);

#if defined(MODE_RESOLVE)
    if ((OutOccupancyMask[maskIndex] | OutOccupancyMask[maskIndex + 1u]) != 0u)
    {
        const float3 albedoSum = float3(OutAlbedoSums[sumIndex + 0u], OutAlbedoSums[sumIndex + 1u], OutAlbedoSums[sumIndex + 2u]);

        OutOccupancy[GlimmerSHOccupancyTexel(cascadeIndex, voxel)] = float4(albedoSum / float(max(OutAlbedoSums[sumIndex + 3u], 1u)), 1.0);

        if (cascadeIndex < uint(GLIMMER_SH_OCCUPANCY_TRACED_CASCADES))
        {
            const uint lightmapSumIndex = LightmapSumIndex(voxel);
            const uint lightmapIndex = GlimmerSHOccupancyLightmapIndex(cascadeIndex, voxel);

            [unroll]
            for (uint face = 0; face < GLIMMER_SH_OCCUPANCY_LIGHTMAP_FACES; face++)
            {
                const uint faceIndex = lightmapSumIndex + face * 4u;
                const uint faceWeight = OutAlbedoSums[faceIndex + 3u];

                if (faceWeight != 0u)
                {
                    const float3 irradianceSum = float3(OutAlbedoSums[faceIndex + 0u], OutAlbedoSums[faceIndex + 1u], OutAlbedoSums[faceIndex + 2u]);

                    OutOccupancyMask[lightmapIndex + face] = PackRGB9E5(irradianceSum / (float(faceWeight) * LIGHTMAP_IRRADIANCE_SCALE));
                }
            }
        }
    }
#else
    OutOccupancy[GlimmerSHOccupancyTexel(cascadeIndex, voxel)] = (float4)0.0;

    [unroll]
    for (uint sumWord = 0; sumWord < ALBEDO_SUM_WORDS; sumWord++)
    {
        OutAlbedoSums[sumIndex + sumWord] = 0u;
    }

    [unroll]
    for (uint wordIndex = 0; wordIndex < GLIMMER_SH_OCCUPANCY_MASK_WORDS; wordIndex++)
    {
        OutOccupancyMask[maskIndex + wordIndex] = 0u;
    }

    const uint lightmapSumIndex = LightmapSumIndex(voxel);

    [unroll]
    for (uint lightmapSumWord = 0; lightmapSumWord < LIGHTMAP_SUM_WORDS; lightmapSumWord++)
    {
        OutAlbedoSums[lightmapSumIndex + lightmapSumWord] = 0u;
    }

    if (cascadeIndex < uint(GLIMMER_SH_OCCUPANCY_TRACED_CASCADES))
    {
        const uint lightmapIndex = GlimmerSHOccupancyLightmapIndex(cascadeIndex, voxel);

        [unroll]
        for (uint face = 0; face < GLIMMER_SH_OCCUPANCY_LIGHTMAP_FACES; face++)
        {
            OutOccupancyMask[lightmapIndex + face] = 0u;
        }
    }
#endif
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
