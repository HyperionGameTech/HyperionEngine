#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

PERMUTE(MODE, TRACE, SHADE, RELIGHT)
PERMUTE(TRACER, BRICKS, BVH)

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/EnvProbes.hlsli"
#include "../GlimmerCommon.hlsli"
#include "../SH/GlimmerSHCommon.hlsli"

#include "../GlimmerRelight.hlsli"
#include "../GlimmerLocalLights.hlsli"
#include "GlimmerSurfelTypes.hlsli"

#if defined(TRACER_BVH)
#include "../../Include/RayTracing/BVH.hlsli"
#include "../BVH/GlimmerBVHCommon.hlsli"

DECLARE_SRV(GlimmerSurfelTrace, GlimmerTLASNodesBuffer) StructuredBuffer<BVHNode> glimmerTLASNodes;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerTracedInstancesBuffer) StructuredBuffer<GlimmerTracedInstance> glimmerTracedInstances;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerBLASNodesBuffer) StructuredBuffer<BVHBLASNode> glimmerBLASNodes;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerTrianglesBuffer) StructuredBuffer<uint> glimmerTriangles;

#include "../BVH/GlimmerBVHTrace.hlsli"

#define GLIMMER_BRICKS_NO_TRACE
#include "../Bricks/GlimmerBricks.hlsli"
#undef GLIMMER_BRICKS_NO_TRACE
#else
DECLARE_SRV(GlimmerSurfelTrace, GlimmerBrickTableBuffer) StructuredBuffer<uint> glimmerBrickTable;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerBrickIndexBuffer) StructuredBuffer<uint> glimmerBrickIndex;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerBrickCrossingsBuffer) StructuredBuffer<uint> glimmerBrickCrossings;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerBrickMaterialBuffer) StructuredBuffer<uint> glimmerBrickMaterial;

#include "../Bricks/GlimmerBricks.hlsli"
#endif

#define GLIMMER_MAX_EMITTERS 64

/// halves, two emitters to an element (xy, then zw)
struct GlimmerEmitters
{
    uint4 info;                                 // x = number of emitters
    float4 origin;                              // xyz = what the centers are relative to
    uint4 spheres[GLIMMER_MAX_EMITTERS / 2];    // xyz = center, w = radius of the sphere rays hit
    uint4 radiance[GLIMMER_MAX_EMITTERS / 2];   // rgb = radiance over that sphere, w = radius of the emitter itself
};

float4 GlimmerUnpackEmitterHalves(uint4 pair, uint emitterIndex)
{
    uint2 packed = pair.xy;

    if ((emitterIndex & 1u) != 0u)
    {
        packed = pair.zw;
    }

    return float4(GlimmerUnpackHalf2(packed.x), GlimmerUnpackHalf2(packed.y));
}

struct GlimmerSurfelTraceConstants
{
    GlimmerSurfels surfels;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;

    /// x = how far rays look before taking the sky
    /// y = luminance emissive surfaces and emitters are clamped to
    /// z = 1 when there are bricks or a BVH to trace near
    /// w = albedo where the ground's isn't known
    float4 trace;

    /// x = foliage extinction
    float4 params;
    GlimmerSkyParams sky;

    GlimmerSHVolume sh;
    EnvProbe skyProbe;   // textureIndices is ~0 without a sky probe. used for ratios in relight pass.
    GlimmerRelightParams relight;

    /// relight pass ///
    /// xy = texel of the rect to light
    /// zw = its extent
    int4 relightRect;

    /// x = ground level
    /// y = groups along x
    uint4 relightInfo;

    ////////////////////

    GlimmerBrickParams bricks;

    /// x = traced instances in the TLAS
    uint4 bvh;

    GlimmerLocalLights localLights;
    GlimmerEmitters emitters;
    GlimmerLightParams light;
};

#include "../../Include/Clouds.hlsli"

DECLARE_BUFFER_DYNAMIC(GlimmerSurfelTrace, CBuffer) cbuffer CBuffer
{
    GlimmerSurfelTraceConstants constants;
    CloudVolume cloudVolume;
    CloudWeatherMap cloudWeatherMap;
    CloudShadowMap cloudShadowMap;
};

