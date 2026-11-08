#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

PERMUTE(MODE, UPDATE, COMPOSE)
PERMUTE(PROBES);

#define GLIMMER_SH_PROFILE 0

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/EnvProbes.hlsli"

#include "../GlimmerCommon.hlsli"
#include "GlimmerSHCommon.hlsli"
#include "../GlimmerRelight.hlsli"
#include "../GlimmerLocalLights.hlsli"
#include "../Probes/GlimmerProbeTypes.hlsli"

#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "GlimmerSHOccupancy.hlsli"
#undef GLIMMER_SH_OCCUPANCY_NO_TRACE

struct GlimmerSHUpdateConstants
{
    GlimmerSHVolume volume;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    GlimmerSHOccupancyParams occupancy;
    
    /// xyz = voxel pos of the dispatch's first voxel
    /// w = cascade
    int4 boxMin;
    
    /// quaternion applied to this update's ray directions
    float4 rayRotation;
    
    /// x = foliage extinction
    /// y = max ray distance
    /// z = albedo where the ground's isn't know
    /// w = how much of the radiance's history is kept
    float4 params;
    GlimmerRelightParams relight;
    GlimmerProbeVolume probes;
    GlimmerSkyParams sky;
    EnvProbe skyProbe;         // textureIndices is ~0 without a sky probe
    GlimmerLightParams light;
    
    /// x = 1 to light hits from the nearest voxel rather than the volume's filtered sample
    uint4 options;
    
    GlimmerLocalLights localLights;
};

#include "../../Include/Clouds.hlsli"

DECLARE_BUFFER_DYNAMIC(GlimmerSHUpdate, CBuffer) cbuffer CBuffer
{
    GlimmerSHUpdateConstants constants;
    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;
};

DECLARE_UAV(GlimmerSHUpdate, OutRadiance) RWTexture3D<float4> OutRadiance;
DECLARE_UAV(GlimmerSHUpdate, OutLight) RWStructuredBuffer<uint> OutLight;

void GlimmerSHWriteComposed(uint3 texel, float4 entries[GLIMMER_LIGHT_ENTRIES])
{
    float4 composed[3];
    GlimmerComposeStoredLight(constants.light, entries, composed);

    OutRadiance[GlimmerSHRadianceTexel(texel, 0u)] = composed[0];
    OutRadiance[GlimmerSHRadianceTexel(texel, 1u)] = composed[1];
    OutRadiance[GlimmerSHRadianceTexel(texel, 2u)] = composed[2];
}

void GlimmerSHWriteLight(uint3 texel, float4 entries[GLIMMER_LIGHT_ENTRIES])
{
    const uint seed = GlimmerProbeHash(GlimmerSHLightIndex(texel, 0u) ^ asuint(constants.rayRotation.x));

    for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
    {
        const uint bits = GlimmerProbeHash(seed + entry * 0x9E3779B9u);
        const float4 dither = float4(bits & 0xFFu, (bits >> 8) & 0xFFu, (bits >> 16) & 0xFFu, bits >> 24) * (1.0 / 256.0);

        const uint2 packed = GlimmerSHPackLight(entry, entries[entry], dither);
        const uint index = GlimmerSHLightIndex(texel, entry);

        OutLight[index] = packed.x;

        if (GlimmerSHIsWideLightEntry(entry))
        {
            OutLight[index + 1u] = packed.y;
        }
    }

    GlimmerSHWriteComposed(texel, entries);
}

#if defined(MODE_COMPOSE)

