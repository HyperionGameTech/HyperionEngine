#ifndef GLIMMER_SH_SAMPLE_HLSLI
#define GLIMMER_SH_SAMPLE_HLSLI

#include "GlimmerSHCommon.hlsli"

#define GLIMMER_SH_VISIBILITY_NORMAL_BIAS 0.3
#define GLIMMER_SH_SAMPLE_NORMAL_OFFSET 0.25
#define GLIMMER_SH_CRUSH_THRESHOLD 0.2
#define GLIMMER_SH_AXIS_FADE 0.5
// a rejected voxel keeps this much weight: with next to none, the blend jumps where the voxels around the point change
#define GLIMMER_SH_MIN_WEIGHT 0.05

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

struct GlimmerSHTaps
{
    uint3 texels[8];
    float weights[8];
    float weightSum;
};

/// Based on DDGI:  https://www.jcgt.org/published/0008/02/01/paper-lowres.pdf
// the voxels around P and what each counts for; returns how much the cascade covers P
float GlimmerSHGatherTaps(GlimmerSHVolume volume, uint cascadeIndex, float3 P, float3 N, out GlimmerSHTaps outTaps)
{
    [unroll]
    for (uint tap = 0; tap < 8; tap++)
    {
        outTaps.texels[tap] = (uint3)0u;
        outTaps.weights[tap] = 0.0;
    }

    outTaps.weightSum = 0.0;

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

    const float3 fromCenter = P * cascade.params.y - cascade.center.xyz;
    const float3 halfGrid = float3(gridSize) * 0.5;

    const float3 edgeDistance = min(fromCenter + halfGrid - 2.5, halfGrid - 3.5 - fromCenter);
    const float edgeWeight = saturate(min(edgeDistance.x, min(edgeDistance.y, edgeDistance.z)) / 2.0);

    if (edgeWeight <= 0.0)
    {
        return 0.0;
    }

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

        weight = trilinearWeight * max(weight, GLIMMER_SH_MIN_WEIGHT);

        outTaps.texels[corner] = texel;
        outTaps.weights[corner] = weight;
        outTaps.weightSum += weight;
    }

    if (outTaps.weightSum <= 0.0)
    {
        return 0.0;
    }

    return edgeWeight * smoothstep(0.05, 0.5, trilinearSum);
}

