#ifndef GLIMMER_SH_SAMPLE_HLSLI
#define GLIMMER_SH_SAMPLE_HLSLI

// define GLIMMER_SH_LOAD_DATA(texel), GLIMMER_SH_LOAD_STATE(texel) and GLIMMER_SH_LOAD_RADIANCE(texel) before including

#include "GlimmerSHCommon.hlsli"

#define GLIMMER_SH_VISIBILITY_NORMAL_BIAS 0.3
#define GLIMMER_SH_CRUSH_THRESHOLD 0.2
#define GLIMMER_SH_AXIS_FADE 0.5

float GlimmerSHAxisVisibility(GlimmerSHVoxel voxelData, float3 voxelToPoint)
{
    const float3 distances = abs(voxelToPoint);

    const float3 depths = float3(
        voxelData.depths[voxelToPoint.x < 0.0 ? 1u : 0u],
        voxelData.depths[voxelToPoint.y < 0.0 ? 3u : 2u],
        voxelData.depths[voxelToPoint.z < 0.0 ? 5u : 4u]);

    const float3 visible = saturate(1.0 - (distances - depths) / GLIMMER_SH_AXIS_FADE);

    const float3 direction = distances / max(length(distances), 1e-4);

    float3 weights = direction * direction;
    weights *= weights;

    const float weightSum = weights.x + weights.y + weights.z;

    return weightSum > 1e-6 ? dot(visible, weights) / weightSum : 1.0;
}

/// Based on DDGI:  https://www.jcgt.org/published/0008/02/01/paper-lowres.pdf
float GlimmerSHSampleCascade(GlimmerSHVolume volume, uint cascadeIndex, float3 P, float3 N, out float4 outVisibility, out GlimmerSHRadiance outRadiance)
{
    outVisibility = (float4)0.0;
    outRadiance = (GlimmerSHRadiance)0;

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
    GlimmerSHRadiance radianceSum = (GlimmerSHRadiance)0;
    float weightSum = 0.0;

    float trilinearSum = 0.0;

    [unroll]
    for (uint corner = 0; corner < 8; corner++)
    {
        const int3 offset = int3(corner & 1u, (corner >> 1) & 1u, (corner >> 2) & 1u);
        const int3 voxel = voxel0 + offset;
        const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);

        GlimmerSHVoxel voxelData;

        if (!GlimmerSHUnpackVoxel(GLIMMER_SH_LOAD_STATE(texel), voxel, voxelData) || voxelData.isBuried)
        {
            continue;
        }

        const float3 trilinear = lerp(1.0 - fraction, fraction, float3(offset));
        const float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;

        trilinearSum += trilinearWeight;

        const float3 voxelCenter = GlimmerSHVoxelCenter(cascade, voxel) + voxelData.originOffset * cascade.params.x;
        const float3 toVoxel = voxelCenter - P;
        const float toVoxelLength = length(toVoxel);
        const float facing = toVoxelLength > 1e-4 ? (dot(toVoxel / toVoxelLength, N) + 1.0) * 0.5 : 1.0;

        const float3 voxelToPoint = (P + N * (GLIMMER_SH_VISIBILITY_NORMAL_BIAS * cascade.params.x) - voxelCenter) * cascade.params.y;
        const float visibility = lerp(1.0, GlimmerSHAxisVisibility(voxelData, voxelToPoint), cascade.params.z);

        float weight = (facing * facing + 0.2) * visibility;

        if (weight < GLIMMER_SH_CRUSH_THRESHOLD)
        {
            weight *= weight * weight / (GLIMMER_SH_CRUSH_THRESHOLD * GLIMMER_SH_CRUSH_THRESHOLD);
        }

        weight = trilinearWeight * max(weight, 1e-5);

        visibilitySum += GLIMMER_SH_LOAD_DATA(texel) * weight;
        radianceSum.r += GLIMMER_SH_LOAD_RADIANCE(GlimmerSHRadianceTexel(texel, 0u)) * weight;
        radianceSum.g += GLIMMER_SH_LOAD_RADIANCE(GlimmerSHRadianceTexel(texel, 1u)) * weight;
        radianceSum.b += GLIMMER_SH_LOAD_RADIANCE(GlimmerSHRadianceTexel(texel, 2u)) * weight;
        weightSum += weight;
    }

    if (weightSum <= 0.0)
    {
        return 0.0;
    }

    outVisibility = visibilitySum / weightSum;
    outRadiance.r = radianceSum.r / weightSum;
    outRadiance.g = radianceSum.g / weightSum;
    outRadiance.b = radianceSum.b / weightSum;

    return edgeWeight * smoothstep(0.05, 0.5, trilinearSum);
}

float GlimmerSHSampleVolume(GlimmerSHVolume volume, float3 P, float3 N, out float4 outVisibility, out GlimmerSHRadiance outRadiance)
{
    float4 visibility = (float4)0.0;
    GlimmerSHRadiance radiance = (GlimmerSHRadiance)0;
    float remaining = 1.0;

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES && remaining > 1e-3; cascadeIndex++)
    {
        float4 cascadeVisibility;
        GlimmerSHRadiance cascadeRadiance;
        const float cascadeWeight = GlimmerSHSampleCascade(volume, cascadeIndex, P + N * 0.05, N, cascadeVisibility, cascadeRadiance);

        if (cascadeWeight <= 0.0)
        {
            continue;
        }

        const float weight = cascadeWeight * remaining;

        visibility += cascadeVisibility * weight;
        radiance.r += cascadeRadiance.r * weight;
        radiance.g += cascadeRadiance.g * weight;
        radiance.b += cascadeRadiance.b * weight;

        remaining *= 1.0 - cascadeWeight;
    }

    const float coverage = 1.0 - remaining;

    outVisibility = (float4)0.0;
    outRadiance = (GlimmerSHRadiance)0;

    if (coverage <= 1e-4)
    {
        return 0.0;
    }

    outVisibility = visibility / coverage;
    outRadiance.r = radiance.r / coverage;
    outRadiance.g = radiance.g / coverage;
    outRadiance.b = radiance.b / coverage;

    return coverage;
}

#endif
