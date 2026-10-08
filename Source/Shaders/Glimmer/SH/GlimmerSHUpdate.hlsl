#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/EnvProbes.hlsli"

#include "../GlimmerCommon.hlsli"
#include "GlimmerSHCommon.hlsli"
#include "../GlimmerRelight.hlsli"
#include "../SWRT/GlimmerProbeTypes.hlsli"

#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "GlimmerSHOccupancy.hlsli"
#undef GLIMMER_SH_OCCUPANCY_NO_TRACE

struct GlimmerSHUpdateConstants
{
    GlimmerSHVolume volume;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    GlimmerSHOccupancyParams occupancy;
    int4 boxMin;        // xyz = absolute voxel of the dispatch's first voxel, w = cascade
    float4 rayRotation; // quaternion applied to this update's ray directions
    float4 params;      // x = foliage extinction, y = max ray distance, z = albedo where the ground's isn't known
    GlimmerRelightParams relight;
    GlimmerProbeVolume probes; // the near field, for what lights the hits; zeroed while it can't be sampled
    GlimmerSkyParams sky;
    EnvProbe skyProbe;         // textureIndices is ~0 without a sky probe
};

#include "../../Include/Clouds.hlsli"

DECLARE_BUFFER_DYNAMIC(GlimmerSHUpdate, CBuffer) cbuffer CBuffer
{
    GlimmerSHUpdateConstants constants;
    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;
};

#define skyProbe constants.skyProbe

