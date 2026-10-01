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

// in voxels^2, see GlimmerSHVoxelVisibility
#define GLIMMER_SH_MIN_VARIANCE 0.03

// Chebyshev bound on how likely the voxel sees a point this far (in voxels) toward it, from the distances its rays in that direction went
float GlimmerSHVoxelVisibility(uint3 texel, float3 voxelToPoint, float distanceInVoxels)
{
    // bilinear between the four depth map texels around the direction (clamped rather than wrapped at the octahedron's edges)
    const float2 mapCoord = GlimmerOctahedralEncode(voxelToPoint / distanceInVoxels) * float(GLIMMER_SH_VISIBILITY_RES) - 0.5;
    const int2 mapTexel0 = int2(floor(mapCoord));
    const float2 fraction = mapCoord - float2(mapTexel0);

    float2 moments = (float2)0.0;

    [unroll]
    for (uint corner = 0; corner < 4; corner++)
    {
        const int2 offset = int2(corner & 1u, corner >> 1);
        const int2 mapTexel = clamp(mapTexel0 + offset, 0, GLIMMER_SH_VISIBILITY_RES - 1);
        const uint mapIndex = uint(mapTexel.y * GLIMMER_SH_VISIBILITY_RES + mapTexel.x);
        const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));

        const float4 depthPair = glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_DEPTH + mapIndex / 2u), 0));

        moments += ((mapIndex & 1u) != 0u ? depthPair.zw : depthPair.xy) * (bilinear.x * bilinear.y);
    }

    if (distanceInVoxels <= moments.x)
    {
        return 1.0;
    }

    // floored at about (0.17 voxel)^2, so a voxel's shadow of a flat wall fades in rather than cutting off: with every ray that way ending on
    // the same wall the variance is ~0, and the bound would snap from 1 to 0 over a few centimetres
    const float variance = max(moments.y - moments.x * moments.x, GLIMMER_SH_MIN_VARIANCE);
    const float difference = distanceInVoxels - moments.x;
    const float chebyshev = variance / (variance + difference * difference);

    // sharpened, as DDGI does, so a partly blocked direction still mostly rejects; only squared, as a voxel is several metres across and
    // DDGI's cube makes the edge of its shadow read as a hard line
    return max(chebyshev * chebyshev, 0.0);
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

    // the trilinear weight of the corners that were traced and aren't buried. Where it runs out (next to a buried voxel, at the cell
    // face the usable corners have no weight on) the renormalized blend would jump, so the cascade fades out and the next takes over
    float presentTrilinear = 0.0;

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

        presentTrilinear += trilinearWeight;

        // voxels behind the surface see what's behind it; measured from where its rays started, which is off the centre when that's in a solid
        const float3 voxelCenter = GlimmerSHVoxelCenter(cascade, voxel) + GlimmerSHStateOffset(state) * cascade.params.x;
        const float3 toVoxel = voxelCenter - P;
        const float toVoxelLength = length(toVoxel);
        const float facing = toVoxelLength > 1e-4 ? (dot(toVoxel / toVoxelLength, N) + 1.0) * 0.5 : 1.0;

        // and voxels with a wall, trunk or ridge between them and the surface see the other side of it
        const float3 voxelToPoint = (P + N * (GLIMMER_SH_VISIBILITY_NORMAL_BIAS * cascade.params.x) - voxelCenter) * cascade.params.y;
        const float voxelToPointLength = length(voxelToPoint);
        const float visibility = voxelToPointLength > 1e-3 ? lerp(1.0, GlimmerSHVoxelVisibility(texel, voxelToPoint, voxelToPointLength), cascade.params.z) : 1.0;

        // like DDGI, the facing and visibility weight is crushed rather than dropped: where every voxel around P is blocked they
        // still blend evenly, instead of P falling through to the next cascade with a seam. Trilinear comes in after the crush, which
        // would otherwise turn the interpolation between voxels into steps (nearly every trilinear weight is under the threshold)
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

    // every usable corner keeps some weight, so this is only where none is: when they're all blocked they blend evenly rather
    // than dropping P to the next cascade
    if (weightSum <= 0.0)
    {
        return 0.0;
    }

    outVisibility = visibilitySum / weightSum;
    outBounce = bounceSum / weightSum;

    return edgeWeight * smoothstep(0.05, 0.5, presentTrilinear);
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

        if (!GlimmerSHStateMatches(glimmerSHState.Load(int4(texel, 0)), voxel) || glimmerSHData.Load(int4(GlimmerSHSlabTexel(texel, GLIMMER_SH_SLAB_BOUNCE), 0)).a < 0.0)
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

    // blockers reflect the sky they face (already in bounce.rgb) and the sun where it reaches them (bounce.a, tinted as the sky-lit blockers are)
    const float3 blockerAlbedo = min(bounce.rgb, (float3)GLIMMER_SH_MAX_ALBEDO);
    const float blockerLuminance = dot(blockerAlbedo, float3(0.2126, 0.7152, 0.0722));
    const float3 sunTint = blockerLuminance > 1e-4 ? blockerAlbedo / blockerLuminance : (float3)1.0;

    const float3 sunIrradiance = world_shader_data.sun_color.rgb * world_shader_data.sun_direction_intensity.w;
    const float3 bounceLight = blockedSeen * (blockerAlbedo * GlimmerSHSkyIrradiance(float3(0.0, 1.0, 0.0)) + sunTint * sunIrradiance * max(bounce.a, 0.0) * 0.31830988618);

    return float4(sky + bounceLight, coverage);
}

#endif
