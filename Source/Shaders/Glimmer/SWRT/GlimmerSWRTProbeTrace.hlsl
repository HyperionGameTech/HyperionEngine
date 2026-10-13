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
    uint inscatter[GLIMMER_LIGHT_PACKED_UINTS_PADDED]; // light the canopy scattered toward the probe in front of the hit
};

//////////
#define GLIMMER_RAY_HIT_DONE 0u
#define GLIMMER_RAY_HIT_SHADE 1u
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

#ifdef BACKEND_WEBGPU
// WebGPU allows 16 storage buffers in a stage and the trace pass reads 18,
// so the two it only reads the first element of are bound as uniform buffers there

// @FIXME: Better fix! Or just use cbuffers everywhere?
DECLARE_BUFFER(GlimmerProbeTrace, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};
#else
DECLARE_SRV(GlimmerProbeTrace, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]
#endif

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
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeLightBuffer) StructuredBuffer<float4> glimmerProbeLight;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeStatesBuffer) StructuredBuffer<uint4> glimmerProbeStates;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeVisibilityBuffer) StructuredBuffer<uint> glimmerProbeVisibility;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeSlotsBuffer) StructuredBuffer<int4> glimmerProbeSlots;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeUpdateListBuffer) StructuredBuffer<uint> glimmerProbeUpdateList;
#ifdef BACKEND_WEBGPU
DECLARE_BUFFER(GlimmerProbeTrace, GlimmerProbeCountersBuffer) cbuffer GlimmerProbeCountersBuffer
{
    uint glimmerProbeUpdateCount;
};
#else
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeCountersBuffer) StructuredBuffer<uint> glimmerProbeCounters;
#define glimmerProbeUpdateCount glimmerProbeCounters[0]
#endif

DECLARE_SRV(GlimmerProbeTrace, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(GlimmerProbeTrace, CloudWeatherMapTexture) Texture2DArray cloudWeatherMapTexture;
DECLARE_SRV(GlimmerProbeTrace, CloudShadowMapTexture) Texture2D cloudShadowMapTexture;

DECLARE_SRV(GlimmerProbeTrace, GlimmerSHDataTexture) Texture3D<float4> glimmerSHData;
DECLARE_SRV(GlimmerProbeTrace, GlimmerSHStateTexture) Texture3D<uint4> glimmerSHState;
DECLARE_SRV(GlimmerProbeTrace, GlimmerSHRadianceTexture) Texture3D<float4> glimmerSHRadiance;
DECLARE_SRV(GlimmerProbeTrace, GlimmerSHLightBuffer) StructuredBuffer<uint> glimmerSHLight;

/// GLIMMER_RAY_UINTS per ray: the hit distance (float bits), then the light's parts
/// the distance is negative for the back face of one sided geometry, 0 where the probe is underground
DECLARE_UAV(GlimmerProbeTrace, OutRays) RWStructuredBuffer<uint> OutRays;
DECLARE_UAV(GlimmerProbeTrace, RayHits) RWStructuredBuffer<GlimmerProbeRayHit> RayHits;

#if defined(MODE_RELIGHT)
DECLARE_UAV(GlimmerProbeTrace, OutRelight) RWTexture2DArray<float4> OutRelight;
#else
DECLARE_SRV(GlimmerProbeTrace, GlimmerRelightTexture) Texture2DArray<float4> glimmerRelight;

#define GLIMMER_RELIGHT_WITH_SAMPLING
#include "../GlimmerRelight.hlsli"
#undef GLIMMER_RELIGHT_WITH_SAMPLING
#endif

#define GLIMMER_PROBES_SPLIT
#include "GlimmerProbes.hlsli"
#include "GlimmerSWRT.hlsli"
#include "GlimmerSWRTFootprint.hlsli"
#include "../GlimmerMaterial.hlsli"

#define GLIMMER_SH_SPLIT
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

#define GLIMMER_MASK_RAY_LEVEL_BIAS 1

#define GLIMMER_SPAN_FOOTPRINT_SLACK 1.5

#if defined(MODE_RELIGHT)

bool GlimmerRelightSurfaceAt(uint level, uint surface, int2 texel, out float3 outP, out float3 outN)
{
    const float2 xz = (float2(texel) + 0.5) * constants.ground.levels[level].params.x;

    outP = float3(xz.x, 0.0, xz.y);
    outN = float3(0.0, 1.0, 0.0);

    if (surface == GLIMMER_RELIGHT_GROUND)
    {
        float groundHeight = 0.0;

        if (!GlimmerSampleGroundLevel(constants.ground, level, xz, groundHeight))
        {
            return false;
        }

        outP.y = groundHeight;
        outN = GlimmerGroundNormal(constants.ground, xz, level);

        return true;
    }

    GlimmerSpanSample spanSample;

    if (!GlimmerSampleSpans(constants.spans, level, xz, spanSample) || spanSample.solidBins == 0u)
    {
        return false;
    }

    outP.y = spanSample.solidMin + GlimmerSpanHeight(spanSample.solidMin, spanSample.solidMax);

    return true;
}

// texel is the one that lights its 2x2 block
void GlimmerRelightBlock(int2 texel, uint level, uint surface)
{
    float3 P = (float3)0.0;
    float3 N = float3(0.0, 1.0, 0.0);

    // what the block receives is taken where its first texel with the surface is
    bool hasSurface = false;

    [unroll]
    for (uint blockTexel = 0; blockTexel < 4; blockTexel++)
    {
        if (!hasSurface)
        {
            hasSurface = GlimmerRelightSurfaceAt(level, surface, texel + int2(blockTexel & 1u, blockTexel >> 1), P, N);
        }
    }

    GlimmerLight received = GlimmerNoLight();

    if (hasSurface)
    {
        received = GlimmerIndirect(P, N);
    }

    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        OutRelight[GlimmerRelightTexel(texel, level, surface, part)] = float4(received.parts[part], part == 0u && !hasSurface ? -1.0 : 0.0);
    }
}