float GlimmerSHSampleCascade(GlimmerSHVolume volume, uint cascadeIndex, float3 P, float3 N, out float4 outVisibility, out GlimmerSHRadiance outRadiance)
{
    outVisibility = (float4)0.0;
    outRadiance = (GlimmerSHRadiance)0;

    GlimmerSHTaps taps;
    const float cascadeWeight = GlimmerSHGatherTaps(volume, cascadeIndex, P, N, taps);

    if (cascadeWeight <= 0.0)
    {
        return 0.0;
    }

    float4 visibilitySum = (float4)0.0;
    GlimmerSHRadiance radianceSum = (GlimmerSHRadiance)0;

    [unroll]
    for (uint tap = 0; tap < 8; tap++)
    {
        [branch]
        if (taps.weights[tap] > 0.0)
        {
            const uint3 texel = taps.texels[tap];
            const float weight = taps.weights[tap];

            visibilitySum += GLIMMER_SH_LOAD_DATA(texel) * weight;
            radianceSum.r += GLIMMER_SH_LOAD_RADIANCE(GlimmerSHRadianceTexel(texel, 0u)) * weight;
            radianceSum.g += GLIMMER_SH_LOAD_RADIANCE(GlimmerSHRadianceTexel(texel, 1u)) * weight;
            radianceSum.b += GLIMMER_SH_LOAD_RADIANCE(GlimmerSHRadianceTexel(texel, 2u)) * weight;
        }
    }

    outVisibility = visibilitySum / taps.weightSum;
    outRadiance.r = radianceSum.r / taps.weightSum;
    outRadiance.g = radianceSum.g / taps.weightSum;
    outRadiance.b = radianceSum.b / taps.weightSum;

    return cascadeWeight;
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
        const float cascadeWeight = GlimmerSHSampleCascade(volume, cascadeIndex, P + N * (GLIMMER_SH_SAMPLE_NORMAL_OFFSET * volume.cascades[cascadeIndex].params.x), N, cascadeVisibility, cascadeRadiance);

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

#ifdef GLIMMER_SH_SPLIT

#define GLIMMER_SH_LIGHT_TAP_CUTOFF 0.02
#define GLIMMER_SH_LIGHT_MIN_REMAINING 0.02

// the sun bounce a voxel holds facing N, for the sun now as fresh as it is, per unit of the sun's irradiance
float3 GlimmerSHVoxelSun(GlimmerSurfels surfels, uint3 texel, float3 N)
{
    float4 keyedSun[3];
    float keyedFreshness;
    bool hasKeys;
    GLIMMER_SH_LOAD_SUN_KEYS(surfels, texel, keyedSun, keyedFreshness, hasKeys);

    if (hasKeys)
    {
        return GlimmerEvaluateSunBounce(keyedSun, keyedFreshness, N);
    }

    float4 sun[3];
    uint sunState;
    GLIMMER_SH_LOAD_SUN(texel, sun, sunState);

    return GlimmerEvaluateSunBounce(sun, GlimmerSurfelSunFreshness(surfels, sunState), N);
}

float GlimmerSHSampleCascadeLight(GlimmerSHVolume volume, GlimmerSurfels surfels, uint cascadeIndex, float3 P, float3 N, out GlimmerLight outLight, out float3 outSun)
{
    outLight = GlimmerNoLight();
    outSun = (float3)0.0;

    GlimmerSHTaps taps;
    const float cascadeWeight = GlimmerSHGatherTaps(volume, cascadeIndex, P, N, taps);

    if (cascadeWeight <= 0.0)
    {
        return 0.0;
    }

    float weights[8];
    float weightSum = 0.0;

    [unroll]
    for (uint keptTap = 0; keptTap < 8; keptTap++)
    {
        weights[keptTap] = taps.weights[keptTap] * smoothstep(GLIMMER_SH_LIGHT_TAP_CUTOFF, 2.0 * GLIMMER_SH_LIGHT_TAP_CUTOFF, taps.weights[keptTap] / taps.weightSum);
        weightSum += weights[keptTap];
    }

    float4 entries[GLIMMER_LIGHT_ENTRIES];

    for (uint clearEntry = 0; clearEntry < GLIMMER_LIGHT_ENTRIES; clearEntry++)
    {
        entries[clearEntry] = (float4)0.0;
    }

    [unroll]
    for (uint tap = 0; tap < 8; tap++)
    {
        [branch]
        if (weights[tap] > 0.0)
        {
            const float weight = weights[tap] / weightSum;

            for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
            {
                entries[entry] += GLIMMER_SH_LOAD_LIGHT(taps.texels[tap], entry) * weight;
            }

            outSun += GlimmerSHVoxelSun(surfels, taps.texels[tap], N) * weight;
        }
    }

    outLight = GlimmerEvaluateStoredLight(entries, N);

    return cascadeWeight;
}

/// what the volume receives at P facing N, and its sun bounce for the sun now as fresh as it is; returns its coverage
float GlimmerSHSampleVolumeLightAndSun(GlimmerSHVolume volume, GlimmerSurfels surfels, float3 P, float3 N, out GlimmerLight outLight, out float3 outSun)
{
    outLight = GlimmerNoLight();
    outSun = (float3)0.0;

    if (volume.info.w == 0u)
    {
        return 0.0;
    }

    GlimmerLight light = GlimmerNoLight();
    float3 sun = (float3)0.0;
    float remaining = 1.0;

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES && remaining > GLIMMER_SH_LIGHT_MIN_REMAINING; cascadeIndex++)
    {
        GlimmerLight cascadeLight;
        float3 cascadeSun;
        const float cascadeWeight = GlimmerSHSampleCascadeLight(volume, surfels, cascadeIndex, P + N * 0.05, N, cascadeLight, cascadeSun);

        if (cascadeWeight <= 0.0)
        {
            continue;
        }

        GlimmerAddLight(light, cascadeLight, (float3)(cascadeWeight * remaining));
        sun += cascadeSun * (cascadeWeight * remaining);
        remaining *= 1.0 - cascadeWeight;
    }

    const float coverage = 1.0 - remaining;

    if (coverage <= 1e-4)
    {
        return 0.0;
    }

    outLight = GlimmerScaleLight(light, (float3)(1.0 / coverage));
    outSun = sun / coverage;

    return coverage;
}

bool GlimmerSHSampleNearestLightAndSun(GlimmerSHVolume volume, GlimmerSurfels surfels, float3 P, float3 N, float surfaceOffset, out GlimmerLight outLight, out float3 outSun)
{
    outLight = GlimmerNoLight();
    outSun = (float3)0.0;

    if (volume.info.w == 0u)
    {
        return false;
    }

    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        const GlimmerSHCascade cascade = volume.cascades[cascadeIndex];

        const int3 voxel = int3(floor((P + N * (surfaceOffset * cascade.params.x)) * cascade.params.y));
        const int3 local = voxel - cascade.origin.xyz;

        if (cascade.origin.w == 0 || any(local < 0) || any(local >= int3(GLIMMER_SH_GRID_XZ, GLIMMER_SH_GRID_Y, GLIMMER_SH_GRID_XZ)))
        {
            continue;
        }

        const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);

        GlimmerSHVoxel voxelData;

        if (!GlimmerSHUnpackVoxel(GLIMMER_SH_LOAD_STATE(texel), voxel, voxelData) || voxelData.isBuried)
        {
            continue;
        }

        if (surfaceOffset > 0.0 && dot(GlimmerSHVoxelCenter(cascade, voxel) + voxelData.originOffset * cascade.params.x - P, N) <= 0.0)
        {
            return false;
        }

        float4 entries[GLIMMER_LIGHT_ENTRIES];

        for (uint entry = 0; entry < GLIMMER_LIGHT_ENTRIES; entry++)
        {
            entries[entry] = GLIMMER_SH_LOAD_LIGHT(texel, entry);
        }

        outLight = GlimmerEvaluateStoredLight(entries, N);
        outSun = GlimmerSHVoxelSun(surfels, texel, N);

        return true;
    }

    return false;
}

#endif // GLIMMER_SH_SPLIT

#endif
