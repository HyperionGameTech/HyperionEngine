#ifndef GLIMMER_APPLY_HLSLI
#define GLIMMER_APPLY_HLSLI

#include "SWRT/GlimmerSWRTApply.hlsli"
#include "SH/GlimmerSHApply.hlsli"

struct GlimmerApply
{
    uint4 params;    // x = GLIMMER_DEBUG_VIS_*, y = 1 when either volume can be sampled, z = GLIMMER_DEBUG_SH_*
    float4 settings; // x = intensity
    GlimmerProbeVolume probes;
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

#include "SWRT/GlimmerSWRTApply.hlsli"
#include "SH/GlimmerSHApply.hlsli"

float4 EvaluateGlimmer(GlimmerApply glimmer, float3 P, float3 N)
{
    if (glimmer.params.y == 0u)
    {
        return (float4)0.0;
    }

    const float4 nearField = EvaluateGlimmerProbes(glimmer.probes, P, N);

    float4 farField = (float4)0.0;

    if (nearField.a < 0.999)
    {
        farField = EvaluateGlimmerSH(glimmer.sh, P, N);
    }

    float4 irradiance = GlimmerBlendFarField(nearField, farField);
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

    float3 nearFieldR;
    const float4 nearField = EvaluateGlimmerProbes(glimmer.probes, P, N, R, nearFieldR);

    float4 visibility;
    GlimmerSHRadiance radiance;
    const float shCoverage = GlimmerSHSampleVolume(glimmer.sh, P, N, visibility, radiance);

    float4 farField = (float4)0.0;
    float3 farFieldR = (float3)0.0;

    if (shCoverage > 0.0)
    {
        farField = float4(GlimmerSHEvaluate(radiance, N), shCoverage);
        farFieldR = GlimmerSHEvaluate(radiance, R);
        outSpecular = GlimmerSHSpecularVisibility(visibility, shCoverage, R);
    }

    float4 irradiance = GlimmerBlendFarField(nearField, farField);
    irradiance.rgb *= glimmer.settings.x;

    outReflection = GlimmerBlendFarField(float4(nearFieldR, nearField.a), float4(farFieldR, farField.a)).rgb * glimmer.settings.x;

    return irradiance;
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

float3 EvaluateGlimmerCoverage(GlimmerApply glimmer, float3 P, float3 N)
{
    float3 color = (float3)0.0;
    float remaining = 1.0;

    if (glimmer.params.y != 0u)
    {
        if (glimmer.probes.info.w != 0u)
        {
            [loop]
            for (uint probeLevel = 0; probeLevel < glimmer.probes.info.x && remaining > 1e-3; probeLevel++)
            {
                float3 levelIrradiance;
                const float weight = GlimmerSampleProbeLevel(glimmer.probes, probeLevel, P + N * 0.05, N, levelIrradiance);

                const float t = float(probeLevel) / float(max(GLIMMER_PROBE_LEVELS - 1, 1));

                color += lerp(float3(1.0, 0.85, 0.1), float3(1.0, 0.15, 0.05), t) * weight * remaining;
                remaining *= 1.0 - weight;
            }
        }

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
