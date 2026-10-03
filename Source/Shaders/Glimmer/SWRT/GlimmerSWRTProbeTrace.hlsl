#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

PERMUTE(MODE, TRACE, SHADE, RELIGHT)

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/RayTracing/BVH.hlsli"
#include "../../Include/EnvProbes.hlsli"
#include "GlimmerSWRTCommon.hlsli"
#include "GlimmerProbeTypes.hlsli"
#include "../SH/GlimmerSHCommon.hlsli"

#include "../GlimmerRelight.hlsli"

struct GlimmerProbeRayHit
{
    float4 positionT;           // xyz = shading position, w = hit distance for the ray record
    float4 normalFlags;         // xyz = normal, w = GLIMMER_RAY_HIT_* as uint
    float4 albedoTransmittance; // rgb = albedo, a = transmittance of the canopy in front of the hit
    float4 inscatter;           // rgb = light the canopy scattered toward the probe in front of the hit
};

//////////
#define GLIMMER_RAY_HIT_DONE 0u
#define GLIMMER_RAY_HIT_SHADE 1u
#define GLIMMER_RAY_HIT_SHADE_SWRT 2u
//////////

struct GlimmerProbeTraceConstants
{
    GlimmerProbeVolume volume;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    uint4 dispatch; // x = probes traced per frame at most
    float4 params;  // x = foliage extinction
    GlimmerSkyParams sky;
    GlimmerFootprintMaskParams mask;
    GlimmerSHVolume sh;  // the far field, sampled where a ray's hit is past the probes
    EnvProbe skyProbe;   // textureIndices is ~0 without a sky probe. used for ratios in relight pass.
    GlimmerRelightParams relight;
    int4 relightRect;    // relight pass: xy = absolute texel of the rect to light, zw = its extent
    uint4 relightInfo;   // relight pass: x = ground level, y = groups along x
};

#include "../../Include/Clouds.hlsli"

DECLARE_BUFFER_DYNAMIC(GlimmerProbeTrace, CBuffer) cbuffer CBuffer
{
    GlimmerProbeTraceConstants constants;
    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;
};

#define skyProbe constants.skyProbe

DECLARE_SRV(GlimmerProbeTrace, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(GlimmerProbeTrace, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerProbeTrace, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerProbeTrace, GlimmerTLASNodesBuffer) StructuredBuffer<BVHNode> glimmerTLASNodes;
DECLARE_SRV(GlimmerProbeTrace, GlimmerInstancesBuffer) StructuredBuffer<GlimmerInstance> glimmerInstances;
DECLARE_SRV(GlimmerProbeTrace, GlimmerBLASNodesBuffer) StructuredBuffer<BVHBLASNode> glimmerBLASNodes;
DECLARE_SRV(GlimmerProbeTrace, GlimmerBLASTrianglesBuffer) StructuredBuffer<uint> glimmerBLASTriangles;

DECLARE_SRV(GlimmerProbeTrace, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerProbeTrace, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerProbeTrace, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerProbeTrace, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;
DECLARE_SRV(GlimmerProbeTrace, FootprintMaskBuffer) StructuredBuffer<uint> footprintMask;

DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeBlockTableBuffer) StructuredBuffer<uint> glimmerProbeBlockTable;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeSHBuffer) StructuredBuffer<float4> glimmerProbeSH;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeStatesBuffer) StructuredBuffer<uint4> glimmerProbeStates;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeVisibilityBuffer) StructuredBuffer<uint> glimmerProbeVisibility;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeSlotsBuffer) StructuredBuffer<int4> glimmerProbeSlots;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeUpdateListBuffer) StructuredBuffer<uint> glimmerProbeUpdateList;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeCountersBuffer) StructuredBuffer<uint> glimmerProbeCounters;

