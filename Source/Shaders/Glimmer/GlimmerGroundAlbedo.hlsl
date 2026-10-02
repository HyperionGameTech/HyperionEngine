#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GlimmerCommon.hlsli"

struct GlimmerGroundAlbedoConstants
{
    GlimmerGroundParams ground;
    int4 windowOrigins; // xy = absolute texel of the rect being filled, zw = the window's origin at the level's last fill
    uint4 info;         // x = level, y = number of terrain patches, z = 1 when the last fill's origin is valid
    float4 groundCover; // per splat layer, how much of the ground its plants hide where the layer is full
    int4 fillMax;       // xy = absolute texel past the rect being filled (exclusive)
};

DECLARE_BUFFER_DYNAMIC(GlimmerGroundAlbedo, CBuffer) cbuffer CBuffer
{
    GlimmerGroundAlbedoConstants constants;
};

DECLARE_SRV(GlimmerGroundAlbedo, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerGroundAlbedo, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerGroundAlbedo, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerGroundAlbedo, TerrainPatchesBuffer) StructuredBuffer<GlimmerTerrainPatch> terrainPatches;
DECLARE_SRV(GlimmerGroundAlbedo, GroundCoverAlbedoBuffer) StructuredBuffer<float4> groundCoverAlbedo; // rgb, a = 1 once known

DECLARE_UAV(GlimmerGroundAlbedo, OutGroundAlbedo) RWTexture2DArray<float4> OutGroundAlbedo;

#define texture_sampler glimmerMaterialSampler
#include "../Include/RayTracing/TerrainSurface.hlsli"

bool GlimmerLoadGroundHeight(GlimmerGroundLevel groundLevel, uint level, int2 texel, out float height)
{
    height = GLIMMER_NO_GROUND_HEIGHT;

    if (any(texel < groundLevel.validRect.xy) || any(texel >= groundLevel.validRect.zw))
    {
        return false;
    }

    height = glimmerGround.Load(int4(GlimmerWrapGroundTexel(texel), level, 0));

    return height > GLIMMER_NO_GROUND_HEIGHT + 1.0;
}

float GlimmerLoadNeighbourHeight(GlimmerGroundLevel groundLevel, uint level, int2 texel, float centerHeight)
{
    float height;

    return GlimmerLoadGroundHeight(groundLevel, level, texel, height) ? height : centerHeight;
}

float3 GlimmerApplyGroundCover(float3 terrainAlbedo, float4 splatWeights)
{
    float4 cover = smoothstep(0.3, 0.8, splatWeights) * constants.groundCover;

    float3 coverAlbedo = (float3)0.0;

    [unroll]
    for (uint layerIndex = 0; layerIndex < GLIMMER_GROUND_COVER_LAYERS; layerIndex++)
    {
        const float4 layerAlbedo = groundCoverAlbedo[layerIndex];

        cover[layerIndex] *= layerAlbedo.a;
        coverAlbedo += layerAlbedo.rgb * cover[layerIndex];
    }

    const float coverSum = cover.x + cover.y + cover.z + cover.w;

    if (coverSum <= 1e-4)
    {
        return terrainAlbedo;
    }

    return lerp(terrainAlbedo, coverAlbedo / coverSum, saturate(coverSum));
}

float3 GlimmerSamplePatchAlbedo(GlimmerTerrainPatch patch, float3 P, float3 N)
{
    const Material material = materials[patch.data.x];

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    const float4 P1 = float4(P, 1.0);
    const float2 objectXZ = float2(dot(patch.worldToObject0, P1), dot(patch.worldToObject2, P1));

    float2 splatSize = float2(1.0, 1.0);

    if (HAS_TEXTURE(material, TerrainSplatMap))
    {
        GET_TEXTURE(material, TerrainSplatMap).GetDimensions(splatSize.x, splatSize.y);
    }

    float2 splatTexcoord = saturate((objectXZ + 0.5) / max(splatSize, 1.0));
    splatTexcoord.y = 1.0 - splatTexcoord.y;

    return GlimmerApplyGroundCover(SampleTerrainAlbedo(material, P, N, splatTexcoord), SampleTerrainSplatWeights(material, N, splatTexcoord));
#else
    return material.albedo.rgb;
#endif
}

#define GLIMMER_ALBEDO_MAX_SUBSAMPLES 4