#define skyProbe constants.skyProbe

#ifdef BACKEND_WEBGPU
/// WebGPU allows 16 storage buffers in a stage.
/// the trace pass only reads the first element of this one so it is bound as a uniform buffer.

// @FIXME: Better fix! Or just use cbuffers everywhere?
DECLARE_BUFFER(GlimmerSurfelTrace, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};
#else
DECLARE_SRV(GlimmerSurfelTrace, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]
#endif

DECLARE_SRV(GlimmerSurfelTrace, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerSurfelTrace, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerSurfelTrace, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;

DECLARE_SRV(GlimmerSurfelTrace, GlimmerSurfelsBuffer) StructuredBuffer<uint4> glimmerSurfels;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerSurfelHashBuffer) StructuredBuffer<uint> glimmerSurfelHash;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerSurfelDataBuffer) StructuredBuffer<uint> glimmerSurfelData;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerFoliageBuffer) StructuredBuffer<uint> glimmerFoliage;

#include "GlimmerSurfelsLight.hlsli"

DECLARE_SRV(GlimmerSurfelTrace, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(GlimmerSurfelTrace, CloudWeatherMapTexture) Texture2DArray cloudWeatherMapTexture;
DECLARE_SRV(GlimmerSurfelTrace, CloudShadowMapTexture) Texture2D cloudShadowMapTexture;

DECLARE_SRV(GlimmerSurfelTrace, GlimmerSHDataTexture) Texture3D<float4> glimmerSHData;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerSHStateTexture) Texture3D<uint4> glimmerSHState;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerSHRadianceTexture) Texture3D<float4> glimmerSHRadiance;
DECLARE_SRV(GlimmerSurfelTrace, GlimmerSHLightBuffer) StructuredBuffer<uint> glimmerSHLight;

/// GLIMMER_SURFEL_RAY_RECORD_UINTS per ray: the hit distance (float bits), what escapes to the sky, then the light's parts
/// the distance is negative for the back face of one sided geometry, 0 where the ray starts underground
/// a hit the trace pass leaves to be shaded has a negative escape, the light the canopy scattered toward the surfel in front of it
/// for its light, and after that its position, normal, albedo and the canopy's transmittance in front of it
DECLARE_UAV(GlimmerSurfelTrace, OutRays) RWStructuredBuffer<uint> OutRays;

#define GLIMMER_RAY_HIT_SHADE -1.0
#define GLIMMER_RAY_HIT_WORDS (GLIMMER_SURFEL_RAY_SUN_UINT + 2u)

#if defined(MODE_RELIGHT)
DECLARE_UAV(GlimmerSurfelTrace, OutRelight) RWTexture2DArray<float4> OutRelight;
#else
DECLARE_SRV(GlimmerSurfelTrace, GlimmerRelightTexture) Texture2DArray<float4> glimmerRelight;

#define GLIMMER_RELIGHT_WITH_SAMPLING
#include "../GlimmerRelight.hlsli"
#undef GLIMMER_RELIGHT_WITH_SAMPLING
#endif

#include "../GlimmerMaterial.hlsli"

#define GLIMMER_SH_SPLIT
#define GLIMMER_APPLY_WITH_SAMPLING
#define GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#include "../SH/GlimmerSHApply.hlsli"
#undef GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#undef GLIMMER_APPLY_WITH_SAMPLING

#define GLIMMER_LIGHTING_SKY constants.sky
#define GLIMMER_LIGHTING_SURFELS constants.surfels
#define GLIMMER_LIGHTING_SH constants.sh
#define GLIMMER_LIGHTING_GROUND constants.ground
#define GLIMMER_LIGHTING_SPANS constants.spans
#define GLIMMER_LIGHTING_RELIGHT constants.relight
#include "../GlimmerLighting.hlsli"

#include "../GlimmerHeightfield.hlsli"

