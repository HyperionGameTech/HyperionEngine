#ifndef GLIMMER_SH_APPLY_HLSLI
#define GLIMMER_SH_APPLY_HLSLI

#include "GlimmerSHCommon.hlsli"

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_SH_APPLY_SAMPLING_HLSLI)
#define GLIMMER_SH_APPLY_SAMPLING_HLSLI

#ifndef GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
DECLARE_SRV(DeferredPass, GlimmerSHDataTexture) Texture3D<float4> glimmerSHData;
DECLARE_SRV(DeferredPass, GlimmerSHStateTexture) Texture3D<uint2> glimmerSHState;
#endif

#define GLIMMER_SH_MAX_ALBEDO 0.9
// the point tested for visibility sits this many voxels off the surface, so a voxel's rays that end on the surface itself don't count against it
#define GLIMMER_SH_VISIBILITY_NORMAL_BIAS 0.3
// in voxels^2, see GlimmerSHVoxelVisibility
#define GLIMMER_SH_MIN_VARIANCE 0.03

float GlimmerSHVoxelVisibility(uint3 texel, float3 voxelToPoint, float distanceInVoxels)
{
    const float2 mapCoord = GlimmerOctahedralEncode(voxelToPoint / distanceInVoxels) * float(GLIMMER_SH_VISIBILITY_RES) - 0.5;
    const int2 mapTexel0 = int2(floor(mapCoord));
    const float2 fraction = mapCoord - float2(mapTexel0);

    float2 moments = (float2)0.0;

    [unroll]
    for (uint corner = 0; corner < 4; corner++)
    {
        const int2 offset = int2(corner & 1u, corner >> 1);
        const int2 mapTexel = GlimmerOctahedralWrapTexel(mapTexel0 + offset, GLIMMER_SH_VISIBILITY_RES);
        const uint mapIndex = uint(mapTexel.y * GLIMMER_SH_VISIBILITY_RES + mapTexel.x);
        const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));

        const float4 depthPair = glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + mapIndex / 2u), 0));

        moments += ((mapIndex & 1u) != 0u ? depthPair.zw : depthPair.xy) * (bilinear.x * bilinear.y);
    }

    if (distanceInVoxels <= moments.x)
    {
        return 1.0;
    }

    const float variance = max(moments.y - moments.x * moments.x, GLIMMER_SH_MIN_VARIANCE);
    const float difference = distanceInVoxels - moments.x;
    const float chebyshev = variance / (variance + difference * difference);

    return max(chebyshev * chebyshev, 0.0);
}

/// Based on DGGI:  https://www.jcgt.org/published/0008/02/01/paper-lowres.pdf
float GlimmerSHSampleCascade(GlimmerSHVolume volume, uint cascadeIndex, float3 P, float3 N, out float4 outVisibility, out float4 outBounce)
{
    outVisibility = (float4)0.0;
    outBounce = (float4)0.0;

    const GlimmerSHCascade cascade = volume.cascades[cascadeIndex];

    if (cascade.origin.w == 0)
    {
        return 0.0;
    }

    const int3 gridSize = int3(GLIMMER_SH_GRID_XZ, GLIMMER_SH_GRID_Y, GLIMMER_SH_GRID_XZ);

    const float3 voxelCoord = P * cascade.params.y - 0.5;
    const int3 voxel0 = int3(floor(voxelCoord));
    const float3 fraction = voxelCoord - float3(voxel0);

    const int3 local0 = voxel0 - cascade.origin.xyz;

    if (any(local0 < 0) || any(local0 >= gridSize - 1))
    {
        return 0.0;
    }

    const float3 edgeDistance = min(float3(local0) + fraction, float3(gridSize - 2) - float3(local0) - fraction);
    const float edgeWeight = saturate(min(edgeDistance.x, min(edgeDistance.y, edgeDistance.z)) / 2.0);

    if (edgeWeight <= 0.0)
    {
        return 0.0;
    }

    float4 visibilitySum = (float4)0.0;
    float4 bounceSum = (float4)0.0;
    float weightSum = 0.0;

    float triAccum = 0.0;

    [unroll]
    for (uint corner = 0; corner < 8; corner++)
    {
        const int3 offset = int3(corner & 1u, (corner >> 1) & 1u, (corner >> 2) & 1u);
        const int3 voxel = voxel0 + offset;
        const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);

        const uint2 state = glimmerSHState.Load(int4(texel, 0));

        if (!GlimmerSHStateMatches(state, voxel))
        {
            continue;
        }

        const float4 bounce = glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE), 0));

        if (bounce.a < 0.0)
        {
            continue;
        }

        const float3 trilinear = lerp(1.0 - fraction, fraction, float3(offset));
        const float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;

        triAccum += trilinearWeight;

        const float3 voxelCenter = GlimmerSHVoxelCenter(cascade, voxel) + GlimmerSHStateOffset(state) * cascade.params.x;
        const float3 toVoxel = voxelCenter - P;
        const float toVoxelLength = length(toVoxel);
        const float facing = toVoxelLength > 1e-4 ? (dot(toVoxel / toVoxelLength, N) + 1.0) * 0.5 : 1.0;

        // and voxels with a wall, trunk or ridge between them and the surface see the other side of it
        const float3 voxelToPoint = (P + N * (GLIMMER_SH_VISIBILITY_NORMAL_BIAS * cascade.params.x) - voxelCenter) * cascade.params.y;
        const float voxelToPointLength = length(voxelToPoint);
        const float visibility = voxelToPointLength > 1e-3 ? lerp(1.0, GlimmerSHVoxelVisibility(texel, voxelToPoint, voxelToPointLength), cascade.params.z) : 1.0;

        float weight = (facing * facing + 0.2) * visibility;

        const float crushThreshold = 0.2;

        if (weight < crushThreshold)
        {
            weight *= weight * weight / (crushThreshold * crushThreshold);
        }

        weight = trilinearWeight * max(weight, 1e-5);

        visibilitySum += glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY), 0)) * weight;
        bounceSum += bounce * weight;
        weightSum += weight;
    }

    if (weightSum <= 0.0)
    {
        return 0.0;
    }

    outVisibility = visibilitySum / weightSum;
    outBounce = bounceSum / weightSum;

    return edgeWeight * smoothstep(0.05, 0.5, triAccum);
}