DECLARE_SRV(GlimmerSHUpdate, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SAMPLER(GlimmerSHUpdate, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerSHUpdate, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerSHUpdate, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerSHUpdate, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerSHUpdate, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;
DECLARE_SRV(GlimmerSHUpdate, GlimmerRelightTexture) Texture2DArray<float4> glimmerRelight;

#define GLIMMER_RELIGHT_WITH_SAMPLING
#include "../GlimmerRelight.hlsli"
#undef GLIMMER_RELIGHT_WITH_SAMPLING

DECLARE_SRV(GlimmerSHUpdate, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(GlimmerSHUpdate, CloudWeatherMapTexture) Texture2DArray cloudWeatherMapTexture;
DECLARE_SRV(GlimmerSHUpdate, CloudShadowMapTexture) Texture2D cloudShadowMapTexture;

DECLARE_SRV(GlimmerSHUpdate, GlimmerProbeBlockTableBuffer) StructuredBuffer<uint> glimmerProbeBlockTable;
DECLARE_SRV(GlimmerSHUpdate, GlimmerProbeSHBuffer) StructuredBuffer<float4> glimmerProbeSH;
DECLARE_SRV(GlimmerSHUpdate, GlimmerProbeStatesBuffer) StructuredBuffer<uint4> glimmerProbeStates;
DECLARE_SRV(GlimmerSHUpdate, GlimmerProbeVisibilityBuffer) StructuredBuffer<uint> glimmerProbeVisibility;

#include "../SWRT/GlimmerProbes.hlsli"

DECLARE_UAV(GlimmerSHUpdate, OutData) RWTexture3D<float4> OutData;
DECLARE_UAV(GlimmerSHUpdate, OutState) RWTexture3D<uint4> OutState;
DECLARE_UAV(GlimmerSHUpdate, OutRadiance) RWTexture3D<float4> OutRadiance;

DECLARE_SRV(GlimmerSHUpdate, GlimmerSHOccupancyTexture) Texture3D<float4> glimmerSHOccupancy;
DECLARE_SRV(GlimmerSHUpdate, GlimmerSHOccupancyMaskBuffer) StructuredBuffer<uint> glimmerSHOccupancyMask;

// the hits see the far field as this update's own volume has it
#define GLIMMER_SH_LOAD_DATA(texel) OutData[texel]
#define GLIMMER_SH_LOAD_STATE(texel) OutState[texel]
#define GLIMMER_SH_LOAD_RADIANCE(texel) OutRadiance[texel]

#define GLIMMER_APPLY_WITH_SAMPLING
#define GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#include "GlimmerSHApply.hlsli"
#undef GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#undef GLIMMER_APPLY_WITH_SAMPLING

#define GLIMMER_LIGHTING_SKY constants.sky
#define GLIMMER_LIGHTING_PROBES constants.probes
#define GLIMMER_LIGHTING_SH constants.volume
#define GLIMMER_LIGHTING_GROUND constants.ground
#define GLIMMER_LIGHTING_RELIGHT constants.relight
#include "../GlimmerLighting.hlsli"

#include "../GlimmerHeightfield.hlsli"

#include "GlimmerSHOccupancy.hlsli"

#define GLIMMER_SH_HIT_OCCUPANCY 3u

#define GLIMMER_SH_GROUP_SIZE 64
#define GLIMMER_SH_RAYS 32

#define GLIMMER_SH_SUN_DISTANCE 256.0

#define GLIMMER_SH_HYSTERESIS 0.5

groupshared float4 gsVisibility[GLIMMER_SH_GROUP_SIZE]; // visible, visible * direction
groupshared float4 gsRadianceR[GLIMMER_SH_GROUP_SIZE];  // radiance, radiance * direction
groupshared float4 gsRadianceG[GLIMMER_SH_GROUP_SIZE];
groupshared float4 gsRadianceB[GLIMMER_SH_GROUP_SIZE];
groupshared float gsAxisDepths[GLIMMER_SH_AXIS_DEPTHS]; // in voxels

#define GLIMMER_SH_VOXEL_TRACED 0u
#define GLIMMER_SH_VOXEL_BURIED 1u
#define GLIMMER_SH_VOXEL_AIR 2u

groupshared uint gsVoxelKind;
groupshared float3 gsOriginOffset;
groupshared uint gsHasGround;
groupshared float gsGroundHeight;
groupshared uint gsGroundLevel;

bool GlimmerSHTraceScene(float3 origin, float3 direction, float tMax, uint startLevel, bool accumulateCanopy, out GlimmerHeightfieldHit hit, out float4 outLightmap)
{
    outLightmap = (float4)0.0;

    GlimmerSHOccupancyHit occupancyHit;
    float coveredT;

    const bool hitOccupancy = GlimmerSHTraceOccupancy(constants.occupancy, origin, direction, tMax, occupancyHit, coveredT);

    if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, hitOccupancy ? occupancyHit.t : tMax, startLevel, coveredT, constants.params.x, accumulateCanopy, hit))
    {
        return true;
    }

    if (hitOccupancy && hit.transmittance > 0.0)
    {
        hit.t = occupancyHit.t;
        hit.kind = GLIMMER_SH_HIT_OCCUPANCY;
        hit.normal = occupancyHit.normal;
        hit.albedo = occupancyHit.albedo;

        outLightmap = GlimmerSHLoadHitLightmap(occupancyHit, direction);

        return true;
    }

    return false;
}

float GlimmerSHSunVisibility(float3 P, float3 N, float3 L, uint level, bool isOccupancyHit)
{
    GlimmerHeightfieldHit shadowHit;
    float4 shadowHitLightmap;

    float spacing = 0.6; // past the occupancy, the heightfield alone

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        if (GlimmerSHOccupancyContains(constants.occupancy.cascades[cascadeIndex], P))
        {
            spacing = constants.occupancy.cascades[cascadeIndex].params.x;

            break;
        }
    }

    const float3 offset = isOccupancyHit ? N * 0.05 : N * 0.5 + L * 2.0;

    if (GlimmerSHTraceScene(P + offset * spacing, L, GLIMMER_SH_SUN_DISTANCE, level, false, shadowHit, shadowHitLightmap))
    {
        return 0.0;
    }

    return shadowHit.transmittance;
}

bool GlimmerSHIsBuried(float3 P)
{
    float groundHeight;
    uint groundLevel;

    if (GlimmerSampleGround(constants.ground, P.xz, 0u, groundHeight, groundLevel) && P.y < groundHeight + 0.1)
    {
        return true;
    }

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        if (GlimmerSHOccupancyContains(constants.occupancy.cascades[cascadeIndex], P))
        {
            return GlimmerSHOccupancyIsSolid(constants.occupancy, P);
        }
    }

    GlimmerSpanSample spanSample;

    return GlimmerSampleSpans(constants.spans, 0u, P.xz, spanSample)
        && GlimmerSpanSolidFillAt(spanSample, P.y, constants.spans.levels[0].params.x) >= GLIMMER_SPAN_SOLID_THRESHOLD;
}