float GlimmerSurfelShadowTransmittance(float3 origin, float3 direction, float tMax)
{
#if defined(TRACER_BVH)
    GlimmerBVHHit bvhHit;
    GlimmerBVHStats stats = GlimmerMakeBVHStats();

    if (TraceGlimmerBVH(origin, direction, 0.0, tMax, constants.bvh.x, true, bvhHit, stats))
    {
        return 0.0;
    }
#else
    GlimmerBrickHit brickHit;

    if (TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, tMax, brickHit))
    {
        return 0.0;
    }
#endif

    GlimmerHeightfieldHit heightfieldHit;

    if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, tMax, 0u, tMax, constants.params.x, false, heightfieldHit))
    {
        return 0.0;
    }

    return heightfieldHit.transmittance;
}

#define GLIMMER_LOCAL_LIGHT_TRANSMITTANCE GlimmerSurfelShadowTransmittance
#include "../GlimmerLocalLights.hlsli"
#undef GLIMMER_LOCAL_LIGHT_TRANSMITTANCE

float3 GlimmerClampEmitted(float3 emitted)
{
    const float emittedLuminance = GlimmerLuminance(emitted);

    if (emittedLuminance > constants.trace.y)
    {
        emitted *= constants.trace.y / emittedLuminance;
    }

    return emitted;
}

void GlimmerAddEmitters(float3 origin, float3 direction, float hitT, float tMax, inout GlimmerLight ioInscatter)
{
    for (uint emitterIndex = 0; emitterIndex < constants.emitters.info.x; emitterIndex++)
    {
        const float4 sphere = GlimmerUnpackEmitterHalves(constants.emitters.spheres[emitterIndex >> 1u], emitterIndex);
        const float4 radiance = GlimmerUnpackEmitterHalves(constants.emitters.radiance[emitterIndex >> 1u], emitterIndex);

        const float centerError = max(abs(sphere.x), max(abs(sphere.y), abs(sphere.z))) * (1.0 / 1024.0);

        const float3 toCenter = constants.emitters.origin.xyz + sphere.xyz - origin;
        const float tCenter = dot(toCenter, direction);

        if (tCenter <= 0.0 || tCenter > tMax || dot(toCenter, toCenter) <= sphere.w * sphere.w)
        {
            continue;
        }

        const float3 offCenter = toCenter - direction * tCenter;

        if (dot(offCenter, offCenter) > sphere.w * sphere.w || hitT < tCenter - radiance.w * 1.1 - centerError)
        {
            continue;
        }

        GlimmerAddLight(ioInscatter, GlimmerRestLight(GlimmerClampEmitted(radiance.rgb)), (float3)1.0);
    }
}

GlimmerLight GlimmerReceivedAtWithSun(float3 P, float3 N, out float3 outSun)
{
    GlimmerLight received = GlimmerIndirectWithSun(P, N, outSun);
    received.parts[GLIMMER_LIGHT_REST] += GlimmerLocalLightsAt(constants.localLights, P, N);

    return received;
}

GlimmerLight GlimmerReceivedAt(float3 P, float3 N)
{
    float3 sun;

    return GlimmerReceivedAtWithSun(P, N, sun);
}

// irradiance / pi a sun of unit irradiance gives a surface facing N at P straight, behind a shadow ray; the clouds move, so they dim it when it is composed
float GlimmerSurfelSunAtFrom(float3 P, float3 N, float3 toSun)
{
    const float cosine = dot(N, toSun);

    [branch]
    if (cosine <= 0.0 || (constants.surfels.foliage.w == 0.0 && !GlimmerSurfelHasSunKeys(constants.surfels)))
    {
        return 0.0;
    }

    return cosine * HYP_FMATH_ONE_OVER_PI * GlimmerSurfelShadowTransmittance(P + N * GLIMMER_SURFEL_ORIGIN_BIAS, toSun, constants.trace.x);
}

