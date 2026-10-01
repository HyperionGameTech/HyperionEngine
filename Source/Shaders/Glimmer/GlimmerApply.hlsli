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
#define GLIMMER_DEBUG_SH_BOUNCE 4

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

float3 EvaluateGlimmerSHDebug(GlimmerApply glimmer, float3 P, float3 N)
{
    const float3 missing = float3(1.0, 0.0, 1.0);

    if (glimmer.params.z == GLIMMER_DEBUG_SH_NEAREST)
    {
        const float skySeen = GlimmerSHNearestSkySeen(glimmer.sh, P, N);

        return skySeen >= 0.0 ? (float3)skySeen : missing;
    }

    float4 visibility;
    float4 bounce;
    const float coverage = GlimmerSHSampleVolume(glimmer.sh, P, N, visibility, bounce);

    if (coverage <= 0.0)
    {
        return missing;
    }

    if (glimmer.params.z == GLIMMER_DEBUG_SH_VISIBILITY)
    {
        return (float3)saturate(visibility.x + dot(visibility.yzw, N));
    }

    if (glimmer.params.z == GLIMMER_DEBUG_SH_BOUNCE)
    {
        const float skyLit = dot(bounce.rgb, float3(0.2126, 0.7152, 0.0722));

        return float3(skyLit, max(bounce.a, 0.0), 0.0);
    }

    return EvaluateGlimmerSH(glimmer.sh, P, N).rgb * glimmer.settings.x;
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
            float4 cascadeBounce;
            const float weight = GlimmerSHSampleCascade(glimmer.sh, shCascade, P + N * 0.05, N, cascadeVisibility, cascadeBounce);

            const float t = float(shCascade) / float(max(GLIMMER_SH_CASCADES - 1, 1));

            color += lerp(float3(0.0, 1.0, 1.0), float3(0.6, 0.1, 1.0), t) * weight * remaining;
            remaining *= 1.0 - weight;
        }
    }

    return color + float3(1.0, 0.0, 1.0) * remaining;
}

#endif
