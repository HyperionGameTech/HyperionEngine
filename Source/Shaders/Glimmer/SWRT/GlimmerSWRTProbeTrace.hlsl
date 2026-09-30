#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/RayTracing/BVH.hlsli"
#include "../../Include/EnvProbes.hlsli"
#include "GlimmerSWRTCommon.hlsli"
#include "GlimmerProbeTypes.hlsli"
#include "../SH/GlimmerSHCommon.hlsli"

// Two passes, so each kernel carries one BVH traversal (two inlined ones spill and halve occupancy for every ray):
//  TRACE finds what each ray hits (SWRT, then the heightfield) and writes a hit record, or the ray's final radiance when it escapes
//  SHADE lights the recorded hits (sun visibility is the second traversal) and writes their radiance
PERMUTE(MODE, TRACE, SHADE)

// A ray's hit between the passes
struct GlimmerProbeRayHit
{
    float4 positionT;           // xyz = shading position, w = hit distance for the ray record
    float4 normalFlags;         // xyz = normal, w = GLIMMER_RAY_HIT_* as uint
    float4 albedoTransmittance; // rgb = albedo, a = transmittance of the canopy in front of the hit
    float4 inscatter;           // rgb = light the canopy scattered toward the probe in front of the hit
};

#define GLIMMER_RAY_HIT_DONE 0u       // OutRays already holds the ray's radiance
#define GLIMMER_RAY_HIT_SHADE 1u
#define GLIMMER_RAY_HIT_SHADE_SWRT 2u // shade, and trace the sun against SWRT too

// Must match GlimmerProbeTraceConstants in GlimmerSWRTProbeVolume.cpp
struct GlimmerProbeTraceConstants
{
    GlimmerProbeVolume volume;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    uint4 dispatch; // y = sky probe color texture index (~0 without one)
    float4 sky;     // x = sky probe diffuse strength, y = foliage extinction
    uint4 cascadeDispatches[GLIMMER_PROBE_CASCADES]; // x = GLIMMER_PROBE_DISPATCH_*, y = slice period | slice << 16, z = probes
    GlimmerFootprintMaskParams mask;
    GlimmerSHVolume sh;  // the far field, sampled where a ray's hit is past the probes
    EnvProbe skyProbe;   // its spherical harmonics relight the SH voxels; textureIndices is ~0 without a sky probe
};

DECLARE_BUFFER_DYNAMIC(GlimmerProbeTrace, CBuffer) cbuffer CBuffer
{
    GlimmerProbeTraceConstants constants;
};

// what the SH apply code below expects to find declared
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
DECLARE_SRV(GlimmerProbeTrace, GlimmerBLASNodesBuffer) StructuredBuffer<BVHNode> glimmerBLASNodes;
DECLARE_SRV(GlimmerProbeTrace, GlimmerBLASTrianglesBuffer) StructuredBuffer<BVHTriangle> glimmerBLASTriangles;

DECLARE_SRV(GlimmerProbeTrace, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerProbeTrace, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerProbeTrace, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerProbeTrace, FootprintMaskBuffer) StructuredBuffer<uint> footprintMask;

DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeSHTexture) Texture3D<float4> glimmerProbeSH;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeStateTexture) Texture3D<uint2> glimmerProbeState;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeBaseTexture) Texture2DArray<float> glimmerProbeBase;