// how much of the sun reaches a surfel at a slot (GLIMMER_SURFEL_SUN_SLOTS): one of the keys when there are keys, or the sun now
float GlimmerSurfelSunVisibility(float3 origin, float3 surfelNormal, uint slot)
{
    float3 toSun = GlimmerSurfelSunDirection(constants.surfels);

    if (slot < GLIMMER_SUN_KEYS)
    {
        if (!GlimmerSurfelHasSunKeys(constants.surfels))
        {
            return 0.0;
        }

        toSun = constants.surfels.sunKeys[slot].xyz;
    }

    if (dot(surfelNormal, toSun) <= 0.0)
    {
        return 0.0;
    }

    return GlimmerSurfelShadowTransmittance(origin, toSun, constants.trace.x);
}

#define GLIMMER_SURFEL_SUN_VISIBILITY_COVERAGE 0.5

float GlimmerSurfelSunAt(float3 P, float3 N)
{
    return GlimmerSurfelSunAtFrom(P, N, GlimmerSurfelSunDirection(constants.surfels));
}

bool GlimmerTracesAllSunKeys()
{
    return glimmerSunKey == GLIMMER_SUN_KEY_ALL;
}

void GlimmerWriteRaySunKeys(uint rayRecordIndex, float3 suns[GLIMMER_SUN_KEYS])
{
    const uint base = rayRecordIndex * GLIMMER_SURFEL_RAY_RECORD_UINTS + GLIMMER_SURFEL_RAY_SUN_KEYS_UINT;

    [unroll]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        OutRays[base + key] = GlimmerPackRGB9E5(suns[key]);
    }
}

// the first of the keys a hit tests the sun's visibility for, the rest following GLIMMER_SURFEL_SUN_KEY_STRIDE apart: a surfel's rays take turns, so every key is tested by some of them each update
uint GlimmerShadowedSunKeyStart(uint rayRecordIndex)
{
    return (GlimmerSurfelHash((rayRecordIndex / GLIMMER_SURFEL_RAYS) * 0x9E3779B9u + constants.surfels.info.z * 0x85EBCA6Bu) + rayRecordIndex) % GLIMMER_SURFEL_SUN_KEY_STRIDE;
}

// a key's sunlight at a hit behind its shadow ray, for the keys the hit tests, counting for the ones between that it doesn't
float GlimmerSurfelSunAtKey(float3 P, float3 N, uint key, uint shadowedStart)
{
    if (key % GLIMMER_SURFEL_SUN_KEY_STRIDE != shadowedStart)
    {
        return 0.0;
    }

    return GlimmerSurfelSunAtFrom(P, N, constants.surfels.sunKeys[key].xyz) * float(GLIMMER_SURFEL_SUN_KEY_STRIDE);
}

// what the canopy in front of a hit scatters of each key's sun, then the hit's own sunlight for each behind its shadow ray
void GlimmerRaySunKeys(GlimmerLight inscatter, bool isLit, float3 P, float3 N, float3 albedo, float transmittance, uint rayRecordIndex, out float3 outSuns[GLIMMER_SUN_KEYS])
{
    const uint shadowedStart = GlimmerShadowedSunKeyStart(rayRecordIndex);

    [loop]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        outSuns[key] = GlimmerSmoothSun(inscatter, constants.surfels.sunKeys[key].xyz);

        if (isLit)
        {
            outSuns[key] += min(albedo, (float3)GLIMMER_MAX_ALBEDO) * GlimmerSurfelSunAtKey(P, N, key, shadowedStart) * transmittance;
        }
    }
}

float3 GlimmerSurfelSmoothSun(GlimmerLight light)
{
    return GlimmerSmoothSun(light, GlimmerSurfelSunDirection(constants.surfels));
}

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
    float3 receivedSun = (float3)0.0;

    if (hasSurface)
    {
        received = GlimmerReceivedAtWithSun(P, N, receivedSun);
    }

    // a holds the received sun bounce in parts 1-3, and marks a block without the surface in part 0
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        float alpha = 0.0;

        if (part == 0u && !hasSurface)
        {
            alpha = -1.0;
        }
        else if (part >= 1u && part <= 3u)
        {
            alpha = receivedSun[part - 1u];
        }

        OutRelight[GlimmerRelightTexel(texel, level, surface, part)] = float4(received.parts[part], alpha);
    }
}