#endif

/// 1 lane per ray, 2 probes per group
/// NOTE must match GlimmerProbeRays and ProbesPerTraceGroup in GlimmerSWRTProbeVolume
#define RAYS_PER_PROBE 32
#define PROBES_PER_GROUP 2

void GlimmerWriteRay(uint rayRecordIndex, GlimmerLight light, float hitT, float escape)
{
    const uint base = rayRecordIndex * GLIMMER_RAY_UINTS;

    OutRays[base] = asuint(hitT);
    OutRays[base + 1u] = asuint(escape);

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        const uint2 packed = GlimmerPackLightPart(light.parts[part]);

        OutRays[base + 2u + part * 2u] = packed.x;
        OutRays[base + 3u + part * 2u] = packed.y;
    }
}

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

    rayHit.inscatter = (uint[GLIMMER_LIGHT_PACKED_UINTS_PADDED])0u;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        const uint2 packed = GlimmerPackLightPart(heightfieldHit.inscatter.parts[part]);

        rayHit.inscatter[part * 2u] = packed.x;
        rayHit.inscatter[part * 2u + 1u] = packed.y;
    }

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

    if (!GlimmerRelightLightsBlock(texel))
    {
        return;
    }

    GlimmerRelightBlock(texel, level, GLIMMER_RELIGHT_GROUND);
    GlimmerRelightBlock(texel, level, GLIMMER_RELIGHT_SPAN_TOP);
#else
    const uint listIndex = groupId.x * PROBES_PER_GROUP + groupIndex / RAYS_PER_PROBE;

    if (listIndex >= min(glimmerProbeUpdateCount, constants.dispatch.x))
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

        const GlimmerLight surfaceLight = GlimmerShadeSurface(rayHit.positionT.xyz, rayHit.normalFlags.xyz, rayHit.albedoTransmittance.rgb);

        GlimmerLight rayLight;

        [unroll]
        for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
        {
            rayLight.parts[part] = GlimmerUnpackLightPart(uint2(rayHit.inscatter[part * 2u], rayHit.inscatter[part * 2u + 1u]));
        }

        GlimmerAddLight(rayLight, surfaceLight, (float3)rayHit.albedoTransmittance.a);

        GlimmerWriteRay(rayRecordIndex, rayLight, rayHit.positionT.w, 0.0);
    }
#else
    const uint probeIndex = glimmerProbeUpdateList[listIndex];
    const int4 slot = glimmerProbeSlots[probeIndex / GLIMMER_PROBES_PER_BLOCK];
    const uint levelIndex = uint(slot.w);

    const GlimmerProbeLevel level = constants.volume.levels[levelIndex];
    const float3 origin = GlimmerProbePosition(constants.volume, probeIndex, slot, glimmerProbeStates[probeIndex]);

    const uint numInstances = uint(constants.volume.nearField.z);

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
        
        GlimmerLight light = GlimmerNoLight();
        
        float escape = 0.0;

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
                if (!GlimmerShadeRelitHit(P, albedo, heightfieldHit.normal, heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND, heightfieldHit.level, light))
                {
                    GlimmerWriteRayHit(
                        rayRecordIndex,
                        P + heightfieldHit.normal * 0.05,
                        heightfieldHit.normal,
                        hitT,
                        GLIMMER_RAY_HIT_SHADE,
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

            if (!isBackface)
            {
                const float3 P = origin + direction * hitT;
                const float3 N = GlimmerGetHitNormal(hit, direction);

                // the shade pass adds its lighting to the inscatter, so the surface's own light rides in with it
                float3 emissive = GlimmerGetMaterialEmissive(instance.data.z);

                const float emissiveLuminance = GlimmerLuminance(emissive);

                if (emissiveLuminance > constants.volume.params.z)
                {
                    emissive *= constants.volume.params.z / emissiveLuminance;
                }

                GlimmerHeightfieldHit emittingHit = heightfieldHit;
                GlimmerAddLight(emittingHit.inscatter, GlimmerRestLight(emissive), (float3)heightfieldHit.transmittance);

                GlimmerWriteRayHit(
                    rayRecordIndex,
                    P,
                    N,
                    hitT,
                    GLIMMER_RAY_HIT_SHADE,
                    GlimmerGetMaterialAverageAlbedo(instance.data.z),
                    emittingHit);

                isDone = false;
            }
        }
        else
        {
            escape = heightfieldHit.transmittance;
        }

        if (isDone)
        {
            RayHits[rayRecordIndex].normalFlags.w = asfloat(GLIMMER_RAY_HIT_DONE);

            GlimmerLight rayLight = heightfieldHit.inscatter;
            GlimmerAddLight(rayLight, light, (float3)heightfieldHit.transmittance);

            GlimmerWriteRay(rayRecordIndex, rayLight, select(isBackface, -hitT, hitT), escape);
        }
    }
#endif
#endif
}