bool GlimmerSHIsAir(GlimmerSHCascade cascade, int3 voxel)
{
    const float spacing = cascade.params.x;

    const float tileSize = float(GLIMMER_HEIGHT_BOUNDS_TILE_TEXELS) * constants.ground.levels[0].params.x;
    const uint level = uint(clamp(int(ceil(log2(max(3.0 * spacing / tileSize, 1.0)))), 0, GLIMMER_GROUND_LEVELS - 1));

    const float2 footprintMin = (float2(voxel.xz) - 1.0) * spacing + 1e-3;
    const float2 footprintMax = (float2(voxel.xz) + 2.0) * spacing - 1e-3;

    float top = -GLIMMER_HEIGHT_UNBOUNDED;

    [unroll]
    for (uint corner = 0; corner < 4; corner++)
    {
        const float2 xz = float2((corner & 1u) != 0u ? footprintMax.x : footprintMin.x, (corner & 2u) != 0u ? footprintMax.y : footprintMin.y);

        top = max(top, GlimmerHeightfieldTileTop(constants.ground, level, xz));
    }

    return float(voxel.y) * spacing > top + spacing;
}

// a voxel well above everything only sees the sky over it and the ground under it, so its rays skip the march and hit the ground
// as a plane at its height under the voxel
bool GlimmerSHTraceAirRay(float3 origin, float3 direction, out GlimmerHeightfieldHit hit)
{
    hit = (GlimmerHeightfieldHit)0;
    hit.kind = GLIMMER_HEIGHTFIELD_MISS;
    hit.transmittance = 1.0;

    if (gsHasGround == 0u || direction.y >= -1e-3)
    {
        return false;
    }

    const float t = (origin.y - gsGroundHeight) / -direction.y;

    if (t > constants.params.y)
    {
        return false;
    }

    const float2 xz = origin.xz + direction.xz * t;

    float groundHeight;
    uint groundLevel;

    if (!GlimmerSampleGround(constants.ground, xz, 0u, groundHeight, groundLevel))
    {
        groundLevel = gsGroundLevel;
    }

    hit.t = t;
    hit.kind = GLIMMER_HEIGHTFIELD_GROUND;
    hit.level = groundLevel;
    hit.normal = GlimmerGroundNormal(constants.ground, xz, groundLevel);

    return true;
}

float3 GlimmerSHShadeHit(float3 P, GlimmerHeightfieldHit hit, float3 albedo, float4 lightmap)
{
    float3 radiance;

    if (hit.kind != GLIMMER_SH_HIT_OCCUPANCY && GlimmerShadeRelitHit(P, albedo, hit.normal, hit.kind == GLIMMER_HEIGHTFIELD_GROUND, hit.level, radiance))
    {
        return radiance;
    }

    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);
    const float sunVisibility = GlimmerFacesSun(hit.normal) ? GlimmerSHSunVisibility(P, hit.normal, L, hit.level, hit.kind == GLIMMER_SH_HIT_OCCUPANCY) : 0.0;

    if (lightmap.a > 0.0)
    {
        return GlimmerShade(P, hit.normal, albedo, sunVisibility, lightmap.rgb);
    }

    return GlimmerShadeSurface(P, hit.normal, albedo, sunVisibility);
}

float3 GlimmerSHAxisDirection(uint axisIndex)
{
    const float sign = (axisIndex & 1u) != 0u ? -1.0 : 1.0;
    const uint axis = axisIndex >> 1;

    return float3(axis == 0u ? sign : 0.0, axis == 1u ? sign : 0.0, axis == 2u ? sign : 0.0);
}

// with N evenly spread rays, the cosine convolved L1 is e0 = mean, e1 = 2 * mean(value * direction)
float4 GlimmerSHProjectSum(float4 sum)
{
    const float invNumRays = 1.0 / float(GLIMMER_SH_RAYS);

    return float4(sum.x * invNumRays, sum.yzw * (2.0 * invNumRays));
}

