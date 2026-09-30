#ifndef GLIMMER_SH_APPLY_HLSLI
#define GLIMMER_SH_APPLY_HLSLI

#include "GlimmerSHCommon.hlsli"

// Must match what GlimmerSHTechnique::WriteApplyShaderData() writes
struct GlimmerTechniqueApply
{
    GlimmerSHVolume volume;
};

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_SH_APPLY_SAMPLING_HLSLI)
#define GLIMMER_SH_APPLY_SAMPLING_HLSLI

// Bound by GlimmerSHTechnique::BindApplyResources()
DECLARE_SRV(DeferredPass, GlimmerSHVisibilityTexture) Texture3D<float4> glimmerSHVisibility;
DECLARE_SRV(DeferredPass, GlimmerSHBounceTexture) Texture3D<float4> glimmerSHBounce;
DECLARE_SRV(DeferredPass, GlimmerSHStateTexture) Texture3D<uint2> glimmerSHState;
DECLARE_SRV(DeferredPass, GlimmerSHDepthXTexture) Texture3D<float4> glimmerSHDepthX;
DECLARE_SRV(DeferredPass, GlimmerSHDepthYTexture) Texture3D<float4> glimmerSHDepthY;
DECLARE_SRV(DeferredPass, GlimmerSHDepthZTexture) Texture3D<float4> glimmerSHDepthZ;

#define GLIMMER_SH_MAX_ALBEDO 0.9

// the point tested for visibility sits this many voxels off the surface, so a voxel's rays that end on the surface itself don't count against it
#define GLIMMER_SH_VISIBILITY_NORMAL_BIAS 0.3

// Chebyshev bound on how likely the voxel sees a point this far (in voxels) toward it, from the distances its rays in that direction went
float GlimmerSHVoxelVisibility(uint3 texel, float3 voxelToPoint, float distanceInVoxels)
{
    const uint bin = GlimmerSHDirectionBin(voxelToPoint);
    const uint axis = bin >> 1;

    const float4 depth = axis == 0u
        ? glimmerSHDepthX.Load(int4(texel, 0))
        : (axis == 1u ? glimmerSHDepthY.Load(int4(texel, 0)) : glimmerSHDepthZ.Load(int4(texel, 0)));

    const float2 moments = (bin & 1u) != 0u ? depth.zw : depth.xy;

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

        const float4 bounce = glimmerSHBounce.Load(int4(texel, 0));

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

        const float weight = trilinear.x * trilinear.y * trilinear.z * (facing * facing + 0.05) * visibility;

        if (weight <= 1e-6)
        {
            continue;
        }

        visibilitySum += glimmerSHVisibility.Load(int4(texel, 0)) * weight;
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

/*! Relights the voxels around P with the current sky and sun. Expects skyProbe (EnvProbe) and world_shader_data to be declared. */
float4 EvaluateGlimmerTechnique(GlimmerTechniqueApply techniqueApply, float3 P, float3 N)
{
    float4 visibility = (float4)0.0;
    float4 bounce = (float4)0.0;
    float remaining = 1.0;

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES && remaining > 1e-3; cascadeIndex++)
    {
        float4 cascadeVisibility;
        float4 cascadeBounce;
        const float cascadeWeight = GlimmerSHSampleCascade(techniqueApply.volume, cascadeIndex, P + N * 0.05, N, cascadeVisibility, cascadeBounce);

        if (cascadeWeight <= 0.0)
        {
            continue;
        }

        visibility += cascadeVisibility * cascadeWeight * remaining;
        bounce += cascadeBounce * cascadeWeight * remaining;
        remaining *= 1.0 - cascadeWeight;
    }

    const float coverage = 1.0 - remaining;

    if (coverage <= 1e-4)
    {
        return (float4)0.0;
    }

    visibility /= coverage;
    bounce /= coverage;

    // cosine weighted fraction of the sky seen, and of what blocks it
    const float skySeen = saturate(visibility.x + dot(visibility.yzw, N));
    const float blockedSeen = 1.0 - skySeen;

    // the sky probe's irradiance already falls off toward the ground, so the sky term takes the visibility relative to open ground
    const float openSky = max(0.5 + 0.5 * N.y, 0.05);
    const float3 sky = GlimmerSHSkyIrradiance(N) * saturate(skySeen / openSky);

    // blockers reflect the sun where it reaches them and about half the sky
    const float3 sunIrradiance = world_shader_data.sun_color.rgb * world_shader_data.sun_direction_intensity.w;
    const float3 blockerLighting = sunIrradiance * saturate(bounce.a) * 0.31830988618 + GlimmerSHSkyIrradiance(float3(0.0, 1.0, 0.0)) * 0.5;
    const float3 bounceLight = blockedSeen * min(bounce.rgb, (float3)GLIMMER_SH_MAX_ALBEDO) * blockerLighting;

    return float4(sky + bounceLight, coverage);
}

#endif
