#ifndef GLIMMER_SH_APPLY_HLSLI
#define GLIMMER_SH_APPLY_HLSLI

#include "GlimmerSHCommon.hlsli"

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_SH_APPLY_SAMPLING_HLSLI)
#define GLIMMER_SH_APPLY_SAMPLING_HLSLI

// Bound by GlimmerTechnique::BindApplyResources(). A shader that samples the volume for its own purposes declares these itself
// (in its own descriptor set) and defines GLIMMER_SH_APPLY_EXTERNAL_RESOURCES first.
#ifndef GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
DECLARE_SRV(DeferredPass, GlimmerSHDataTexture) Texture3D<float4> glimmerSHData;
DECLARE_SRV(DeferredPass, GlimmerSHStateTexture) Texture3D<uint2> glimmerSHState;
#endif

#define GLIMMER_SH_MAX_ALBEDO 0.9

// the point tested for visibility sits this many voxels off the surface, so a voxel's rays that end on the surface itself don't count against it
#define GLIMMER_SH_VISIBILITY_NORMAL_BIAS 0.3

// Chebyshev bound on how likely the voxel sees a point this far (in voxels) toward it, from the distances its rays in that direction went
float GlimmerSHVoxelVisibility(uint3 texel, float3 voxelToPoint, float distanceInVoxels)
{
    // each axis contributes the moments of the way it points, by how much of the direction is along it; taking the dominant axis's
    // alone jumps wherever two axes tie, which shows up as hard diagonal lines through every voxel
    const float3 axisWeights = voxelToPoint * voxelToPoint / (distanceInVoxels * distanceInVoxels);

    float2 moments = (float2)0.0;

    [unroll]
    for (uint axis = 0; axis < 3; axis++)
    {
        const float4 depth = glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + axis), 0));

        moments += (voxelToPoint[axis] < 0.0 ? depth.zw : depth.xy) * axisWeights[axis];
    }

    if (distanceInVoxels <= moments.x)
    {
        return 1.0;
    }

    const float variance = max(moments.y - moments.x * moments.x, 1e-3);
    const float difference = distanceInVoxels - moments.x;
    const float chebyshev = variance / (variance + difference * difference);

    // sharpen, as DDGI does, so a partly blocked direction still mostly rejects
    return max(chebyshev * chebyshev * chebyshev, 0.0);
}

// Trilinear over the voxels traced for this cascade's window that aren't buried, favouring those in front of the surface.
// \return how much this cascade covers P, fading out over its outer voxels so the next one takes over
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

    [unroll]
    for (uint corner = 0; corner < 8; corner++)
    {
        const int3 offset = int3(corner & 1u, (corner >> 1) & 1u, (corner >> 2) & 1u);
        const int3 voxel = voxel0 + offset;
        const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);

        if (any(glimmerSHState.Load(int4(texel, 0)) != GlimmerSHPackVoxel(voxel)))
        {
            continue;
        }

        const float4 bounce = glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE), 0));

        if (bounce.a < 0.0)
        {
            continue;
        }

        const float3 trilinear = lerp(1.0 - fraction, fraction, float3(offset));

        // voxels behind the surface see what's behind it
        const float3 voxelCenter = GlimmerSHVoxelCenter(cascade, voxel);
        const float3 toVoxel = voxelCenter - P;
        const float toVoxelLength = length(toVoxel);
        const float facing = toVoxelLength > 1e-4 ? (dot(toVoxel / toVoxelLength, N) + 1.0) * 0.5 : 1.0;

        // and voxels with a wall, trunk or ridge between them and the surface see the other side of it
        const float3 voxelToPoint = (P + N * (GLIMMER_SH_VISIBILITY_NORMAL_BIAS * cascade.params.x) - voxelCenter) * cascade.params.y;
        const float voxelToPointLength = length(voxelToPoint);
        const float visibility = voxelToPointLength > 1e-3 ? GlimmerSHVoxelVisibility(texel, voxelToPoint, voxelToPointLength) : 1.0;

        // like DDGI, weights are crushed rather than dropped: where every voxel around P is blocked they still blend evenly,
        // instead of P falling through to the next cascade with a seam
        float weight = trilinear.x * trilinear.y * trilinear.z * (facing * facing + 0.05) * visibility;

        const float crushThreshold = 0.2;

        if (weight < crushThreshold)
        {
            weight *= weight * weight / (crushThreshold * crushThreshold);
        }

        weight = max(weight, 1e-6);

        visibilitySum += glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY), 0)) * weight;
        bounceSum += bounce * weight;
        weightSum += weight;
    }

    if (weightSum <= 1e-5)
    {
        return 0.0;
    }

    outVisibility = visibilitySum / weightSum;
    outBounce = bounceSum / weightSum;

    return edgeWeight;
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

/*! Blends the cascades that cover P, finest first. \return their combined coverage */
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

    outVisibility = coverage > 1e-4 ? visibility / coverage : (float4)0.0;
    outBounce = coverage > 1e-4 ? bounce / coverage : (float4)0.0;

    return coverage > 1e-4 ? coverage : 0.0;
}

/*! The fraction of the sky a surface facing N sees, from the traced voxel P is in and nothing else (no interpolation, no weights), or -1 where
 *  no cascade has traced it. For telling artifacts in the traced data from ones made by interpolating it. */
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

        if (any(glimmerSHState.Load(int4(texel, 0)) != GlimmerSHPackVoxel(voxel)) || glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE), 0)).a < 0.0)
        {
            return -1.0;
        }

        const float4 visibility = glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_VISIBILITY), 0));

        return saturate(visibility.x + dot(visibility.yzw, N));
    }

    return -1.0;
}

/*! The far field: relights the voxels around P with the current sky and sun. Expects skyProbe (EnvProbe) and world_shader_data to be declared. */
float4 EvaluateGlimmerSH(GlimmerSHVolume volume, float3 P, float3 N)
{
    float4 visibility;
    float4 bounce;
    const float coverage = GlimmerSHSampleVolume(volume, P, N, visibility, bounce);

    if (coverage <= 0.0)
    {
        return (float4)0.0;
    }

    // cosine weighted fraction of the sky seen, and of what blocks it
    const float skySeen = saturate(visibility.x + dot(visibility.yzw, N));
    const float blockedSeen = 1.0 - skySeen;

    // the sky probe's irradiance already falls off toward the ground, so the sky term takes the visibility relative to open ground
    const float openSky = max(0.5 + 0.5 * N.y, 0.05);
    const float3 sky = GlimmerSHSkyIrradiance(N) * saturate(skySeen / openSky);

    // blockers reflect the sky they face (already in bounce.rgb) and the sun where it reaches them
    const float3 sunIrradiance = world_shader_data.sun_color.rgb * world_shader_data.sun_direction_intensity.w;
    const float3 blockerLighting = GlimmerSHSkyIrradiance(float3(0.0, 1.0, 0.0)) + sunIrradiance * max(bounce.a, 0.0) * 0.31830988618;
    const float3 bounceLight = blockedSeen * min(bounce.rgb, (float3)GLIMMER_SH_MAX_ALBEDO) * blockerLighting;

    return float4(sky + bounceLight, coverage);
}

#endif