[numthreads(GLIMMER_SH_GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint cascadeIndex = uint(constants.boxMin.w);
    const GlimmerSHCascade cascade = constants.volume.cascades[cascadeIndex];

    const float spacing = cascade.params.x;

    const int3 voxel = constants.boxMin.xyz + int3(groupId);
    const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);
    const float3 center = GlimmerSHVoxelCenter(cascade, voxel);

    if (groupIndex == 0u)
    {
        float3 originOffset = (float3)0.0;

        float3 freeSum = (float3)0.0;
        float freeCount = 0.0;
        float3 firstFree = (float3)0.0;

        [unroll]
        for (uint octant = 0; octant < 8; octant++)
        {
            const float3 octantOffset = float3(octant & 1u, (octant >> 1) & 1u, (octant >> 2) & 1u) * 0.5 - 0.25;

            if (!GlimmerSHIsBuried(center + octantOffset * spacing))
            {
                firstFree = freeCount == 0.0 ? octantOffset : firstFree;
                freeSum += octantOffset;
                freeCount += 1.0;
            }
        }

        const bool isBuried = freeCount == 0.0;

        if (freeCount > 0.0 && freeCount < 8.0)
        {
            originOffset = freeSum / freeCount;

            if (GlimmerSHIsBuried(center + originOffset * spacing))
            {
                originOffset = firstFree;
            }
        }

        uint voxelKind = GLIMMER_SH_VOXEL_TRACED;

        if (isBuried)
        {
            GlimmerSHVoxel data = (GlimmerSHVoxel)0;
            data.isBuried = true;

            OutData[texel] = (float4)0.0;
            OutRadiance[GlimmerSHRadianceTexel(texel, 0u)] = (float4)0.0;
            OutRadiance[GlimmerSHRadianceTexel(texel, 1u)] = (float4)0.0;
            OutRadiance[GlimmerSHRadianceTexel(texel, 2u)] = (float4)0.0;
            OutState[texel] = GlimmerSHPackVoxel(voxel, data);

            voxelKind = GLIMMER_SH_VOXEL_BURIED;
        }
        else if (GlimmerSHIsAir(cascade, voxel))
        {
            voxelKind = GLIMMER_SH_VOXEL_AIR;
        }

        float groundHeight;
        uint groundLevel;

        gsHasGround = GlimmerSampleGround(constants.ground, center.xz, 0u, groundHeight, groundLevel) ? 1u : 0u;
        gsGroundHeight = groundHeight;
        gsGroundLevel = groundLevel;

        gsVoxelKind = voxelKind;
        gsOriginOffset = originOffset;
    }

    GroupMemoryBarrierWithGroupSync();

    // a buried voxel traces nothing, but still runs to the last barrier: WGSL wants every thread to reach them
    const bool isBuried = gsVoxelKind == GLIMMER_SH_VOXEL_BURIED;
    const bool isAir = gsVoxelKind == GLIMMER_SH_VOXEL_AIR;

    const float3 originOffset = gsOriginOffset;
    const float3 origin = center + originOffset * spacing;

    const float finestTexel = constants.ground.levels[0].params.x;
    const uint startLevel = uint(clamp(int(round(log2(max(spacing / finestTexel, 1.0)))), 0, GLIMMER_GROUND_LEVELS - 1));

    float4 visibilitySample = (float4)0.0;
    float3 radianceSample = (float3)0.0;
    float3 rayDirection = (float3)0.0;

    if (isBuried)
    {
    }
    else if (groupIndex < GLIMMER_SH_RAYS)
    {
        rayDirection = normalize(GlimmerRotateByQuaternion(constants.rayRotation, GlimmerSphericalFibonacci(groupIndex, GLIMMER_SH_RAYS)));

        g_hasCanopySurroundings = false;

        GlimmerHeightfieldHit hit;
        float4 hitLightmap = (float4)0.0;
        bool didHit;

        if (isAir)
        {
            didHit = GlimmerSHTraceAirRay(origin, rayDirection, hit);
        }
        else
        {
            didHit = GlimmerSHTraceScene(origin, rayDirection, constants.params.y, startLevel, true, hit, hitLightmap);
        }

        float3 radiance;

        if (didHit)
        {
            const float3 P = origin + rayDirection * hit.t;

            const float3 albedo = hit.kind == GLIMMER_HEIGHTFIELD_GROUND
                ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, hit.level, (float3)constants.params.z)
                : hit.albedo;

            radiance = GlimmerSHShadeHit(P, hit, albedo, hitLightmap);
        }
        else
        {
            radiance = GlimmerSkyRadiance(rayDirection);
        }

        radianceSample = hit.inscatter + hit.transmittance * radiance;

        const float visible = didHit ? 0.0 : hit.transmittance;

        visibilitySample = float4(visible, visible * rayDirection);
    }
    else if (groupIndex < GLIMMER_SH_RAYS + GLIMMER_SH_AXIS_DEPTHS)
    {
        const uint axisIndex = groupIndex - GLIMMER_SH_RAYS;

        if (isAir)
        {
            // nothing is near but the ground under it
            gsAxisDepths[axisIndex] = axisIndex == 3u && gsHasGround != 0u
                ? clamp((origin.y - gsGroundHeight) / spacing, 0.0, GLIMMER_SH_DEPTH_RANGE)
                : GLIMMER_SH_DEPTH_RANGE;
        }
        else
        {
            // leaves don't count: light through a canopy isn't a leak
            GlimmerHeightfieldHit axisHit;
            float4 axisHitLightmap;
            const bool didHit = GlimmerSHTraceScene(origin, GlimmerSHAxisDirection(axisIndex), GLIMMER_SH_DEPTH_RANGE * spacing, startLevel, false, axisHit, axisHitLightmap);

            gsAxisDepths[axisIndex] = didHit ? min(axisHit.t / spacing, GLIMMER_SH_DEPTH_RANGE) : GLIMMER_SH_DEPTH_RANGE;
        }
    }

    gsVisibility[groupIndex] = visibilitySample;
    gsRadianceR[groupIndex] = float4(radianceSample.r, radianceSample.r * rayDirection);
    gsRadianceG[groupIndex] = float4(radianceSample.g, radianceSample.g * rayDirection);
    gsRadianceB[groupIndex] = float4(radianceSample.b, radianceSample.b * rayDirection);

    // every thread reads the history here, before thread 0 overwrites it at the end
    GlimmerSHVoxel previous;
    const bool hasHistory = GlimmerSHUnpackVoxel(OutState[texel], voxel, previous) && !previous.isBuried;
    const float4 previousVisibility = OutData[texel];
    const float4 previousR = OutRadiance[GlimmerSHRadianceTexel(texel, 0u)];
    const float4 previousG = OutRadiance[GlimmerSHRadianceTexel(texel, 1u)];
    const float4 previousB = OutRadiance[GlimmerSHRadianceTexel(texel, 2u)];

    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint stride = GLIMMER_SH_GROUP_SIZE / 2; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
        {
            gsVisibility[groupIndex] += gsVisibility[groupIndex + stride];
            gsRadianceR[groupIndex] += gsRadianceR[groupIndex + stride];
            gsRadianceG[groupIndex] += gsRadianceG[groupIndex + stride];
            gsRadianceB[groupIndex] += gsRadianceB[groupIndex + stride];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (isBuried || groupIndex != 0u)
    {
        return;
    }

    float4 visibility = GlimmerSHProjectSum(gsVisibility[0]);
    float4 radianceR = GlimmerSHProjectSum(gsRadianceR[0]);
    float4 radianceG = GlimmerSHProjectSum(gsRadianceG[0]);
    float4 radianceB = GlimmerSHProjectSum(gsRadianceB[0]);

    if (hasHistory)
    {
        visibility = lerp(visibility, previousVisibility, GLIMMER_SH_HYSTERESIS);
        radianceR = lerp(radianceR, previousR, GLIMMER_SH_HYSTERESIS);
        radianceG = lerp(radianceG, previousG, GLIMMER_SH_HYSTERESIS);
        radianceB = lerp(radianceB, previousB, GLIMMER_SH_HYSTERESIS);
    }

    GlimmerSHVoxel data = (GlimmerSHVoxel)0;
    data.originOffset = originOffset;
    data.isAir = isAir;

    [unroll]
    for (uint axisIndex = 0; axisIndex < GLIMMER_SH_AXIS_DEPTHS; axisIndex++)
    {
        data.depths[axisIndex] = gsAxisDepths[axisIndex];
    }

    OutData[texel] = visibility;
    OutRadiance[GlimmerSHRadianceTexel(texel, 0u)] = radianceR;
    OutRadiance[GlimmerSHRadianceTexel(texel, 1u)] = radianceG;
    OutRadiance[GlimmerSHRadianceTexel(texel, 2u)] = radianceB;
    OutState[texel] = GlimmerSHPackVoxel(voxel, data);
}