#define GROUP_SIZE 8
#define MAX_GROUP_PATCHES 64

groupshared uint groupPatchIndices[MAX_GROUP_PATCHES];
groupshared uint groupNumPatches;

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint level = constants.info.x;

    const GlimmerGroundLevel groundLevel = constants.ground.levels[level];
    const float texelSize = groundLevel.params.x;

    const float2 groupMin = float2(constants.windowOrigins.xy + int2(groupId.xy * GROUP_SIZE)) * texelSize;
    const float2 groupMax = groupMin + float(GROUP_SIZE) * texelSize;

    if (groupIndex == 0u)
    {
        groupNumPatches = 0u;
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint candidateIndex = groupIndex; candidateIndex < constants.info.y; candidateIndex += GROUP_SIZE * GROUP_SIZE)
    {
        const float4 bounds = terrainPatches[candidateIndex].boundsXZ;

        if (all(bounds.xy <= groupMax) && all(bounds.zw >= groupMin))
        {
            uint listIndex;
            InterlockedAdd(groupNumPatches, 1u, listIndex);

            if (listIndex < MAX_GROUP_PATCHES)
            {
                groupPatchIndices[listIndex] = candidateIndex;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    const uint numPatches = min(groupNumPatches, uint(MAX_GROUP_PATCHES));

    const int2 texel = constants.windowOrigins.xy + int2(dispatchThreadId.xy);

    if (any(texel >= constants.fillMax.xy))
    {
        return;
    }
    const uint3 slot = uint3(GlimmerWrapGroundTexel(texel), level);

    const bool isNew = constants.info.z == 0u
        || any(texel < constants.windowOrigins.zw)
        || any(texel >= constants.windowOrigins.zw + GLIMMER_GROUND_RESOLUTION);

    float height;

    if (!GlimmerLoadGroundHeight(groundLevel, level, texel, height))
    {
        if (isNew)
        {
            OutGroundAlbedo[slot] = (float4)0.0;
        }

        return;
    }

    const float hL = GlimmerLoadNeighbourHeight(groundLevel, level, texel - int2(1, 0), height);
    const float hR = GlimmerLoadNeighbourHeight(groundLevel, level, texel + int2(1, 0), height);
    const float hD = GlimmerLoadNeighbourHeight(groundLevel, level, texel - int2(0, 1), height);
    const float hU = GlimmerLoadNeighbourHeight(groundLevel, level, texel + int2(0, 1), height);

    const float3 N = normalize(float3(hL - hR, 2.0 * texelSize, hD - hU));
    const float3 center = float3((float2(texel) + 0.5) * texelSize, height).xzy;

    // coarse texels span many splat texels, so they average a few points like their heights do
    const uint numSubsamples = level != 0u ? GLIMMER_ALBEDO_MAX_SUBSAMPLES : 1u;

    const float2 subsampleOffsets[GLIMMER_ALBEDO_MAX_SUBSAMPLES] = {
        float2(-0.25, -0.25), float2(0.25, -0.25), float2(-0.25, 0.25), float2(0.25, 0.25)
    };

    float3 albedoSum = (float3)0.0;
    uint coveredMask = 0u;

    [loop]
    for (uint listIndex = 0; listIndex < numPatches; listIndex++)
    {
        const GlimmerTerrainPatch patch = terrainPatches[groupPatchIndices[listIndex]];

        for (uint subsampleIndex = 0; subsampleIndex < numSubsamples; subsampleIndex++)
        {
            if ((coveredMask & (1u << subsampleIndex)) != 0u)
            {
                continue;
            }

            const float3 P = numSubsamples == 1u ? center : center + float3(subsampleOffsets[subsampleIndex].x, 0.0, subsampleOffsets[subsampleIndex].y) * texelSize;

            if (any(P.xz < patch.boundsXZ.xy) || any(P.xz > patch.boundsXZ.zw))
            {
                continue;
            }

            albedoSum += GlimmerSamplePatchAlbedo(patch, P, N);
            coveredMask |= 1u << subsampleIndex;
        }
    }

    if (coveredMask != 0u)
    {
        OutGroundAlbedo[slot] = float4(albedoSum / float(countbits(coveredMask)), 1.0);
    }
    else if (isNew)
    {
        OutGroundAlbedo[slot] = (float4)0.0;
    }
}