[numthreads(GLIMMER_SH_GRID_XZ * 2, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint3 texel = uint3(groupIndex % GLIMMER_SH_GRID_XZ, groupId.y * 2u + groupIndex / GLIMMER_SH_GRID_XZ, groupId.z);

    float4 light[GLIMMER_LIGHT_ENTRIES];

    for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
    {
        light[entry] = GLIMMER_SH_LOAD_LIGHT_FROM(OutLight, texel, entry);
    }

    GlimmerSHWriteComposed(texel, light);
}

#else

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

#if defined(PROBES)

DECLARE_SRV(GlimmerSHUpdate, GlimmerProbeBlockTableBuffer) StructuredBuffer<uint> glimmerProbeBlockTable;
DECLARE_SRV(GlimmerSHUpdate, GlimmerProbeLightBuffer) StructuredBuffer<float4> glimmerProbeLight;
DECLARE_SRV(GlimmerSHUpdate, GlimmerProbeStatesBuffer) StructuredBuffer<uint4> glimmerProbeStates;
DECLARE_SRV(GlimmerSHUpdate, GlimmerProbeVisibilityBuffer) StructuredBuffer<uint> glimmerProbeVisibility;

#define GLIMMER_PROBES_SPLIT
#include "../Probes/GlimmerProbes.hlsli"

#else

float SampleGlimmerProbesLight(GlimmerProbeVolume volume, float3 P, float3 N, out GlimmerLight outLight)
{
    outLight = GlimmerNoLight();

    return 0.0;
}

bool SampleGlimmerNearestProbeLight(GlimmerProbeVolume volume, float3 P, float3 N, out GlimmerLight outLight)
{
    outLight = GlimmerNoLight();

    return false;
}

#endif

DECLARE_UAV(GlimmerSHUpdate, OutData) RWTexture3D<float4> OutData;
DECLARE_UAV(GlimmerSHUpdate, OutState) RWTexture3D<uint4> OutState;

DECLARE_SRV(GlimmerSHUpdate, GlimmerSHOccupancyTexture) Texture3D<float4> glimmerSHOccupancy;
DECLARE_SRV(GlimmerSHUpdate, GlimmerSHOccupancyMaskBuffer) StructuredBuffer<uint> glimmerSHOccupancyMask;

#define GLIMMER_SH_LOAD_DATA(texel) OutData[texel]
#define GLIMMER_SH_LOAD_STATE(texel) OutState[texel]
#define GLIMMER_SH_LOAD_RADIANCE(texel) OutRadiance[texel]
#define GLIMMER_SH_LOAD_LIGHT(texel, entry) GLIMMER_SH_LOAD_LIGHT_FROM(OutLight, texel, entry)

#define GLIMMER_SH_SPLIT
#define GLIMMER_APPLY_WITH_SAMPLING
#define GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#include "GlimmerSHApply.hlsli"
#undef GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#undef GLIMMER_APPLY_WITH_SAMPLING

#define GLIMMER_LIGHTING_SKY constants.sky
#define GLIMMER_LIGHTING_PROBES constants.probes
#define GLIMMER_LIGHTING_SH constants.volume
#define GLIMMER_LIGHTING_GROUND constants.ground
#define GLIMMER_LIGHTING_SPANS constants.spans
#define GLIMMER_LIGHTING_RELIGHT constants.relight
#include "../GlimmerLighting.hlsli"

#include "../GlimmerHeightfield.hlsli"

#include "GlimmerSHOccupancy.hlsli"


#define GLIMMER_SH_HIT_OCCUPANCY 3u

#define GLIMMER_SH_GROUP_SIZE 64
#define GLIMMER_SH_RAYS 32

// every light entry, then the visibility
#define GLIMMER_SH_SEEDS (GLIMMER_LIGHT_ENTRIES + 1)

groupshared float4 gsVisibility[GLIMMER_SH_GROUP_SIZE]; // visible, visible * direction
groupshared float3 gsRayLight[GLIMMER_SH_RAYS * GLIMMER_LIGHT_PARTS];
groupshared float3 gsRayDirections[GLIMMER_SH_RAYS];
groupshared float gsRayEscapes[GLIMMER_SH_RAYS];
groupshared float gsRayBasis[GLIMMER_SH_RAYS * GLIMMER_LIGHT_TERMS]; // each ray's light basis, times how much of it got out
groupshared float4 gsSums[GLIMMER_LIGHT_PARTS * 3 + GLIMMER_LIGHT_TERMS];
groupshared uint gsSubVoxelFree[8];
groupshared float gsAxisDepths[GLIMMER_SH_AXIS_DEPTHS]; // in voxels

#define GLIMMER_SH_VOXEL_TRACED 0u
#define GLIMMER_SH_VOXEL_BURIED 1u
#define GLIMMER_SH_VOXEL_AIR 2u

groupshared uint gsVoxelKind;
groupshared float3 gsOriginOffset;
groupshared uint gsHasGround;
groupshared float gsGroundHeight;
groupshared uint gsGroundLevel;
groupshared float4 gsSeeds[GLIMMER_SH_SEEDS];
groupshared float gsSeedCoverage;

bool GlimmerSHTraceScene(float3 origin, float3 direction, float tMax, uint startLevel, bool accumulateCanopy, out GlimmerHeightfieldHit hit, out float4 outLightmap)
{
    outLightmap = (float4)0.0;

    GlimmerSHOccupancyHit occupancyHit;
    float coveredT;

#if GLIMMER_SH_PROFILE == 1 || GLIMMER_SH_PROFILE == 3
    occupancyHit = (GlimmerSHOccupancyHit)0;
    coveredT = 0.0;
    const bool hitOccupancy = false;
#else
    const bool hitOccupancy = GlimmerSHTraceOccupancy(constants.occupancy, origin, direction, tMax, occupancyHit, coveredT);
#endif

#if GLIMMER_SH_PROFILE == 2 || GLIMMER_SH_PROFILE == 3
    hit = (GlimmerHeightfieldHit)0;
    hit.transmittance = 1.0;
    hit.inscatter = GlimmerNoLight();
    if (false)
#else
    if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, hitOccupancy ? occupancyHit.t : tMax, startLevel, coveredT, constants.params.x, accumulateCanopy, hit))