#endif

/// 1 lane per ray, 2 surfels per group
/// NOTE must match GlimmerSurfelRays and SurfelsPerTraceGroup in GlimmerSurfelCache.cpp
#define SURFELS_PER_GROUP 2

// the sun bounce is per unit of the sun's irradiance, for the sun the surfels are traced for, and stands in for what the light's terms carry of it
void GlimmerWriteRay(uint rayRecordIndex, GlimmerLight light, float3 sun, float hitT, float escape)
{
    const uint base = rayRecordIndex * GLIMMER_SURFEL_RAY_RECORD_UINTS;

    OutRays[base] = asuint(hitT);
    OutRays[base + 1u] = asuint(escape);

    const uint2 packedSun = GlimmerPackLightPart(sun);

    OutRays[base + GLIMMER_SURFEL_RAY_SUN_UINT] = packedSun.x;
    OutRays[base + GLIMMER_SURFEL_RAY_SUN_UINT + 1u] = packedSun.y;

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
    float3 albedo,
    GlimmerHeightfieldHit heightfieldHit)
{
    GlimmerWriteRay(rayRecordIndex, heightfieldHit.inscatter, GlimmerSurfelSmoothSun(heightfieldHit.inscatter), hitT, GLIMMER_RAY_HIT_SHADE);

    if (GlimmerTracesAllSunKeys())
    {
        float3 suns[GLIMMER_SUN_KEYS];
        GlimmerRaySunKeys(heightfieldHit.inscatter, false, P, N, albedo, 0.0, rayRecordIndex, suns);
        GlimmerWriteRaySunKeys(rayRecordIndex, suns);
    }

    const uint base = rayRecordIndex * GLIMMER_SURFEL_RAY_RECORD_UINTS + GLIMMER_RAY_HIT_WORDS;

    [unroll]
    for (uint axis = 0; axis < 3u; axis++)
    {
        OutRays[base + axis] = asuint(P[axis]);
        OutRays[base + 3u + axis] = asuint(N[axis]);
        OutRays[base + 6u + axis] = asuint(albedo[axis]);
    }

    OutRays[base + 9u] = asuint(heightfieldHit.transmittance);
}