DECLARE_SRV(GlimmerProbeTrace, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(GlimmerProbeTrace, CloudWeatherMapTexture) Texture2DArray cloudWeatherMapTexture;
DECLARE_SRV(GlimmerProbeTrace, CloudShadowMapTexture) Texture2D cloudShadowMapTexture;

DECLARE_SRV(GlimmerProbeTrace, GlimmerSHDataTexture) Texture3D<float4> glimmerSHData;
DECLARE_SRV(GlimmerProbeTrace, GlimmerSHStateTexture) Texture3D<uint4> glimmerSHState;
DECLARE_SRV(GlimmerProbeTrace, GlimmerSHRadianceTexture) Texture3D<float4> glimmerSHRadiance;

/// rgb = radiance, w = hit distance
/// w is negative for the back face of one sided geometry, 0 where the probe is underground
DECLARE_UAV(GlimmerProbeTrace, OutRays) RWStructuredBuffer<float4> OutRays;
DECLARE_UAV(GlimmerProbeTrace, RayHits) RWStructuredBuffer<GlimmerProbeRayHit> RayHits;

#if defined(MODE_RELIGHT)
DECLARE_UAV(GlimmerProbeTrace, OutRelight) RWTexture2DArray<float4> OutRelight;
#else
DECLARE_SRV(GlimmerProbeTrace, GlimmerRelightTexture) Texture2DArray<float4> glimmerRelight;

#define GLIMMER_RELIGHT_WITH_SAMPLING
#include "../GlimmerRelight.hlsli"
#undef GLIMMER_RELIGHT_WITH_SAMPLING
#endif

#include "GlimmerProbes.hlsli"
#include "GlimmerSWRT.hlsli"
#include "GlimmerSWRTFootprint.hlsli"
#include "../GlimmerMaterial.hlsli"

#define GLIMMER_APPLY_WITH_SAMPLING
#define GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#include "../SH/GlimmerSHApply.hlsli"
#undef GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#undef GLIMMER_APPLY_WITH_SAMPLING

#define GLIMMER_LIGHTING_SKY constants.sky
#define GLIMMER_LIGHTING_PROBES constants.volume
#define GLIMMER_LIGHTING_SH constants.sh
#define GLIMMER_LIGHTING_GROUND constants.ground
#define GLIMMER_LIGHTING_RELIGHT constants.relight
#include "../GlimmerLighting.hlsli"

#include "../GlimmerHeightfield.hlsli"

#define GLIMMER_SUN_SWRT_DISTANCE 256.0

#define GLIMMER_MASK_RAY_LEVEL_BIAS 1
#define GLIMMER_SUN_MASK_LEVEL 2

#define GLIMMER_SPAN_FOOTPRINT_SLACK 1.5

#if defined(MODE_SHADE) || defined(MODE_RELIGHT)

float GlimmerSunVisibility(float3 P, float3 N, float3 L, bool traceSWRT)
{
    const float3 origin = P + N * 0.05 + L * 0.02;

    float solidsFromT = 0.0;

    if (traceSWRT)
    {
        float tFirst;
        float tLast;
        float tCovered;

        const bool maskHit = GlimmerMaskTraceRay(constants.mask, GLIMMER_SUN_MASK_LEVEL, origin, L, GLIMMER_SUN_SWRT_DISTANCE, tFirst, tLast, tCovered);

        solidsFromT = min(tCovered, GLIMMER_SPAN_FOOTPRINT_SLACK * constants.spans.levels[0].params.x);

        if (maskHit)
        {
            GlimmerSWRTStats stats = GlimmerMakeSWRTStats();
            GlimmerSWRTHit shadowHit;

            if (TraceGlimmerSWRT(origin, L, tFirst, tLast, uint(constants.volume.nearField.z), true, shadowHit, stats))
            {
                return 0.0;
            }

            solidsFromT = tCovered;
        }
    }

    GlimmerHeightfieldHit shadowHit;

    if (GlimmerTraceHeightfield(
        constants.ground,
        constants.spans,
        origin,
        L,
        constants.volume.params.w,
        0u,
        solidsFromT,
        constants.params.x,
        false,
        shadowHit))
    {
        return 0.0;
    }

    return shadowHit.transmittance;
}

float3 GlimmerShadeTracedSurface(float3 P, float3 N, float3 albedo, bool traceSWRT)
{
    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);

    return GlimmerShadeSurface(P, N, albedo, GlimmerFacesSun(N) ? GlimmerSunVisibility(P, N, L, traceSWRT) : 0.0);
}

float4 GlimmerRelightSurface(float3 P, float3 N, bool traceSWRT)
{
    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);

    const float sunVisibility = L.y > -0.1 ? GlimmerSunVisibility(P, N, L, traceSWRT) : 0.0;

    return float4(min(GlimmerIndirect(P, N) / GlimmerSkyReference(), (float3)GLIMMER_RELIGHT_MAX_RATIO), sunVisibility);
}

#endif // MODE_SHADE || MODE_RELIGHT

/// 1 lane per ray, 2 probes per group
/// NOTE must match GlimmerProbeRays and ProbesPerTraceGroup in GlimmerSWRTProbeVolume
#define RAYS_PER_PROBE 32
#define PROBES_PER_GROUP 2

void GlimmerWriteRayHit(
    uint rayRecordIndex,
    float3 P,
    float3 N,
    float hitT,
    uint flags,
    float3 albedo,
    GlimmerHeightfieldHit heightfieldHit)
{
    GlimmerProbeRayHit rayHit;
    rayHit.positionT = float4(P, hitT);
    rayHit.normalFlags = float4(N, asfloat(flags));
    rayHit.albedoTransmittance = float4(albedo, heightfieldHit.transmittance);
    rayHit.inscatter = float4(heightfieldHit.inscatter, 0.0);

    RayHits[rayRecordIndex] = rayHit;
}