DECLARE_SRV(GlimmerProbeTrace, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(GlimmerProbeTrace, GlimmerSHDataTexture) Texture3D<float4> glimmerSHData;
DECLARE_SRV(GlimmerProbeTrace, GlimmerSHStateTexture) Texture3D<uint2> glimmerSHState;

// rgb = radiance, w = hit distance: negative for the back face of one sided geometry, 0 where the probe is under the ground
DECLARE_UAV(GlimmerProbeTrace, OutRays) RWStructuredBuffer<float4> OutRays;
DECLARE_UAV(GlimmerProbeTrace, RayHits) RWStructuredBuffer<GlimmerProbeRayHit> RayHits;

#include "GlimmerProbes.hlsli"
#include "GlimmerSWRT.hlsli"
#include "GlimmerSWRTFootprint.hlsli"
#include "../GlimmerMaterial.hlsli"

// the far field's sampling code, on the resources declared above
#if defined(MODE_SHADE)
#define GLIMMER_APPLY_WITH_SAMPLING
#define GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#include "../SH/GlimmerSHApply.hlsli"
#undef GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
#undef GLIMMER_APPLY_WITH_SAMPLING
#endif

float3 GlimmerSkyRadiance(float3 direction);

// Leaves scatter light both ways, so a canopy element is lit by the sun (dimmed by the leaves above it) and by its surroundings
float3 GlimmerCanopyRadiance(float3 P, float3 albedo, float depthBelowTop, float extinction)
{
    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);

    float3 radiance = (float3)0.0;

    if (L.y > 0.0)
    {
        const float3 sunIrradiance = world_shader_data.sun_color.rgb * world_shader_data.sun_direction_intensity.w;
        const float sunTransmittance = exp(-extinction * depthBelowTop / max(L.y, 0.15));

        radiance += sunIrradiance * sunTransmittance * (0.5 * 0.31830988618);
    }

    const float4 surroundings = SampleGlimmerProbes(constants.volume, P, float3(0.0, 1.0, 0.0));
    radiance += surroundings.a > 0.0 ? surroundings.rgb : GlimmerSkyRadiance(float3(0.0, 1.0, 0.0)) * 0.5;

    return min(albedo, (float3)0.9) * radiance;
}

#include "../GlimmerHeightfield.hlsli"

// sky probes are filtered cubemaps; this mip keeps the sun's forward scattering from spiking single rays
#define GLIMMER_SKY_MIP 3.0

#define GLIMMER_MAX_ALBEDO 0.9

// SWRT shadow rays toward the sun only need to reach past nearby occluders; the heightfield handles the terrain beyond
#define GLIMMER_SUN_SWRT_DISTANCE 256.0

// the footprint mask level rays walk: a cascade's rays walk cells as wide as its probe spacing, and sun rays (which are long) 4 m cells
#define GLIMMER_MASK_RAY_LEVEL_BIAS 1
#define GLIMMER_SUN_MASK_LEVEL 2

static const float GlimmerInvPi = 0.31830988618;

float3 GlimmerSkyRadiance(float3 direction)
{
    if (constants.dispatch.y == 0xFFFFu || constants.dispatch.y == 0xFFFFFFFFu)
    {
        return (float3)0.0;
    }

    float3 radiance = envProbesColorTexture.SampleLevel(glimmerMaterialSampler, float4(direction, float(constants.dispatch.y)), GLIMMER_SKY_MIP).rgb;
    radiance *= constants.sky.x * world_shader_data.sky_light_params.x;

    const float luminance = dot(radiance, float3(0.2126, 0.7152, 0.0722));
    const float maxLuminance = constants.volume.params.z;

    return luminance > maxLuminance ? radiance * (maxLuminance / luminance) : radiance;
}

#if defined(MODE_SHADE)

float GlimmerSunVisibility(float3 P, float3 N, float3 L, bool traceSWRT)
{
    const float3 origin = P + N * 0.05 + L * 0.02;

    // the heightfield's solids only skip what SWRT covered; a ray the footprint mask cleared keeps them
    float solidsFromT = 0.0;

    if (traceSWRT)
    {
        float tFirst;
        float tLast;
        float tCovered;

        if (GlimmerMaskTraceRay(constants.mask, GLIMMER_SUN_MASK_LEVEL, origin, L, GLIMMER_SUN_SWRT_DISTANCE, tFirst, tLast, tCovered))
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

    if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, L, constants.volume.params.w, 0u, solidsFromT, constants.sky.y, false, shadowHit))
    {
        return 0.0;
    }

    return shadowHit.transmittance;
}