#endif
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

bool GlimmerSHIsBuried(float3 P, uint minGroundLevel)
{
    float groundHeight;
    uint groundLevel;

    if (GlimmerSampleGround(constants.ground, P.xz, minGroundLevel, groundHeight, groundLevel) && P.y < groundHeight + 0.1)
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
    const uint level = max(uint(clamp(int(ceil(log2(max(3.0 * spacing / tileSize, 1.0)))), 0, GLIMMER_GROUND_LEVELS - 1)), uint(cascade.params.w));

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

/// a voxel well above all geometry has nothing to march through.
/// rays going up see sky, and rays going down hit a flat plane at the ground height below the voxel.
bool GlimmerSHTraceAirRay(float3 origin, float3 direction, uint minGroundLevel, out GlimmerHeightfieldHit hit)
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

    if (!GlimmerSampleGround(constants.ground, xz, minGroundLevel, groundHeight, groundLevel))
    {
        groundLevel = gsGroundLevel;
    }

    hit.t = t;
    hit.kind = GLIMMER_HEIGHTFIELD_GROUND;
    hit.level = groundLevel;
    hit.normal = GlimmerGroundNormal(constants.ground, xz, groundLevel);

    return true;
}

bool GlimmerSHLocalLightVisible(float3 origin, float3 direction, float tMax)
{
    GlimmerHeightfieldHit hit;
    float4 lightmap;

    return !GlimmerSHTraceScene(origin, direction, tMax, 0u, false, hit, lightmap);
}

#define GLIMMER_LOCAL_LIGHT_VISIBLE GlimmerSHLocalLightVisible
#include "../GlimmerLocalLights.hlsli"
#undef GLIMMER_LOCAL_LIGHT_VISIBLE

