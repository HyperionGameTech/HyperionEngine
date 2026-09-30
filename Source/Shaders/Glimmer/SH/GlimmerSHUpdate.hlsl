#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../GlimmerCommon.hlsli"
#include "GlimmerSHCommon.hlsli"

#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "GlimmerSHOccupancy.hlsli"
#undef GLIMMER_SH_OCCUPANCY_NO_TRACE

// Must match GlimmerSHUpdateConstants in GlimmerSHVolume.cpp
struct GlimmerSHUpdateConstants
{
    GlimmerSHVolume volume;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    GlimmerSHOccupancyParams occupancy;
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

DECLARE_UAV(GlimmerSHUpdate, OutData) RWTexture3D<float4> OutData; // GLIMMER_SH_SLABS slabs (GlimmerSHSlabTexel)
DECLARE_UAV(GlimmerSHUpdate, OutState) RWTexture3D<uint2> OutState;

DECLARE_SRV(GlimmerSHUpdate, GlimmerSHOccupancyTexture) Texture3D<float4> glimmerSHOccupancy;

// leaves a ray passes through count toward the bounce as their albedo; the march weighs it by how much they block
float3 GlimmerCanopyRadiance(float3 P, float3 albedo, float depthBelowTop, float extinction)
{
    return albedo;
}

#include "../GlimmerHeightfield.hlsli"

// now that glimmerSHOccupancy is declared, the tracing half
#include "GlimmerSHOccupancy.hlsli"

// a hit on the occupancy clipmap, as opposed to the heightfield's ground or solid spans
#define GLIMMER_SH_HIT_OCCUPANCY 3u

#define GLIMMER_SH_RAYS 64
#define GLIMMER_SH_MAX_ALBEDO 0.9

// blockers barely facing the sky can still catch low sun; this bounds the ratio stored for them
#define GLIMMER_SH_MAX_SUN_RATIO 16.0

// sun shadow rays only need to get out from under the nearby canopy and terrain
#define GLIMMER_SH_SUN_DISTANCE 256.0

// how much of the previous trace a re-traced voxel keeps; each trace uses new ray directions, so this smooths the noise
#define GLIMMER_SH_HYSTERESIS 0.5

groupshared float4 gsVisibility[GLIMMER_SH_RAYS]; // visible, visible * direction
groupshared float4 gsBlockedAlbedo[GLIMMER_SH_RAYS]; // rgb = albedo weighted by how much it blocks and how much sky it faces, a = how much it blocks
groupshared float gsSunlit[GLIMMER_SH_RAYS];         // blocked albedo luminance the sun lights
groupshared float2 gsDepth[GLIMMER_SH_RAYS];         // distance to a solid or the ground in voxels, direction bin

float GlimmerLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

/*! Solids from the occupancy clipmap where it covers the ray, then the heightfield: the ground and canopy all along, and the solid
 *  spans past what the clipmap covered (they can't tell an arch from a pillar). hit.kind is GLIMMER_SH_HIT_OCCUPANCY for clipmap hits. */
bool GlimmerSHTraceScene(float3 origin, float3 direction, float tMax, uint startLevel, bool accumulateCanopy, out GlimmerHeightfieldHit hit)
{
    GlimmerSHOccupancyHit occupancyHit;
    float coveredT;

    const bool hitOccupancy = GlimmerSHTraceOccupancy(constants.occupancy, origin, direction, tMax, occupancyHit, coveredT);

    if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, hitOccupancy ? occupancyHit.t : tMax, startLevel, coveredT, constants.params.x, accumulateCanopy, hit))
    {
        return true;
    }

    // the heightfield also gives up (transmittance 0) once the canopy has taken nearly all the light
    if (hitOccupancy && hit.transmittance > 0.0)
    {
        hit.t = occupancyHit.t;
        hit.kind = GLIMMER_SH_HIT_OCCUPANCY;
        hit.normal = occupancyHit.normal;
        hit.albedo = occupancyHit.albedo;

        return true;
    }

    return false;
}