// Outgoing radiance of a diffuse surface lit by the sun and by the previous frame's probes
float3 GlimmerShadeSurface(float3 P, float3 N, float3 albedo, bool traceSWRT)
{
    albedo = min(albedo, (float3)GLIMMER_MAX_ALBEDO);

    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);
    const float NdotL = dot(N, L);

    float3 direct = (float3)0.0;

    if (NdotL > 0.0 && L.y > -0.1)
    {
        const float3 sunIrradiance = world_shader_data.sun_color.rgb * world_shader_data.sun_direction_intensity.w * NdotL;

        direct = sunIrradiance * GlimmerInvPi * GlimmerSunVisibility(P, N, L, traceSWRT);
    }

    // multiple bounces come from last frame's probes, and past them (a ray goes far) the far field's SH voxels; where neither
    // reaches yet, the sky stands in
    const float4 nearField = SampleGlimmerProbes(constants.volume, P + N * 0.1, N);

    float4 farField = (float4)0.0;

    [branch]
    if (nearField.a < 0.999)
    {
        farField = EvaluateGlimmerSH(constants.sh, P + N * 0.05, N);
    }

    const float4 previous = GlimmerBlendFarField(nearField, farField);
    const float3 indirect = previous.rgb * previous.a + GlimmerSkyRadiance(N) * 0.5 * (1.0 - previous.a);

    return albedo * (direct + indirect);
}

#endif // MODE_SHADE

// a lane per ray, and as many probes as fill a wave of 32 (Must match GlimmerProbeRays and ProbesPerTraceGroup in GlimmerSWRTProbeVolume)
#define RAYS_PER_PROBE 16
#define PROBES_PER_GROUP 2

void GlimmerWriteRayHit(uint rayRecordIndex, float3 P, float3 N, float hitT, uint flags, float3 albedo, GlimmerHeightfieldHit heightfieldHit)
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
    // a row of groups per cascade
    const uint cascadeIndex = groupId.y;
    const uint4 cascadeDispatch = constants.cascadeDispatches[cascadeIndex];
    const uint2 slice = uint2(cascadeDispatch.y & 0xFFFFu, cascadeDispatch.y >> 16);

    const uint dispatchedIndex = groupId.x * PROBES_PER_GROUP + groupIndex / RAYS_PER_PROBE;

    if (dispatchedIndex >= cascadeDispatch.z)
    {
        return;
    }

    const uint probeIndex = GlimmerDispatchedProbe(dispatchedIndex, cascadeDispatch.x, slice);
    const uint rayLane = groupIndex % RAYS_PER_PROBE;

    // ray records of every cascade, one after another
    const uint probeRecord = cascadeIndex * GLIMMER_PROBES_PER_CASCADE + probeIndex;

#if defined(MODE_SHADE)
    const uint numRays = constants.volume.info.y;

    for (uint rayIndex = rayLane; rayIndex < numRays; rayIndex += RAYS_PER_PROBE)
    {
        const uint rayRecordIndex = probeRecord * numRays + rayIndex;
        const GlimmerProbeRayHit rayHit = RayHits[rayRecordIndex];

        const uint flags = asuint(rayHit.normalFlags.w);

        if (flags == GLIMMER_RAY_HIT_DONE)
        {
            continue;
        }

        const float3 radiance = GlimmerShadeSurface(rayHit.positionT.xyz, rayHit.normalFlags.xyz, rayHit.albedoTransmittance.rgb, flags == GLIMMER_RAY_HIT_SHADE_SWRT);

        OutRays[rayRecordIndex] = float4(rayHit.inscatter.rgb + rayHit.albedoTransmittance.a * radiance, rayHit.positionT.w);
    }