[numthreads(GLIMMER_SURFEL_RAYS * SURFELS_PER_GROUP, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
#if defined(MODE_RELIGHT)
    const uint texelIndex = (groupId.y * constants.relightInfo.y + groupId.x) * (GLIMMER_SURFEL_RAYS * SURFELS_PER_GROUP) + groupIndex;
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
    const uint listIndex = groupId.x * SURFELS_PER_GROUP + groupIndex / GLIMMER_SURFEL_RAYS;
    const uint surfelCount = GlimmerSurfelCount(glimmerSurfelHash[GLIMMER_SURFEL_COUNTER_SURFELS]);
    const uint foliageCount = GlimmerFoliageCount(glimmerSurfelHash[GLIMMER_SURFEL_COUNTER_FOLIAGE]);

    if (listIndex >= min(surfelCount + foliageCount, constants.surfels.info.x))
    {
        return;
    }

    const uint rayIndex = groupIndex % GLIMMER_SURFEL_RAYS;
    const uint rayRecordIndex = listIndex * GLIMMER_SURFEL_RAYS + rayIndex;

    // surfels and foliage voxels bake their sun bounce for every key along the sun's arc at once
    if (GlimmerSurfelHasSunKeys(constants.surfels))
    {
        glimmerSunKey = GLIMMER_SUN_KEY_ALL;
    }

#if defined(MODE_SHADE)
    const uint base = rayRecordIndex * GLIMMER_SURFEL_RAY_RECORD_UINTS;

    if (asfloat(OutRays[base + 1u]) >= 0.0)
    {
        return;
    }

    const uint hitBase = base + GLIMMER_RAY_HIT_WORDS;

    const float3 P = asfloat(uint3(OutRays[hitBase], OutRays[hitBase + 1u], OutRays[hitBase + 2u]));
    const float3 N = asfloat(uint3(OutRays[hitBase + 3u], OutRays[hitBase + 4u], OutRays[hitBase + 5u]));
    const float3 albedo = asfloat(uint3(OutRays[hitBase + 6u], OutRays[hitBase + 7u], OutRays[hitBase + 8u]));
    const float transmittance = asfloat(OutRays[hitBase + 9u]);

    GlimmerLight rayLight;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        rayLight.parts[part] = GlimmerUnpackLightPart(uint2(OutRays[base + 2u + part * 2u], OutRays[base + 3u + part * 2u]));
    }

    float3 receivedSun;
    const GlimmerLight surfaceLight = GlimmerShade(albedo, GlimmerReceivedAtWithSun(P, N, receivedSun));

    GlimmerAddLight(rayLight, surfaceLight, (float3)transmittance);

    float3 raySun = GlimmerUnpackLightPart(uint2(OutRays[base + GLIMMER_SURFEL_RAY_SUN_UINT], OutRays[base + GLIMMER_SURFEL_RAY_SUN_UINT + 1u]));

    if (GlimmerTracesAllSunKeys())
    {
        const uint shadowedStart = GlimmerShadowedSunKeyStart(rayRecordIndex);

        float3 receivedSuns[GLIMMER_SUN_KEYS];
        float sunVisibility[GLIMMER_SURFEL_SUN_SLOTS];
        const float sunVisibilityCoverage = GlimmerIndirectSunKeys(P, N, shadowedStart, receivedSuns, sunVisibility);

        const float3 shadedAlbedo = min(albedo, (float3)GLIMMER_MAX_ALBEDO);
        const uint keysBase = base + GLIMMER_SURFEL_RAY_SUN_KEYS_UINT;

        [loop]
        for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
        {
            // the surfels the hit lands among hold how much sun reaches them; where they don't cover it, it traces its own
            float directSun;

            if (sunVisibilityCoverage >= GLIMMER_SURFEL_SUN_VISIBILITY_COVERAGE)
            {
                directSun = max(dot(N, constants.surfels.sunKeys[key].xyz), 0.0) * HYP_FMATH_ONE_OVER_PI * sunVisibility[key];
            }
            else
            {
                directSun = GlimmerSurfelSunAtKey(P, N, key, shadowedStart);
            }

            const float3 sunAt = directSun + receivedSuns[key];

            OutRays[keysBase + key] = GlimmerPackRGB9E5(GlimmerUnpackRGB9E5(OutRays[keysBase + key]) + shadedAlbedo * sunAt * transmittance);
        }
    }
    else
    {
        raySun += min(albedo, (float3)GLIMMER_MAX_ALBEDO) * (GlimmerSurfelSunAt(P, N) + receivedSun) * transmittance;
    }

    GlimmerWriteRay(rayRecordIndex, rayLight, raySun, asfloat(OutRays[base]), 0.0);
#else
    const float maxDistance = constants.trace.x;

    const uint item = GlimmerTracedItem(constants.surfels, listIndex, surfelCount, foliageCount);

    float3 origin;
    float3 direction;

    if (item < surfelCount)
    {
        const uint4 surfel = glimmerSurfels[item];
        const float3 surfelNormal = GlimmerUnpackSurfelNormal(surfel.w);

        origin = asfloat(surfel.xyz) + surfelNormal * GLIMMER_SURFEL_ORIGIN_BIAS;
        direction = GlimmerSurfelRayDirection(surfelNormal, item, constants.surfels.info.z, rayIndex);

        if (rayIndex < GLIMMER_SURFEL_SUN_SLOTS)
        {
            OutRays[rayRecordIndex * GLIMMER_SURFEL_RAY_RECORD_UINTS + GLIMMER_SURFEL_RAY_SUN_VISIBILITY_UINT] = asuint(GlimmerSurfelSunVisibility(origin, surfelNormal, rayIndex));
        }
    }
    else
    {
        const uint voxelIndex = item - surfelCount;

        const int3 cell = int3(
            asint(glimmerFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL)]),
            asint(glimmerFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL + 1u)]),
            asint(glimmerFoliage[GlimmerFoliageIndex(voxelIndex, GLIMMER_FOLIAGE_CELL + 2u)]));

        origin = GlimmerFoliageVoxelCenter(constants.surfels, cell);
        direction = GlimmerFoliageRayDirection(voxelIndex, constants.surfels.info.z, rayIndex);
    }

    const bool traceNear = constants.trace.z != 0.0;
    const float nearRange = min(constants.surfels.params.w, maxDistance);

    // finer levels that don't cover a surfel fall through to coarser ones in GlimmerSampleGround
    const uint startLevel = 0u;

    float hitT = maxDistance;

    GlimmerLight light = GlimmerNoLight();
    float3 litSun = (float3)0.0;

    bool isRelit = false;
    float3 relitP = (float3)0.0;
    float3 relitN = (float3)0.0;
    float3 relitAlbedo = (float3)0.0;

    float escape = 0.0;

    bool isDone = true;
    bool isBackface = false;