[numthreads(RAYS_PER_PROBE * PROBES_PER_GROUP, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
#if defined(MODE_RELIGHT)
    const uint texelIndex = (groupId.y * constants.relightInfo.y + groupId.x) * (RAYS_PER_PROBE * PROBES_PER_GROUP) + groupIndex;
    const uint2 extent = uint2(constants.relightRect.zw);

    if (texelIndex >= extent.x * extent.y)
    {
        return;
    }

    const uint level = constants.relightInfo.x;
    const int2 texel = constants.relightRect.xy + int2(texelIndex % extent.x, texelIndex / extent.x);
    const float2 xz = (float2(texel) + 0.5) * constants.ground.levels[level].params.x;
    const uint2 wrappedTexel = GlimmerWrapGroundTexel(texel);

    const bool traceSWRT = uint(constants.volume.nearField.z) != 0u;

    float4 groundRelight = float4(0.0, 0.0, 0.0, -1.0);
    float groundHeight;

    if (GlimmerSampleGroundLevel(constants.ground, level, xz, groundHeight))
    {
        groundRelight = GlimmerRelightSurface(float3(xz.x, groundHeight, xz.y), GlimmerGroundNormal(constants.ground, xz, level), traceSWRT);
    }

    OutRelight[uint3(wrappedTexel, GlimmerRelightSlice(level, GLIMMER_RELIGHT_GROUND))] = groundRelight;

    float4 spanTopRelight = float4(0.0, 0.0, 0.0, -1.0);
    GlimmerSpanSample spanSample;

    if (GlimmerSampleSpans(constants.spans, level, xz, spanSample) && spanSample.solidBins != 0u)
    {
        const float top = spanSample.solidMin + GlimmerSpanHeight(spanSample.solidMin, spanSample.solidMax);

        spanTopRelight = GlimmerRelightSurface(float3(xz.x, top, xz.y), float3(0.0, 1.0, 0.0), traceSWRT);
    }

    OutRelight[uint3(wrappedTexel, GlimmerRelightSlice(level, GLIMMER_RELIGHT_SPAN_TOP))] = spanTopRelight;
#else
    const uint listIndex = groupId.x * PROBES_PER_GROUP + groupIndex / RAYS_PER_PROBE;

    if (listIndex >= min(glimmerProbeCounters[0], constants.dispatch.x))
    {
        return;
    }

    const uint rayLane = groupIndex % RAYS_PER_PROBE;
    const uint numRays = constants.volume.info.y;

#if defined(MODE_SHADE)
    for (uint rayIndex = rayLane; rayIndex < numRays; rayIndex += RAYS_PER_PROBE)
    {
        const uint rayRecordIndex = listIndex * numRays + rayIndex;
        const GlimmerProbeRayHit rayHit = RayHits[rayRecordIndex];

        const uint flags = asuint(rayHit.normalFlags.w);

        if (flags == GLIMMER_RAY_HIT_DONE)
        {
            continue;
        }

        const float3 radiance = GlimmerShadeTracedSurface(
            rayHit.positionT.xyz,
            rayHit.normalFlags.xyz,
            rayHit.albedoTransmittance.rgb,
            flags == GLIMMER_RAY_HIT_SHADE_SWRT);

        OutRays[rayRecordIndex] = float4(rayHit.inscatter.rgb + rayHit.albedoTransmittance.a * radiance, rayHit.positionT.w);
    }
#else
    const uint probeIndex = glimmerProbeUpdateList[listIndex];
    const int4 slot = glimmerProbeSlots[probeIndex / GLIMMER_PROBES_PER_BLOCK];
    const uint levelIndex = uint(slot.w);

    const GlimmerProbeLevel level = constants.volume.levels[levelIndex];
    const float3 origin = GlimmerProbePosition(constants.volume, probeIndex, slot, glimmerProbeStates[probeIndex]);

    const uint numInstances = uint(constants.volume.nearField.z);

    // whether hits get sun shadows from SWRT (the shade pass asks the footprint mask before tracing)
    const bool swrtEnabled = numInstances != 0u && float(levelIndex) < constants.volume.nearField.x;
    const float swrtReach = constants.volume.nearField.y * level.params.x;

    const float maxDistance = constants.volume.params.w;

    // a probe with nothing in the footprint mask within reach has nothing for SWRT to hit
    const float swrtRange = min(swrtReach, maxDistance);

    const bool traceSWRT = swrtEnabled && GlimmerMaskAnyInBox(
        constants.mask,
        origin.xz - swrtRange,
        origin.xz + swrtRange,
        origin.y - swrtRange,
        origin.y + swrtRange);

    const uint maskLevel = levelIndex + GLIMMER_MASK_RAY_LEVEL_BIAS;

    // finer levels that don't cover a probe fall through to coarser ones in GlimmerSampleGround
    const uint startLevel = 0u;

    for (uint rayIndex = rayLane; rayIndex < numRays; rayIndex += RAYS_PER_PROBE)
    {
        const float3 direction = GlimmerProbeRayDirection(constants.volume, probeIndex, rayIndex);
        const uint rayRecordIndex = listIndex * numRays + rayIndex;

        float hitT = maxDistance;
        float3 radiance = (float3)0.0;
        bool isDone = true;
        bool isBackface = false;

        GlimmerSWRTHit hit;
        bool hitSWRT = false;

        float swrtFirst = 0.0;
        float swrtLast = 0.0;
        float swrtCovered = 0.0;

        // SWRT only traces the stretches of the ray the footprint mask marks occupied
        const bool traceRay = traceSWRT && GlimmerMaskTraceRay(
            constants.mask,
            maskLevel,
            origin,
            direction,
            swrtRange,
            swrtFirst,
            swrtLast,
            swrtCovered);

        if (traceRay)
        {
            GlimmerSWRTStats stats = GlimmerMakeSWRTStats();
            hitSWRT = TraceGlimmerSWRT(origin, direction, swrtFirst, swrtLast, numInstances, false, hit, stats);

            if (hitSWRT)
            {
                hitT = hit.t;
            }
        }

        const float solidsFromT = select(traceRay, swrtCovered, select(traceSWRT, min(swrtCovered, GLIMMER_SPAN_FOOTPRINT_SLACK * constants.spans.levels[0].params.x), 0.0));

        GlimmerHeightfieldHit heightfieldHit;

        g_hasCanopySurroundings = false;

        const bool hitHeightfield = GlimmerTraceHeightfield(
            constants.ground,
            constants.spans,
            origin,
            direction,
            hitT,
            startLevel,
            solidsFromT,
            constants.params.x,
            true,
            heightfieldHit);

        if (hitHeightfield)
        {
            hitT = heightfieldHit.t;

            const float3 P = origin + direction * hitT;

            const float3 albedo = heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND
                ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, heightfieldHit.level, (float3)constants.volume.nearField.w)
                : heightfieldHit.albedo;

            if (hitT > 0.0)
            {
                if (!GlimmerShadeRelitHit(P, albedo, heightfieldHit.normal, heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND, heightfieldHit.level, radiance))
                {
                    GlimmerWriteRayHit(
                        rayRecordIndex,
                        P + heightfieldHit.normal * 0.05,
                        heightfieldHit.normal,
                        hitT,
                        select(swrtEnabled, GLIMMER_RAY_HIT_SHADE_SWRT, GLIMMER_RAY_HIT_SHADE),
                        albedo,
                        heightfieldHit);

                    isDone = false;
                }
            }
        }
        else if (hitSWRT)
        {
            const GlimmerInstance instance = glimmerInstances[hit.instanceIndex];

            isBackface = !hit.frontFace && (instance.data.w & (GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED | GLIMMER_INSTANCE_FLAG_ALPHA_TESTED)) == 0u;

            if (isBackface)
            {
                radiance = (float3)0.0;
            }
            else
            {
                const float3 P = origin + direction * hitT;
                const float3 N = GlimmerGetHitNormal(hit, direction);

                // the shade pass adds its lighting to the inscatter, so the surface's own light rides in with it
                float3 emissive = GlimmerGetMaterialEmissive(instance.data.z);

                const float emissiveLuminance = dot(emissive, float3(0.2126, 0.7152, 0.0722));

                if (emissiveLuminance > constants.volume.params.z)
                {
                    emissive *= constants.volume.params.z / emissiveLuminance;
                }

                GlimmerHeightfieldHit emittingHit = heightfieldHit;
                emittingHit.inscatter += heightfieldHit.transmittance * emissive;

                GlimmerWriteRayHit(
                    rayRecordIndex,
                    P,
                    N,
                    hitT,
                    GLIMMER_RAY_HIT_SHADE_SWRT,
                    GlimmerGetMaterialAverageAlbedo(instance.data.z),
                    emittingHit);

                isDone = false;
            }
        }
        else
        {
            radiance = GlimmerSkyRadiance(direction);
        }

        if (isDone)
        {
            RayHits[rayRecordIndex].normalFlags.w = asfloat(GLIMMER_RAY_HIT_DONE);

            OutRays[rayRecordIndex] = float4(heightfieldHit.inscatter + heightfieldHit.transmittance * radiance, select(isBackface, -hitT, hitT));
        }
    }
#endif
#endif
}