GlimmerLight GlimmerSHShadeHit(float3 P, GlimmerHeightfieldHit hit, float3 albedo, float4 lightmap)
{
    GlimmerLight light;

    if (hit.kind != GLIMMER_SH_HIT_OCCUPANCY && GlimmerShadeRelitHit(P, albedo, hit.normal, hit.kind == GLIMMER_HEIGHTFIELD_GROUND, hit.level, light))
    {
        return light;
    }

    if (lightmap.a > 0.0)
    {
        return GlimmerShade(albedo, GlimmerRestLight(lightmap.rgb));
    }

    GlimmerLight received;
    bool hasReceived = false;

    // GlimmerIndirect loads every light entry of up to 8 voxels per cascade.
    // read just the nearest probe or SH voxel half a voxel out - falls back if that voxel's origin is behind the surface
    if (constants.options.x != 0u)
    {
        hasReceived = SampleGlimmerNearestProbeLight(constants.probes, P + hit.normal * (0.5 * constants.volume.cascades[0].params.x), hit.normal, received)
            || GlimmerSHSampleNearestLight(constants.volume, P, hit.normal, 0.5, received);
    }

    if (!hasReceived)
    {
        received = GlimmerIndirect(P, hit.normal);
    }

    received.parts[GLIMMER_LIGHT_REST] += GlimmerLocalLightsAt(constants.localLights, P, hit.normal);

    return GlimmerShade(albedo, received);
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

float GlimmerSHSampleCoarse(uint cascadeIndex, float3 P, uint seedIndex, out float4 outValue)
{
    outValue = (float4) 0.0;

    float remaining = 1.0;

    [loop]
    for (uint coarserIndex = cascadeIndex + 1u; coarserIndex < GLIMMER_SH_CASCADES && remaining > 1e-3; coarserIndex++)
    {
        GlimmerSHTaps taps;
        const float cascadeWeight = GlimmerSHGatherTaps(constants.volume, coarserIndex, P, (float3)0.0, taps);

        if (cascadeWeight <= 0.0)
        {
            continue;
        }

        float4 value = (float4) 0.0;

        [unroll]
        for (uint tap = 0; tap < 8; tap++)
        {
            [branch]
            if (taps.weights[tap] > 0.0)
            {
                const float4 tapValue = seedIndex < GLIMMER_LIGHT_ENTRIES
                    ? GLIMMER_SH_LOAD_LIGHT(taps.texels[tap], seedIndex)
                    : GLIMMER_SH_LOAD_DATA(taps.texels[tap]);

                value += tapValue * taps.weights[tap];
            }
        }

        outValue += value * (cascadeWeight * remaining / taps.weightSum);
        remaining *= 1.0 - cascadeWeight;
    }

    const float coverage = 1.0 - remaining;

    if (coverage > 1e-4)
    {
        outValue /= coverage;
    }

    return coverage;
}

[numthreads(GLIMMER_SH_GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint cascadeIndex = uint(constants.boxMin.w);
    const GlimmerSHCascade cascade = constants.volume.cascades[cascadeIndex];

    const float spacing = cascade.params.x;
    const uint minGroundLevel = uint(cascade.params.w);

    const int3 voxel = constants.boxMin.xyz + int3(groupId);
    const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);
    const float3 center = GlimmerSHVoxelCenter(cascade, voxel);

    // the sub-voxels' burial tests, one a thread
    if (groupIndex < 8u)
    {
        const float3 subVoxelOffset = float3(
            groupIndex & 1u,
            (groupIndex >> 1) & 1u,
            (groupIndex >> 2) & 1u) * 0.5 - 0.25;

        gsSubVoxelFree[groupIndex] = GlimmerSHIsBuried(center + subVoxelOffset * spacing, minGroundLevel) ? 0u : 1u;
    }

    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0u)
    {
        float3 originOffset = (float3)0.0;

        float3 freeSum = (float3)0.0;
        float freeCount = 0.0;
        float3 firstFree = (float3)0.0;

        [loop]
        for (uint subVoxel = 0; subVoxel < 8; subVoxel++)
        {
            const float3 subVoxelOffset = float3(
                subVoxel & 1u,
                (subVoxel >> 1) & 1u,
                (subVoxel >> 2) & 1u) * 0.5 - 0.25;

            if (gsSubVoxelFree[subVoxel] != 0u)
            {
                firstFree = freeCount == 0.0 ? subVoxelOffset : firstFree;
                freeSum += subVoxelOffset;
                freeCount += 1.0;
            }
        }

        const bool isBuried = freeCount == 0.0;

        if (freeCount > 0.0 && freeCount < 8.0)
        {
            originOffset = freeSum / freeCount;

            if (GlimmerSHIsBuried(center + originOffset * spacing, minGroundLevel))
            {
                originOffset = firstFree;
            }
        }

        uint voxelKind = GLIMMER_SH_VOXEL_TRACED;

        if (isBuried)
        {
            GlimmerSHVoxel data = (GlimmerSHVoxel) 0;
            data.isBuried = true;

            OutData[texel] = (float4)0.0;
            
            float4 noLight[GLIMMER_LIGHT_ENTRIES] = (float4[GLIMMER_LIGHT_ENTRIES]) 0;

            GlimmerSHWriteLight(texel, noLight);
            OutState[texel] = GlimmerSHPackVoxel(voxel, data);

            voxelKind = GLIMMER_SH_VOXEL_BURIED;
        }
        else if (GlimmerSHIsAir(cascade, voxel))
        {
            voxelKind = GLIMMER_SH_VOXEL_AIR;
        }

        float groundHeight;
        uint groundLevel;

        gsHasGround = GlimmerSampleGround(constants.ground, center.xz, minGroundLevel, groundHeight, groundLevel) ? 1u : 0u;
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
    float3 rayDirection = (float3)0.0;

    const bool isRayThread = groupIndex < GLIMMER_SH_RAYS;
    const bool isAxisThread = !isRayThread && groupIndex < GLIMMER_SH_RAYS + GLIMMER_SH_AXIS_DEPTHS;
    const uint axisIndex = groupIndex - GLIMMER_SH_RAYS;

    GlimmerHeightfieldHit hit = (GlimmerHeightfieldHit)0;
    hit.kind = GLIMMER_HEIGHTFIELD_MISS;
    hit.transmittance = 1.0;

    float4 hitLightmap = (float4)0.0;
    bool didHit = false;

    if (!isBuried && (isRayThread || isAxisThread))
    {
        if (isRayThread)
        {
            rayDirection = normalize(GlimmerRotateByQuaternion(constants.rayRotation, GlimmerSphericalFibonacci(groupIndex, GLIMMER_SH_RAYS)));
        }

        if (isAir)
        {
            if (isRayThread)
            {
                didHit = GlimmerSHTraceAirRay(origin, rayDirection, minGroundLevel, hit);
            }
        }
        else
        {
            // leaves don't count for the axis depths: light through a canopy isn't a leak
            const float3 direction = select(isRayThread, rayDirection, GlimmerSHAxisDirection(axisIndex));
            const float tMax = select(isRayThread, constants.params.y, GLIMMER_SH_DEPTH_RANGE * spacing);

            didHit = GlimmerSHTraceScene(origin, direction, tMax, startLevel, isRayThread, hit, hitLightmap);
        }
    }

    if (isBuried)
    {
    }
    else if (isRayThread)
    {
        GlimmerLight light;

        if (didHit)
        {
            const float3 P = origin + rayDirection * hit.t;

            const float3 albedo = hit.kind == GLIMMER_HEIGHTFIELD_GROUND
                ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, hit.level, (float3)constants.params.z)
                : hit.albedo;

#if GLIMMER_SH_PROFILE == 4
            light = GlimmerNoLight();
#else
            light = GlimmerSHShadeHit(P, hit, albedo, hitLightmap);
#endif
        }
        else
        {
            light = GlimmerNoLight();
        }

        GlimmerLight rayLight = hit.inscatter;
        GlimmerAddLight(rayLight, light, (float3)hit.transmittance);

        [unroll]
        for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
        {
            gsRayLight[groupIndex * GLIMMER_LIGHT_PARTS + part] = rayLight.parts[part];
        }

        gsRayDirections[groupIndex] = rayDirection;

        const float visible = didHit ? 0.0 : hit.transmittance;

        gsRayEscapes[groupIndex] = visible;

        float basis[GLIMMER_LIGHT_TERMS];
        GlimmerLightBasis(rayDirection, basis);

        [unroll]
        for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
        {
            gsRayBasis[groupIndex * GLIMMER_LIGHT_TERMS + term] = basis[term] * visible;
        }

        visibilitySample = float4(visible, visible * rayDirection);
    }
    else if (isAxisThread)
    {
        if (isAir)
        {
            gsAxisDepths[axisIndex] = axisIndex == 3u && gsHasGround != 0u
                ? clamp((origin.y - gsGroundHeight) / spacing, 0.0, GLIMMER_SH_DEPTH_RANGE)
                : GLIMMER_SH_DEPTH_RANGE;
        }
        else
        {
            gsAxisDepths[axisIndex] = didHit ? min(hit.t / spacing, GLIMMER_SH_DEPTH_RANGE) : GLIMMER_SH_DEPTH_RANGE;
        }
    }

    gsVisibility[groupIndex] = visibilitySample;

    // every thread reads the history here, before thread 0 overwrites it at the end
    GlimmerSHVoxel previous;
    const bool hasHistory = GlimmerSHUnpackVoxel(OutState[texel], voxel, previous) && !previous.isBuried;
    const float4 previousVisibility = OutData[texel];

    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint stride = GLIMMER_SH_GROUP_SIZE / 2; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
        {
            gsVisibility[groupIndex] += gsVisibility[groupIndex + stride];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (!isBuried && groupIndex < uint(GLIMMER_LIGHT_PARTS * 3 + GLIMMER_LIGHT_TERMS))
    {
        float4 sum = (float4)0.0;

        if (groupIndex < uint(GLIMMER_LIGHT_PARTS * 3))
        {
            const uint part = groupIndex / 3u;
            const uint channel = groupIndex % 3u;

            for (uint rayIndex = 0; rayIndex < GLIMMER_SH_RAYS; rayIndex++)
            {
                const float value = gsRayLight[rayIndex * GLIMMER_LIGHT_PARTS + part][channel];

                sum += float4(value, value * gsRayDirections[rayIndex]);
            }
        }
        else
        {
            const uint term = groupIndex - uint(GLIMMER_LIGHT_PARTS * 3);

            for (uint rayIndex = 0; rayIndex < GLIMMER_SH_RAYS; rayIndex++)
            {
                sum += float4(1.0, gsRayDirections[rayIndex]) * gsRayBasis[rayIndex * GLIMMER_LIGHT_TERMS + term];
            }
        }

        gsSums[groupIndex] = sum;
    }

    const uint seedIndex = groupIndex - uint(GLIMMER_SH_GROUP_SIZE - GLIMMER_SH_SEEDS);

    if (!isBuried && !hasHistory && groupIndex >= uint(GLIMMER_SH_GROUP_SIZE - GLIMMER_SH_SEEDS))
    {
        float4 seed;
        const float coverage = GlimmerSHSampleCoarse(cascadeIndex, origin, seedIndex, seed);

        gsSeeds[seedIndex] = seed;

        if (seedIndex == 0u)
        {
            gsSeedCoverage = coverage;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (isBuried || groupIndex != 0u)
    {
        return;
    }

    float4 visibility = GlimmerSHProjectSum(gsVisibility[0]);
    float4 light[GLIMMER_LIGHT_ENTRIES] = (float4[GLIMMER_LIGHT_ENTRIES]) 0.0;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        GlimmerSetBounceEntry(light, part, 0u, GlimmerSHProjectSum(gsSums[part * 3u + 0u]));
        GlimmerSetBounceEntry(light, part, 1u, GlimmerSHProjectSum(gsSums[part * 3u + 1u]));
        GlimmerSetBounceEntry(light, part, 2u, GlimmerSHProjectSum(gsSums[part * 3u + 2u]));
    }
    
    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        light[GLIMMER_LIGHT_DIRECT_ENTRY + term] = GlimmerSHProjectSum(gsSums[GLIMMER_LIGHT_PARTS * 3 + term]);
    }
    if (hasHistory)
    {
        visibility = lerp(visibility, previousVisibility, constants.params.w);

        for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
        {
            light[entry] = lerp(light[entry], GLIMMER_SH_LOAD_LIGHT_FROM(OutLight, texel, entry), constants.params.w);
        }
    }
    else if (gsSeedCoverage > 0.0)
    {
        const float seedKept = constants.params.w * saturate(gsSeedCoverage);

        visibility = lerp(visibility, gsSeeds[GLIMMER_LIGHT_ENTRIES], seedKept);

        for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
        {
            light[entry] = lerp(light[entry], gsSeeds[entry], seedKept);
        }
    }

    GlimmerSHVoxel data = (GlimmerSHVoxel)0;
    data.originOffset = originOffset;
    data.isAir = isAir;

    [unroll]
    for (uint i = 0; i < GLIMMER_SH_AXIS_DEPTHS; i++)
    {
        data.depths[i] = gsAxisDepths[i];
    }

    OutData[texel] = visibility;
    GlimmerSHWriteLight(texel, light);
    OutState[texel] = GlimmerSHPackVoxel(voxel, data);
}

#endif
