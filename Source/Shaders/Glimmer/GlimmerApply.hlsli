#ifndef GLIMMER_APPLY_HLSLI
#define GLIMMER_APPLY_HLSLI

#include "SH/GlimmerSHApply.hlsli"

struct GlimmerApply
{
    uint4 params;    // x = GLIMMER_DEBUG_VIS_*, y = 1 when the volume can be sampled, z = GLIMMER_DEBUG_SH_*
    float4 settings; // x = intensity
    GlimmerSHVolume sh;
};

#define GLIMMER_DEBUG_VIS_IRRADIANCE 1
#define GLIMMER_DEBUG_VIS_COVERAGE 2

#define GLIMMER_DEBUG_SH_IRRADIANCE 1
#define GLIMMER_DEBUG_SH_VISIBILITY 2
#define GLIMMER_DEBUG_SH_NEAREST 3

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_APPLY_SAMPLING_HLSLI)
#define GLIMMER_APPLY_SAMPLING_HLSLI

#include "SH/GlimmerSHApply.hlsli"

float4 EvaluateGlimmer(GlimmerApply glimmer, float3 P, float3 N)
{
    if (glimmer.params.y == 0u)
    {
        return (float4)0.0;
    }

    float4 irradiance = EvaluateGlimmerSH(glimmer.sh, P, N);
    irradiance.rgb *= glimmer.settings.x;

    return irradiance;
}

float4 EvaluateGlimmer(GlimmerApply glimmer, float3 P, float3 N, float3 R, out float2 outSpecular, out float3 outReflection)
{
    outSpecular = (float2)0.0;
    outReflection = (float3)0.0;

    if (glimmer.params.y == 0u)
    {
        return (float4)0.0;
    }

    float4 visibility;
    GlimmerSHRadiance radiance;
    const float shCoverage = GlimmerSHSampleVolume(glimmer.sh, P, N, visibility, radiance);

    if (shCoverage <= 0.0)
    {
        return (float4)0.0;
    }

    outSpecular = GlimmerSHSpecularVisibility(visibility, shCoverage, R);
    outReflection = GlimmerSHEvaluate(radiance, R) * glimmer.settings.x;

    return float4(GlimmerSHEvaluate(radiance, N) * glimmer.settings.x, shCoverage);
}

float3 EvaluateGlimmerSHDebug(GlimmerApply glimmer, float3 P, float3 N)
{
    const float3 missing = float3(1.0, 0.0, 1.0);

    if (glimmer.params.z == GLIMMER_DEBUG_SH_NEAREST)
    {
        const float skySeen = GlimmerSHNearestSkySeen(glimmer.sh, P, N);

        return skySeen >= 0.0 ? (float3)skySeen : missing;
    }

    float4 visibility;
    GlimmerSHRadiance radiance;
    const float coverage = GlimmerSHSampleVolume(glimmer.sh, P, N, visibility, radiance);

    if (coverage <= 0.0)
    {
        return missing;
    }

    if (glimmer.params.z == GLIMMER_DEBUG_SH_VISIBILITY)
    {
        return (float3)saturate(visibility.x + dot(visibility.yzw, N));
    }

    return GlimmerSHEvaluate(radiance, N) * glimmer.settings.x;
}

/// green for surfels, yellow for foliage voxels, then what's left by SH cascade
float3 EvaluateGlimmerCoverage(GlimmerApply glimmer, float3 P, float3 N, float surfelCoverage, float foliageCoverage)
{
    float3 color = float3(0.1, 1.0, 0.2) * surfelCoverage;
    float remaining = 1.0 - surfelCoverage;

    color += float3(1.0, 0.85, 0.1) * foliageCoverage * remaining;
    remaining *= 1.0 - foliageCoverage;

    if (glimmer.params.y != 0u)
    {
        [loop]
        for (uint shCascade = 0; shCascade < GLIMMER_SH_CASCADES && remaining > 1e-3; shCascade++)
        {
            float4 cascadeVisibility;
            GlimmerSHRadiance cascadeRadiance;
            const float weight = GlimmerSHSampleCascade(glimmer.sh, shCascade, P + N * 0.05, N, cascadeVisibility, cascadeRadiance);

            const float t = float(shCascade) / float(max(GLIMMER_SH_CASCADES - 1, 1));

            color += lerp(float3(0.0, 1.0, 1.0), float3(0.6, 0.1, 1.0), t) * weight * remaining;
            remaining *= 1.0 - weight;
        }
    }

    return color + float3(1.0, 0.0, 1.0) * remaining;
}

#endif
