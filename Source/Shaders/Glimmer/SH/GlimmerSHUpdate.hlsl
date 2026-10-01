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

DECLARE_UAV(GlimmerSHUpdate, OutData) RWTexture3D<float4> OutData;
DECLARE_UAV(GlimmerSHUpdate, OutState) RWTexture3D<uint2> OutState;

DECLARE_SRV(GlimmerSHUpdate, GlimmerSHOccupancyTexture) Texture3D<float4> glimmerSHOccupancy;

float3 GlimmerCanopyRadiance(float3 P, float3 albedo, float depthBelowTop, float extinction)
{
    return albedo;
}

#include "../GlimmerHeightfield.hlsli"

#include "GlimmerSHOccupancy.hlsli"

#define GLIMMER_SH_HIT_OCCUPANCY 3u

#define GLIMMER_SH_RAYS 64
#define GLIMMER_SH_MAX_ALBEDO 0.9

#define GLIMMER_SH_SUN_DISTANCE 256.0

#define GLIMMER_SH_HYSTERESIS 0.5

#define GLIMMER_SH_VISIBILITY_SHARPNESS 6.0

groupshared float4 gsVisibility[GLIMMER_SH_RAYS];       // visible, visible * direction
groupshared float4 gsBlockedAlbedo[GLIMMER_SH_RAYS];    // rgb = albedo weighted by how much it blocks and how much sky it faces, a = how much it blocks
groupshared float gsSunlit[GLIMMER_SH_RAYS];            // blocked albedo luminance the sun lights
groupshared float4 gsDepth[GLIMMER_SH_RAYS];            // xyz = direction, w = distance to a solid or the ground in voxels

float GlimmerLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

bool GlimmerSHTraceScene(float3 origin, float3 direction, float tMax, uint startLevel, bool accumulateCanopy, out GlimmerHeightfieldHit hit)
{
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

        return true;
    }

    return false;
}

float GlimmerSHSunVisibility(float3 P, float3 N, float3 L, uint level)
{
    GlimmerHeightfieldHit shadowHit;

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

    if (GlimmerSHTraceScene(P + (N * 0.5 + L * 2.0) * spacing, L, GLIMMER_SH_SUN_DISTANCE, level, false, shadowHit))
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
        && P.y >= spanSample.solidMin && P.y <= spanSample.solidMax
        && GlimmerSpanSolidFill(spanSample, constants.spans.levels[0].params.x) >= GLIMMER_SPAN_SOLID_THRESHOLD;
}

float4 GlimmerSHBlockerLighting(float3 P, float3 N)
{
    const float4 openGround = float4((float3)saturate(0.5 + 0.5 * N.y), 0.0);

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        const GlimmerSHCascade cascade = constants.volume.cascades[cascadeIndex];

        const int3 voxel = int3(floor((P + N * (0.5 * cascade.params.x)) * cascade.params.y));
        const int3 local = voxel - cascade.origin.xyz;

        if (cascade.origin.w == 0 || any(local < 0) || any(local >= int3(GLIMMER_SH_GRID_XZ, GLIMMER_SH_GRID_Y, GLIMMER_SH_GRID_XZ)))
        {
            continue;
        }

        const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);
        const float4 bounce = OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE)];

        if (!GlimmerSHStateMatches(OutState[texel], voxel) || bounce.a < 0.0)
        {
            return openGround;
        }

        const float4 visibility = OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY)];
        const float skySeen = saturate(visibility.x + dot(visibility.yzw, N));

        return float4(min(skySeen + (1.0 - skySeen) * bounce.rgb, (float3)1.0), (1.0 - skySeen) * bounce.a);
    }

    return openGround;
}