#else
    const GlimmerProbeCascade cascade = constants.volume.cascades[cascadeIndex];

    int2 localColumn;
    uint layer;
    GlimmerProbeFromIndex(probeIndex, localColumn, layer);

    const int2 column = cascade.gridOrigin.xy + localColumn;
    const float3 origin = GlimmerProbePosition(cascade, cascadeIndex, column, layer);

    const uint numRays = constants.volume.info.y;
    const uint numInstances = uint(constants.volume.nearField.z);

    // a cascade that scrolled needs the probes it gained, besides this frame's slice; the rest keep what they have
    if (cascadeDispatch.x == GLIMMER_PROBE_DISPATCH_SCROLLED && !GlimmerIsProbeInSlice(probeIndex, slice))
    {
        const uint2 state = glimmerProbeState.Load(int4(GlimmerProbeTexel(cascadeIndex, column, layer), 0));

        if (GlimmerIsSameColumn(state.x, column) && abs(asfloat(state.y) - origin.y) <= 0.25 * cascade.params.y)
        {
            for (uint rayIndex = rayLane; rayIndex < numRays; rayIndex += RAYS_PER_PROBE)
            {
                RayHits[probeRecord * numRays + rayIndex].normalFlags.w = asfloat(GLIMMER_RAY_HIT_DONE);
            }

            return;
        }
    }

    // whether hits get sun shadows from SWRT (the shade pass asks the footprint mask before tracing)
    const bool swrtEnabled = numInstances != 0u && float(cascadeIndex) < constants.volume.nearField.x;
    const float swrtReach = constants.volume.nearField.y * cascade.params.x;

    const float maxDistance = constants.volume.params.w;

    // a probe with nothing in the footprint mask within reach has nothing for SWRT to hit
    const float swrtRange = min(swrtReach, maxDistance);
    const bool traceSWRT = swrtEnabled && GlimmerMaskAnyInBox(constants.mask, origin.xz - swrtRange, origin.xz + swrtRange, origin.y - swrtRange, origin.y + swrtRange);

    const uint maskLevel = cascadeIndex + GLIMMER_MASK_RAY_LEVEL_BIAS;

    // coarse cascades march the heightfield with coarse steps
    const uint startLevel = uint(clamp(int(cascadeIndex) - 2, 0, GLIMMER_GROUND_LEVELS - 1));

    for (uint rayIndex = rayLane; rayIndex < numRays; rayIndex += RAYS_PER_PROBE)
    {
        const float3 direction = GlimmerProbeRayDirection(constants.volume, rayIndex);
        const uint rayRecordIndex = probeRecord * numRays + rayIndex;

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
        const bool traceRay = traceSWRT && GlimmerMaskTraceRay(constants.mask, maskLevel, origin, direction, swrtRange, swrtFirst, swrtLast, swrtCovered);

        if (traceRay)
        {
            GlimmerSWRTStats stats = GlimmerMakeSWRTStats();
            hitSWRT = TraceGlimmerSWRT(origin, direction, swrtFirst, swrtLast, numInstances, false, hit, stats);

            if (hitSWRT)
            {
                hitT = hit.t;
            }
        }

        // what SWRT covered, the heightfield's solid spans skip; a ray SWRT didn't trace keeps them, and the ground and the canopy
        // apply along the whole ray
        const float solidsFromT = traceRay ? swrtCovered : 0.0;

        GlimmerHeightfieldHit heightfieldHit;
        const bool hitHeightfield = GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, hitT, startLevel, solidsFromT, constants.sky.y, true, heightfieldHit);

        if (hitHeightfield)
        {
            hitT = heightfieldHit.t;

            const float3 P = origin + direction * hitT;

            const float3 albedo = heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND
                ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, heightfieldHit.level, (float3)constants.volume.nearField.w)
                : heightfieldHit.albedo;

            if (hitT > 0.0)
            {
                GlimmerWriteRayHit(rayRecordIndex, P + heightfieldHit.normal * 0.05, heightfieldHit.normal, hitT,
                    swrtEnabled ? GLIMMER_RAY_HIT_SHADE_SWRT : GLIMMER_RAY_HIT_SHADE, albedo, heightfieldHit);

                isDone = false;
            }
        }
        else if (hitSWRT)
        {
            const GlimmerInstance instance = glimmerInstances[hit.instanceIndex];

            isBackface = !hit.frontFace && (instance.data.w & GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED) == 0u;

            if (isBackface)
            {
                // the inside of a closed mesh; nothing should light the probe from here
                radiance = (float3)0.0;
            }
            else
            {
                const float3 P = origin + direction * hitT;
                const float3 N = GlimmerGetHitNormal(hit, direction);

                GlimmerWriteRayHit(rayRecordIndex, P, N, hitT, GLIMMER_RAY_HIT_SHADE_SWRT, GlimmerGetMaterialAverageAlbedo(instance.data.z), heightfieldHit);

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

            OutRays[rayRecordIndex] = float4(heightfieldHit.inscatter + heightfieldHit.transmittance * radiance, isBackface ? -hitT : hitT);
        }
    }
#endif
}