#if defined(TRACER_BVH)
    GlimmerBVHHit bvhHit;
    bool hitNear = false;

    // the BVH owns the solids up to where the ray leaves it; the spans' solids count from there
    const float solidsFromT = traceNear ? min(GlimmerBVHWindowExit(origin, direction, constants.bvh.x), nearRange) : 0.0;

    if (solidsFromT > 0.0)
    {
        GlimmerBVHStats stats = GlimmerMakeBVHStats();

        hitNear = TraceGlimmerBVH(origin, direction, 0.0, solidsFromT, constants.bvh.x, false, bvhHit, stats);

        if (hitNear)
        {
            hitT = bvhHit.t;
        }
    }
#else
    GlimmerBrickHit brickHit;
    bool hitNear = false;

    // the bricks own the solids up to where the ray leaves them; the spans' solids count from there
    const float solidsFromT = traceNear ? min(GlimmerBrickWindowExit(constants.bricks, origin, direction), nearRange) : 0.0;

    if (solidsFromT > 0.0)
    {
        hitNear = TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, solidsFromT, brickHit);

        if (hitNear)
        {
            hitT = brickHit.t;
        }
    }
#endif

    GlimmerHeightfieldHit heightfieldHit;

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

#if defined(TRACER_BVH)
    if (solidsFromT > 0.0)
    {
        GlimmerAddEmitters(origin, direction, hitHeightfield ? heightfieldHit.t : hitT, solidsFromT, heightfieldHit.inscatter);
    }
#endif

    if (hitHeightfield)
    {
        hitT = heightfieldHit.t;

        const float3 P = origin + direction * hitT;

        const float3 albedo = heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND
            ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, heightfieldHit.level, (float3)constants.trace.w)
            : heightfieldHit.albedo;

        if (hitT > 0.0)
        {
            float3 relitSun;

            if (GlimmerShadeRelitHit(P, albedo, heightfieldHit.normal, heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND, heightfieldHit.level, light, relitSun))
            {
                isRelit = true;
                relitP = P;
                relitN = heightfieldHit.normal;
                relitAlbedo = albedo;

                if (!GlimmerTracesAllSunKeys())
                {
                    litSun = min(albedo, (float3)GLIMMER_MAX_ALBEDO) * (GlimmerSurfelSunAt(P, heightfieldHit.normal) + relitSun);
                }
            }
            else
            {
                GlimmerWriteRayHit(
                    rayRecordIndex,
                    P + heightfieldHit.normal * 0.05,
                    heightfieldHit.normal,
                    hitT,
                    albedo,
                    heightfieldHit);

                isDone = false;
            }
        }
    }