[numthreads(GLIMMER_SH_RAYS, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint cascadeIndex = uint(constants.boxMin.w);
    const GlimmerSHCascade cascade = constants.volume.cascades[cascadeIndex];

    const int3 voxel = constants.boxMin.xyz + int3(groupId);
    const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);
    const float3 center = GlimmerSHVoxelCenter(cascade, voxel);

    float3 originOffset = (float3)0.0;
    bool isBuried = GlimmerSHIsBuried(center);

    if (isBuried)
    {
        float3 freeSum = (float3)0.0;
        float freeCount = 0.0;
        float3 firstFree = (float3)0.0;

        [unroll]
        for (uint octant = 0; octant < 8; octant++)
        {
            const float3 octantOffset = float3(octant & 1u, (octant >> 1) & 1u, (octant >> 2) & 1u) * 0.5 - 0.25;

            if (!GlimmerSHIsBuried(center + octantOffset * cascade.params.x))
            {
                firstFree = freeCount == 0.0 ? octantOffset : firstFree;
                freeSum += octantOffset;
                freeCount += 1.0;
            }
        }

        if (freeCount > 0.0)
        {
            originOffset = freeSum / freeCount;

            if (GlimmerSHIsBuried(center + originOffset * cascade.params.x))
            {
                originOffset = firstFree;
            }

            isBuried = false;
        }
    }

    const float3 origin = center + originOffset * cascade.params.x;

    if (isBuried)
    {
        for (uint pair = groupIndex; pair < GLIMMER_SH_VISIBILITY_TEXELS / 2u; pair += GLIMMER_SH_RAYS)
        {
            OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + pair)] = (float4)0.0;
        }

        if (groupIndex == 0u)
        {
            OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY)] = (float4)0.0;
            OutData[GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE)] = float4(0.0, 0.0, 0.0, -1.0);
            OutState[texel] = GlimmerSHPackState(voxel, (float3)0.0);
        }

        return;
    }

    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);
    const float3 direction = normalize(GlimmerRotateByQuaternion(constants.rayRotation, GlimmerSphericalFibonacci(groupIndex, GLIMMER_SH_RAYS)));

    const float finestTexel = constants.ground.levels[0].params.x;
    const uint startLevel = uint(clamp(int(round(log2(max(cascade.params.x / finestTexel, 1.0)))), 0, GLIMMER_GROUND_LEVELS - 1));

    GlimmerHeightfieldHit hit;
    const bool didHit = GlimmerSHTraceScene(origin, direction, constants.params.y, startLevel, true, hit);

    const float visible = didHit ? 0.0 : hit.transmittance;

    const float3 leafAlbedo = min(hit.inscatter, (float3)GLIMMER_SH_MAX_ALBEDO);

    float3 skyLitAlbedo = 0.5 * leafAlbedo;
    float sunlit = L.y > 0.0 ? 0.5 * GlimmerLuminance(leafAlbedo) : 0.0;

    if (didHit)
    {
        const float3 P = origin + direction * hit.t;

        const float3 albedo = min(hit.kind == GLIMMER_HEIGHTFIELD_GROUND
            ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, hit.level, (float3)constants.params.z)
            : hit.albedo, (float3)GLIMMER_SH_MAX_ALBEDO);

        const float4 indirect = GlimmerSHBlockerLighting(P, hit.normal);

        skyLitAlbedo += hit.transmittance * albedo * indirect.rgb;

        float sunLighting = indirect.a;

        const float NdotL = dot(hit.normal, L);

        if (NdotL > 0.0 && L.y > 0.0)
        {
            sunLighting += NdotL * GlimmerSHSunVisibility(P, hit.normal, L, hit.level);
        }

        sunlit += hit.transmittance * GlimmerLuminance(albedo) * sunLighting;
    }

    gsVisibility[groupIndex] = float4(visible, visible * direction);
    gsBlockedAlbedo[groupIndex] = float4(skyLitAlbedo, 1.0 - visible);
    gsSunlit[groupIndex] = sunlit;

    // leaves don't count.
    // light through a canopy isn't a leak
    const float depth = select(didHit, min(hit.t * cascade.params.y, GLIMMER_SH_DEPTH_RANGE), GLIMMER_SH_DEPTH_RANGE);
    gsDepth[groupIndex] = float4(direction, depth);

    GroupMemoryBarrierWithGroupSync();

    // every thread reads the history here, before thread 0 overwrites the state and bounce at the end
    const uint2 state = GlimmerSHPackState(voxel, originOffset);
    const uint3 bounceTexel = GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE);
    const float4 previousBounce = OutData[bounceTexel];

    const bool hasHistory = GlimmerSHStateMatches(OutState[texel], voxel) && previousBounce.a >= 0.0;

    // each depth map texel takes the rays around its direction, two texels (xy, zw) a thread
    for (uint pair = groupIndex; pair < GLIMMER_SH_VISIBILITY_TEXELS / 2u; pair += GLIMMER_SH_RAYS)
    {
        const uint3 depthTexel = GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + pair);
        const float4 previousPair = hasHistory
            ? OutData[depthTexel]
            : float4(GLIMMER_SH_DEPTH_RANGE, GLIMMER_SH_DEPTH_RANGE * GLIMMER_SH_DEPTH_RANGE, GLIMMER_SH_DEPTH_RANGE, GLIMMER_SH_DEPTH_RANGE * GLIMMER_SH_DEPTH_RANGE);

        float4 depthPair;

        [unroll]
        for (uint side = 0; side < 2; side++)
        {
            const uint mapIndex = pair * 2u + side;
            const float2 mapUV = (float2(mapIndex % GLIMMER_SH_VISIBILITY_RES, mapIndex / GLIMMER_SH_VISIBILITY_RES) + 0.5) / float(GLIMMER_SH_VISIBILITY_RES);
            const float3 mapDirection = GlimmerOctahedralDecode(mapUV);

            float3 sums = (float3)0.0; // weighted depth, weighted depth squared, weight

            for (uint rayIndex = 0; rayIndex < GLIMMER_SH_RAYS; rayIndex++)
            {
                const float4 ray = gsDepth[rayIndex];
                const float weight = pow(saturate(dot(mapDirection, ray.xyz)), GLIMMER_SH_VISIBILITY_SHARPNESS);

                sums += float3(ray.w, ray.w * ray.w, 1.0) * weight;
            }

            const float2 previous = side == 0u ? previousPair.xy : previousPair.zw;
            const float2 estimate = sums.z > 1e-4 ? sums.xy / sums.z : previous;
            const float2 moments = lerp(estimate, previous, lerp(1.0, hasHistory ? GLIMMER_SH_HYSTERESIS : 0.0, saturate(sums.z)));

            if (side == 0u)
            {
                depthPair.xy = moments;
            }
            else
            {
                depthPair.zw = moments;
            }
        }

        OutData[depthTexel] = depthPair;
    }

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

    /// rgb: blocker albedo as the sky lights it
    /// a: the luminance of what the sun lights of them, both per unit of blocked
    const float4 blocked = gsBlockedAlbedo[0];

    float4 bounce = select(blocked.a > 1e-3, float4(blocked.rgb, gsSunlit[0]) / blocked.a, (float4)0.0);

    const uint3 visibilityTexel = GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY);

    if (hasHistory)
    {
        visibility = lerp(visibility, OutData[visibilityTexel], GLIMMER_SH_HYSTERESIS);
        bounce = lerp(bounce, previousBounce, GLIMMER_SH_HYSTERESIS);
    }

    OutData[visibilityTexel] = visibility;
    OutData[bounceTexel] = bounce;
    OutState[texel] = state;
}