float GlimmerSHSunVisibility(float3 P, float3 N, float3 L, uint level)
{
    GlimmerHeightfieldHit shadowHit;

    // far enough off the surface to start in the empty occupancy voxel the hit ray came through
    if (GlimmerSHTraceScene(P + N * 0.3 + L * 0.05, L, GLIMMER_SH_SUN_DISTANCE, level, false, shadowHit))
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

    // where the occupancy clipmap covers P it knows; the solid spans would bury everything under an overhang
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
            OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY)] = (float4)0.0;
            OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE)] = float4(0.0, 0.0, 0.0, -1.0);
            OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + 0u)] = (float4)0.0;
            OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + 1u)] = (float4)0.0;
            OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + 2u)] = (float4)0.0;
            OutState[texel] = GlimmerSHPackVoxel(voxel);
        }

        return;
    }

    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);
    const float3 direction = normalize(GlimmerRotateByQuaternion(constants.rayRotation, GlimmerSphericalFibonacci(groupIndex, GLIMMER_SH_RAYS)));

    // the heightfield starts at the ground level whose texels are about a voxel wide, and moves to coarser ones as the ray goes on
    const float finestTexel = constants.ground.levels[0].params.x;
    const uint startLevel = uint(clamp(int(round(log2(max(cascade.params.x / finestTexel, 1.0)))), 0, GLIMMER_GROUND_LEVELS - 1));

    GlimmerHeightfieldHit hit;
    const bool didHit = GlimmerSHTraceScene(origin, direction, constants.params.y, startLevel, true, hit);

    const float visible = didHit ? 0.0 : hit.transmittance;

    // leaves along the way (lit from all around, so by about half the sky and, when it's up, half the sun), then whatever the ray ends on
    const float3 leafAlbedo = min(hit.inscatter, (float3)GLIMMER_SH_MAX_ALBEDO);

    float3 skyLitAlbedo = 0.5 * leafAlbedo;
    float sunlit = L.y > 0.0 ? 0.5 * GlimmerLuminance(leafAlbedo) : 0.0;

    if (didHit)
    {
        const float3 P = origin + direction * hit.t;

        const float3 albedo = min(hit.kind == GLIMMER_HEIGHTFIELD_GROUND
            ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, hit.level, (float3)constants.params.z)
            : hit.albedo, (float3)GLIMMER_SH_MAX_ALBEDO);

        // the fraction of the sky a surface facing this way sees on open ground; undersides see none
        const float skyFacing = saturate(0.5 + 0.5 * hit.normal.y);

        skyLitAlbedo += hit.transmittance * albedo * skyFacing;

        const float NdotL = dot(hit.normal, L);

        if (NdotL > 0.0 && L.y > 0.0)
        {
            sunlit += hit.transmittance * GlimmerLuminance(albedo) * NdotL * GlimmerSHSunVisibility(P, hit.normal, L, hit.level);
        }
    }

    gsVisibility[groupIndex] = float4(visible, visible * direction);
    gsBlockedAlbedo[groupIndex] = float4(skyLitAlbedo, 1.0 - visible);
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

    // rgb: blocker albedo as the sky lights it, per unit of blocked; a: the sun's share relative to that (luminance), so
    // lighting can relight them as rgb * (sky + a * sun)
    const float4 blocked = gsBlockedAlbedo[0];
    const float skyLitLuminance = GlimmerLuminance(blocked.rgb);

    float4 bounce = float4(
        blocked.a > 1e-3 ? blocked.rgb / blocked.a : (float3)0.0,
        skyLitLuminance > 1e-4 ? min(gsSunlit[0] / skyLitLuminance, GLIMMER_SH_MAX_SUN_RATIO) : 0.0);

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

    float4 depths[3] = {
        float4(depthMoments[0], depthMoments[1]),
        float4(depthMoments[2], depthMoments[3]),
        float4(depthMoments[4], depthMoments[5])
    };

    const uint3 visibilityTexel = GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY);
    const uint3 bounceTexel = GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE);

    const uint2 state = GlimmerSHPackVoxel(voxel);
    const float4 previousBounce = OutData[bounceTexel];

    const bool hasHistory = all(OutState[texel] == state) && previousBounce.a >= 0.0;

    if (hasHistory)
    {
        visibility = lerp(visibility, OutData[visibilityTexel], GLIMMER_SH_HYSTERESIS);
        bounce = lerp(bounce, previousBounce, GLIMMER_SH_HYSTERESIS);
    }

    OutData[visibilityTexel] = visibility;
    OutData[bounceTexel] = bounce;

    [unroll]
    for (uint axis = 0; axis < 3; axis++)
    {
        const uint3 depthTexel = GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + axis);

        OutData[depthTexel] = hasHistory ? lerp(depths[axis], OutData[depthTexel], GLIMMER_SH_HYSTERESIS) : depths[axis];
    }

    OutState[texel] = state;
}
