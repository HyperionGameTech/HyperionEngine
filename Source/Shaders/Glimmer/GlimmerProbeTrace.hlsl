#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"
#include "../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Material.hlsli"
#include "../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../Include/RayTracing/BVH.hlsli"
#include "GlimmerCommon.hlsli"
#include "GlimmerProbeTypes.hlsli"

// Must match GlimmerProbeTraceConstants in GlimmerProbeVolume.cpp
struct GlimmerProbeTraceConstants
{
    GlimmerProbeVolume volume;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    uint4 dispatch; // x = cascade, y = sky probe color texture index (~0 without one)
    float4 sky;     // x = sky probe diffuse strength, y = foliage extinction
};

DECLARE_BUFFER_DYNAMIC(GlimmerProbeTrace, CBuffer) cbuffer CBuffer
{
    GlimmerProbeTraceConstants constants;
};

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
DECLARE_SRV(GlimmerProbeTrace, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;

DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeSH0Texture) Texture3D<float4> glimmerProbeSH0;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeSH1Texture) Texture3D<float4> glimmerProbeSH1;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeSH2Texture) Texture3D<float4> glimmerProbeSH2;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeStateTexture) Texture3D<uint2> glimmerProbeState;
DECLARE_SRV(GlimmerProbeTrace, GlimmerProbeBaseTexture) Texture2DArray<float> glimmerProbeBase;

DECLARE_SRV(GlimmerProbeTrace, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_UAV(GlimmerProbeTrace, OutRays) RWStructuredBuffer<float4> OutRays;

#include "GlimmerProbes.hlsli"
#include "GlimmerSWRT.hlsli"
#include "GlimmerMaterial.hlsli"

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

#include "GlimmerHeightfield.hlsli"

// sky probes are filtered cubemaps; this mip keeps the sun's forward scattering from spiking single rays
#define GLIMMER_SKY_MIP 3.0

#define GLIMMER_MAX_ALBEDO 0.9

// SWRT shadow rays toward the sun only need to reach past nearby occluders; the heightfield handles the terrain beyond
#define GLIMMER_SUN_SWRT_DISTANCE 256.0

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

float GlimmerSunVisibility(float3 P, float3 N, float3 L, bool traceSWRT)
{
    const float3 origin = P + N * 0.05 + L * 0.02;

    if (traceSWRT)
    {
        GlimmerSWRTStats stats = GlimmerMakeSWRTStats();
        GlimmerSWRTHit shadowHit;

        if (TraceGlimmerSWRT(origin, L, 0.0, GLIMMER_SUN_SWRT_DISTANCE, uint(constants.volume.nearField.z), true, shadowHit, stats))
        {
            return 0.0;
        }
    }

    GlimmerHeightfieldHit shadowHit;

    if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, L, constants.volume.params.w, 0u, traceSWRT ? GLIMMER_SUN_SWRT_DISTANCE : 0.0, constants.sky.y, false, shadowHit))
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

    // multiple bounces come from last frame's probes; where they don't reach yet, the sky stands in
    const float4 previous = SampleGlimmerProbes(constants.volume, P + N * 0.1, N);
    const float3 indirect = previous.rgb * previous.a + GlimmerSkyRadiance(N) * 0.5 * (1.0 - previous.a);

    return albedo * (direct + indirect);
}

#define RAYS_PER_GROUP 32

[numthreads(RAYS_PER_GROUP, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint cascadeIndex = constants.dispatch.x;
    const GlimmerProbeCascade cascade = constants.volume.cascades[cascadeIndex];

    const uint probeIndex = groupId.x;

    int2 localColumn;
    uint layer;
    GlimmerProbeFromIndex(probeIndex, localColumn, layer);

    const int2 column = cascade.gridOrigin.xy + localColumn;
    const float3 origin = GlimmerProbePosition(cascade, cascadeIndex, column, layer);

    const uint numRays = constants.volume.info.y;
    const uint numInstances = uint(constants.volume.nearField.z);

    const bool traceSWRT = numInstances != 0u && float(cascadeIndex) < constants.volume.nearField.x;
    const float swrtReach = constants.volume.nearField.y * cascade.params.x;

    const float maxDistance = constants.volume.params.w;

    // coarse cascades march the heightfield with coarse steps
    const uint startLevel = uint(clamp(int(cascadeIndex) - 2, 0, GLIMMER_GROUND_LEVELS - 1));

    for (uint rayIndex = groupIndex; rayIndex < numRays; rayIndex += RAYS_PER_GROUP)
    {
        const float3 direction = GlimmerProbeRayDirection(constants.volume, rayIndex);

        float hitT = maxDistance;
        float3 radiance;

        GlimmerSWRTHit hit;
        bool hitSWRT = false;

        if (traceSWRT)
        {
            GlimmerSWRTStats stats = GlimmerMakeSWRTStats();
            hitSWRT = TraceGlimmerSWRT(origin, direction, 0.0, min(swrtReach, maxDistance), numInstances, false, hit, stats);

            if (hitSWRT)
            {
                hitT = hit.t;
            }
        }

        // what SWRT traced, the heightfield's solid spans skip; the ground and the canopy apply along the whole ray
        const float solidsFromT = traceSWRT ? min(swrtReach, maxDistance) : 0.0;

        GlimmerHeightfieldHit heightfieldHit;
        const bool hitHeightfield = GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, hitT, startLevel, solidsFromT, constants.sky.y, true, heightfieldHit);

        if (hitHeightfield)
        {
            hitT = heightfieldHit.t;

            const float3 P = origin + direction * hitT;

            const float3 albedo = heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND
                ? (float3)constants.volume.nearField.w
                : heightfieldHit.albedo;

            radiance = hitT <= 0.0 ? (float3)0.0 : GlimmerShadeSurface(P + heightfieldHit.normal * 0.05, heightfieldHit.normal, albedo, traceSWRT);
        }
        else if (hitSWRT)
        {
            const GlimmerInstance instance = glimmerInstances[hit.instanceIndex];

            const bool isBackface = !hit.frontFace && (instance.data.w & GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED) == 0u;

            if (isBackface)
            {
                // the inside of a closed mesh; nothing should light the probe from here
                radiance = (float3)0.0;
            }
            else
            {
                const float3 P = origin + direction * hitT;
                const float3 N = GlimmerGetHitNormal(hit, direction);

                radiance = GlimmerShadeSurface(P, N, GlimmerGetMaterialAverageAlbedo(instance.data.z), true);
            }
        }
        else
        {
            radiance = GlimmerSkyRadiance(direction);
        }

        radiance = heightfieldHit.inscatter + heightfieldHit.transmittance * radiance;

        OutRays[probeIndex * numRays + rayIndex] = float4(radiance, hitT);
    }
}