float3 GlimmerSHSkyIrradiance(float3 N)
{
    if (skyProbe.textureIndices == ~0u)
    {
        return (float3)0.0;
    }

    float shBands[9];
    ProjectSHBands(N, shBands);

    return max(EnvProbeSH(skyProbe, shBands), (float3)0.0) * skyProbe.world_position.w * world_shader_data.sky_light_params.x;
}

float GlimmerSHSampleVolume(GlimmerSHVolume volume, float3 P, float3 N, out float4 outVisibility, out float4 outBounce)
{
    float4 visibility = (float4)0.0;
    float4 bounce = (float4)0.0;
    float remaining = 1.0;

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES && remaining > 1e-3; cascadeIndex++)
    {
        float4 cascadeVisibility;
        float4 cascadeBounce;
        const float cascadeWeight = GlimmerSHSampleCascade(volume, cascadeIndex, P + N * 0.05, N, cascadeVisibility, cascadeBounce);

        if (cascadeWeight <= 0.0)
        {
            continue;
        }

        visibility += cascadeVisibility * cascadeWeight * remaining;
        bounce += cascadeBounce * cascadeWeight * remaining;
        remaining *= 1.0 - cascadeWeight;
    }

    const float coverage = 1.0 - remaining;

    outVisibility = select(coverage > 1e-4, visibility / coverage, (float4)0.0);
    outBounce = select(coverage > 1e-4, bounce / coverage, (float4)0.0);

    return select(coverage > 1e-4, coverage, 0.0);
}

float GlimmerSHNearestSkySeen(GlimmerSHVolume volume, float3 P, float3 N)
{
    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        const GlimmerSHCascade cascade = volume.cascades[cascadeIndex];

        const int3 voxel = int3(floor(P * cascade.params.y));
        const int3 local = voxel - cascade.origin.xyz;

        if (cascade.origin.w == 0 || any(local < 1) || any(local >= int3(GLIMMER_SH_GRID_XZ, GLIMMER_SH_GRID_Y, GLIMMER_SH_GRID_XZ) - 1))
        {
            continue;
        }

        const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);

        if (!GlimmerSHStateMatches(glimmerSHState.Load(int4(texel, 0)), voxel) || glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE), 0)).a < 0.0)
        {
            return -1.0;
        }

        const float4 visibility = glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY), 0));

        return saturate(visibility.x + dot(visibility.yzw, N));
    }

    return -1.0;
}

float4 EvaluateGlimmerSH(GlimmerSHVolume volume, float3 P, float3 N)
{
    float4 visibility;
    float4 bounce;
    const float coverage = GlimmerSHSampleVolume(volume, P, N, visibility, bounce);

    if (coverage <= 0.0)
    {
        return (float4)0.0;
    }

    const float skySeen = saturate(visibility.x + dot(visibility.yzw, N));
    const float blockedSeen = 1.0 - skySeen;

    const float openSky = max(0.5 + 0.5 * N.y, 0.05);
    const float3 sky = GlimmerSHSkyIrradiance(N) * saturate(skySeen / openSky);

    const float3 blockerAlbedo = min(bounce.rgb, (float3)GLIMMER_SH_MAX_ALBEDO);
    const float blockerLuminance = dot(blockerAlbedo, float3(0.2126, 0.7152, 0.0722));
    const float3 sunTint = blockerLuminance > 1e-4 ? blockerAlbedo / blockerLuminance : (float3)1.0;

    const float3 sunIrradiance = world_shader_data.sun_color.rgb * world_shader_data.sun_direction_intensity.w;
    const float3 bounceLight = blockedSeen * (blockerAlbedo * GlimmerSHSkyIrradiance(float3(0.0, 1.0, 0.0)) + sunTint * sunIrradiance * max(bounce.a, 0.0) * 0.31830988618);

    return float4(sky + bounceLight, coverage);
}

#endif
