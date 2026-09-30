#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../GlimmerCommon.hlsli"
#include "GlimmerSHCommon.hlsli"

// Must match GlimmerSHUpdateConstants in GlimmerSHVolume.cpp
struct GlimmerSHUpdateConstants
{
    GlimmerSHVolume volume;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    int4 boxMin;        // xyz = absolute voxel of the dispatch's first voxel, w = cascade
    float4 rayRotation; // quaternion applied to this update's ray directions
    float4 params;      // x = foliage extinction, y = max ray distance, z = albedo where the ground's isn't known
};

DECLARE_BUFFER_DYNAMIC(GlimmerSHUpdate, CBuffer) cbuffer CBuffer
{
    GlimmerSHUpdateConstants constants;
};

DECLARE_SRV(GlimmerSHUpdate, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(GlimmerSHUpdate, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerSHUpdate, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerSHUpdate, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;

DECLARE_UAV(GlimmerSHUpdate, OutVisibility) RWTexture3D<float4> OutVisibility;
DECLARE_UAV(GlimmerSHUpdate, OutBounce) RWTexture3D<float4> OutBounce;
DECLARE_UAV(GlimmerSHUpdate, OutState) RWTexture3D<uint2> OutState;
DECLARE_UAV(GlimmerSHUpdate, OutDepthX) RWTexture3D<float4> OutDepthX;
DECLARE_UAV(GlimmerSHUpdate, OutDepthY) RWTexture3D<float4> OutDepthY;
DECLARE_UAV(GlimmerSHUpdate, OutDepthZ) RWTexture3D<float4> OutDepthZ;

// leaves a ray passes through count toward the bounce as their albedo; the march weighs it by how much they block
float3 GlimmerCanopyRadiance(float3 P, float3 albedo, float depthBelowTop, float extinction)
{
    return albedo;
}

#include "../GlimmerHeightfield.hlsli"

#define GLIMMER_SH_RAYS 64
#define GLIMMER_SH_MAX_ALBEDO 0.9

// sun shadow rays only need to get out from under the nearby canopy and terrain
#define GLIMMER_SH_SUN_DISTANCE 256.0

// how much of the previous trace a re-traced voxel keeps; each trace uses new ray directions, so this smooths the noise
#define GLIMMER_SH_HYSTERESIS 0.5

groupshared float4 gsVisibility[GLIMMER_SH_RAYS]; // visible, visible * direction
groupshared float4 gsBlockedAlbedo[GLIMMER_SH_RAYS]; // rgb = albedo weighted by how much it blocks, a = how much it blocks
groupshared float gsSunlit[GLIMMER_SH_RAYS];         // blocked albedo luminance the sun lights
groupshared float2 gsDepth[GLIMMER_SH_RAYS];         // distance to a solid or the ground in voxels, direction bin

float GlimmerLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float GlimmerSHSunVisibility(float3 P, float3 N, float3 L, uint level)
{
    GlimmerHeightfieldHit shadowHit;

    if (GlimmerTraceHeightfield(constants.ground, constants.spans, P + N * 0.1 + L * 0.05, L, GLIMMER_SH_SUN_DISTANCE, level, 0.0, constants.params.x, false, shadowHit))
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

    GlimmerSpanSample spanSample;

    return GlimmerSampleSpans(constants.spans, 0u, P.xz, spanSample)
        && P.y >= spanSample.solidMin && P.y <= spanSample.solidMax
        && GlimmerSpanSolidFill(spanSample, constants.spans.levels[0].params.x) >= GLIMMER_SPAN_SOLID_THRESHOLD;
}

// One group per voxel, one thread per ray
[numthreads(GLIMMER_SH_RAYS, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint cascadeIndex = uint(constants.boxMin.w);
    const GlimmerSHCascade cascade = constants.volume.cascades[cascadeIndex];

    const int3 voxel = constants.boxMin.xyz + int3(groupId);
    const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);
    const float3 origin = GlimmerSHVoxelCenter(cascade, voxel);

    // lighting skips voxels inside the ground or a solid; this is uniform across the group
    if (GlimmerSHIsBuried(origin))
    {
        if (groupIndex == 0u)
        {
            OutVisibility[texel] = (float4)0.0;
            OutBounce[texel] = float4(0.0, 0.0, 0.0, -1.0);
            OutState[texel] = GlimmerSHPackVoxel(voxel);
            OutDepthX[texel] = (float4)0.0;
            OutDepthY[texel] = (float4)0.0;
            OutDepthZ[texel] = (float4)0.0;
        }

        return;
    }

    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);
    const float3 direction = normalize(GlimmerRotateByQuaternion(constants.rayRotation, GlimmerSphericalFibonacci(groupIndex, GLIMMER_SH_RAYS)));

    // coarse cascades march the heightfield with coarse steps
    const uint startLevel = min(cascadeIndex, uint(GLIMMER_GROUND_LEVELS - 1));

    GlimmerHeightfieldHit hit;
    const bool didHit = GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, constants.params.y, startLevel, 0.0, constants.params.x, true, hit);

    const float visible = didHit ? 0.0 : hit.transmittance;

    // leaves along the way, then whatever the ray ends on
    float3 blockedAlbedo = min(hit.inscatter, (float3)GLIMMER_SH_MAX_ALBEDO);
    float sunlit = L.y > 0.0 ? 0.5 * GlimmerLuminance(blockedAlbedo) : 0.0;

    if (didHit)
    {
        const float3 P = origin + direction * hit.t;

        const float3 albedo = min(hit.kind == GLIMMER_HEIGHTFIELD_GROUND
            ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, hit.level, (float3)constants.params.z)
            : hit.albedo, (float3)GLIMMER_SH_MAX_ALBEDO);

        blockedAlbedo += hit.transmittance * albedo;

        const float NdotL = dot(hit.normal, L);

        if (NdotL > 0.0 && L.y > 0.0)
        {
            sunlit += hit.transmittance * GlimmerLuminance(albedo) * NdotL * GlimmerSHSunVisibility(P, hit.normal, L, hit.level);
        }
    }

    gsVisibility[groupIndex] = float4(visible, visible * direction);
    gsBlockedAlbedo[groupIndex] = float4(blockedAlbedo, 1.0 - visible);
    gsSunlit[groupIndex] = sunlit;

    // leaves don't count: light through a canopy isn't a leak
    const float depth = didHit ? min(hit.t * cascade.params.y, GLIMMER_SH_DEPTH_RANGE) : GLIMMER_SH_DEPTH_RANGE;
    gsDepth[groupIndex] = float2(depth, float(GlimmerSHDirectionBin(direction)));

    GroupMemoryBarrierWithGroupSync();

    // gsDepth stays as written; thread 0 bins it after the reductions
    [unroll]
    for (uint stride = GLIMMER_SH_RAYS / 2; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
        {
            gsVisibility[groupIndex] += gsVisibility[groupIndex + stride];
            gsBlockedAlbedo[groupIndex] += gsBlockedAlbedo[groupIndex + stride];
            gsSunlit[groupIndex] += gsSunlit[groupIndex + stride];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex != 0u)
    {
        return;
    }

    // with N evenly spread rays, the cosine convolved L1 is e0 = mean, e1 = 2 * mean(value * direction)
    const float invNumRays = 1.0 / float(GLIMMER_SH_RAYS);
    float4 visibility = float4(gsVisibility[0].x * invNumRays, gsVisibility[0].yzw * (2.0 * invNumRays));

    const float4 blocked = gsBlockedAlbedo[0];
    const float blockedLuminance = GlimmerLuminance(blocked.rgb);

    float4 bounce = float4(
        blocked.a > 1e-3 ? blocked.rgb / blocked.a : (float3)0.0,
        blockedLuminance > 1e-4 ? saturate(gsSunlit[0] / blockedLuminance) : 0.0);

    float2 depthSums[6] = { (float2)0.0, (float2)0.0, (float2)0.0, (float2)0.0, (float2)0.0, (float2)0.0 };
    float depthCounts[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

    for (uint rayIndex = 0; rayIndex < GLIMMER_SH_RAYS; rayIndex++)
    {
        const float2 rayDepth = gsDepth[rayIndex];
        const uint bin = uint(rayDepth.y);

        depthSums[bin] += float2(rayDepth.x, rayDepth.x * rayDepth.x);
        depthCounts[bin] += 1.0;
    }

    float2 depthMoments[6];

    [unroll]
    for (uint bin = 0; bin < 6; bin++)
    {
        // a direction no ray went in this time reads as open
        depthMoments[bin] = depthCounts[bin] > 0.0
            ? depthSums[bin] / depthCounts[bin]
            : float2(GLIMMER_SH_DEPTH_RANGE, GLIMMER_SH_DEPTH_RANGE * GLIMMER_SH_DEPTH_RANGE);
    }

    float4 depthX = float4(depthMoments[0], depthMoments[1]);
    float4 depthY = float4(depthMoments[2], depthMoments[3]);
    float4 depthZ = float4(depthMoments[4], depthMoments[5]);

    const uint2 state = GlimmerSHPackVoxel(voxel);
    const float4 previousBounce = OutBounce[texel];

    if (all(OutState[texel] == state) && previousBounce.a >= 0.0)
    {
        visibility = lerp(visibility, OutVisibility[texel], GLIMMER_SH_HYSTERESIS);
        bounce = lerp(bounce, previousBounce, GLIMMER_SH_HYSTERESIS);
        depthX = lerp(depthX, OutDepthX[texel], GLIMMER_SH_HYSTERESIS);
        depthY = lerp(depthY, OutDepthY[texel], GLIMMER_SH_HYSTERESIS);
        depthZ = lerp(depthZ, OutDepthZ[texel], GLIMMER_SH_HYSTERESIS);
    }

    OutVisibility[texel] = visibility;
    OutBounce[texel] = bounce;
    OutState[texel] = state;
    OutDepthX[texel] = depthX;
    OutDepthY[texel] = depthY;
    OutDepthZ[texel] = depthZ;
}