#if defined(TRACER_BVH)
    else if (hitNear)
    {
        const GlimmerTracedInstance instance = glimmerTracedInstances[bvhHit.instanceIndex];

        isBackface = !bvhHit.frontFace && (instance.data.w & (GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED | GLIMMER_INSTANCE_FLAG_ALPHA_TESTED)) == 0u;

        if (!isBackface)
        {
            const float3 P = origin + direction * hitT;
            const float3 N = GlimmerGetHitNormal(bvhHit, direction);

            float3 emissive = (float3)0.0;

            if ((instance.data.w & GLIMMER_INSTANCE_FLAG_EMITTER) == 0u)
            {
                emissive = GlimmerClampEmitted(GlimmerGetMaterialEmissive(instance.data.z));
            }

            GlimmerHeightfieldHit emittingHit = heightfieldHit;
            GlimmerAddLight(emittingHit.inscatter, GlimmerRestLight(emissive), (float3)heightfieldHit.transmittance);

            GlimmerWriteRayHit(
                rayRecordIndex,
                P,
                N,
                hitT,
                GlimmerGetMaterialAverageAlbedo(instance.data.z),
                emittingHit);

            isDone = false;
        }
    }
#else
    else if (hitNear)
    {
        const uint packedMaterial = glimmerBrickMaterial[brickHit.crossing];
        const uint materialIndex = (packedMaterial & 0xFFFFu) == 0xFFFFu ? 0xFFFFFFFFu : (packedMaterial & 0xFFFFu);
        const bool isBack = dot(brickHit.normal, direction) > 0.0;

        isBackface = isBack && !GlimmerBrickHasFlag(packedMaterial, GLIMMER_BRICK_FLAG_DOUBLE_SIDED);

        if (!isBackface)
        {
            GlimmerHeightfieldHit emittingHit = heightfieldHit;
            GlimmerAddLight(emittingHit.inscatter, GlimmerRestLight(GlimmerClampEmitted(GlimmerGetMaterialEmissive(materialIndex))), (float3)heightfieldHit.transmittance);

            GlimmerWriteRayHit(
                rayRecordIndex,
                origin + direction * hitT,
                isBack ? -brickHit.normal : brickHit.normal,
                hitT,
                GlimmerBrickUnpackColor(glimmerBrickCrossings[2u * brickHit.crossing + 1u] >> 16u),
                emittingHit);

            isDone = false;
        }
    }
#endif
    else
    {
        escape = heightfieldHit.transmittance;
    }

    if (isDone)
    {
        GlimmerLight rayLight = heightfieldHit.inscatter;
        GlimmerAddLight(rayLight, light, (float3)heightfieldHit.transmittance);

        // the sky terms recompose the sky as it changes; what they smooth out of it (the glow around the sun) is kept as it was traced
        if (escape > 0.0 && constants.sky.info.x < INVALID_ENV_PROBE_TEXTURE)
        {
            float basis[GLIMMER_LIGHT_TERMS];
            GlimmerLightBasis(direction, basis);

            float3 fittedSky = (float3)0.0;

            [unroll]
            for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
            {
                fittedSky += constants.light.directWeights[term].rgb * basis[term];
            }

            const float3 sky = EnvProbeSample(glimmerMaterialSampler, envProbesColorTexture, constants.sky.info.x, direction, 0.0).rgb * constants.sky.params.w;

            rayLight.parts[GLIMMER_LIGHT_REST] += (sky - fittedSky) * escape;
        }

        const float3 raySun = GlimmerSurfelSmoothSun(heightfieldHit.inscatter) + litSun * heightfieldHit.transmittance;

        if (GlimmerTracesAllSunKeys())
        {
            float3 suns[GLIMMER_SUN_KEYS];
            GlimmerRaySunKeys(heightfieldHit.inscatter, isRelit, relitP, relitN, relitAlbedo, heightfieldHit.transmittance, rayRecordIndex, suns);
            GlimmerWriteRaySunKeys(rayRecordIndex, suns);
        }

        GlimmerWriteRay(rayRecordIndex, rayLight, raySun, select(isBackface, -hitT, hitT), escape);
    }
#endif
#endif
}
