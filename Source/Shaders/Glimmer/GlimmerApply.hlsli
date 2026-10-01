#ifndef GLIMMER_APPLY_HLSLI
#define GLIMMER_APPLY_HLSLI

// How lighting samples Glimmer. Included twice: first for the constants, then with GLIMMER_APPLY_WITH_SAMPLING once they are declared.
// Two volumes make up Glimmer: SWRT probes for the near field, and SH voxels for the far field beyond them.
#include "SWRT/GlimmerSWRTApply.hlsli"
#include "SH/GlimmerSHApply.hlsli"

// Must match GlimmerApplyShaderData in GlimmerPass.hpp, followed by the probe volume and the SH volume (GlimmerTechnique::WriteApplyShaderData)
struct GlimmerApply
{
    uint4 params;    // x = GLIMMER_DEBUG_VIS_*, y = 1 when either volume can be sampled, z = GLIMMER_DEBUG_SH_*
    float4 settings; // x = intensity
    GlimmerProbeVolume probes;
    GlimmerSHVolume sh;
};

// Must match GlimmerDebugView in GlimmerCVars.hpp
#define GLIMMER_DEBUG_VIS_IRRADIANCE 1 // Glimmer's irradiance on its own
#define GLIMMER_DEBUG_VIS_COVERAGE 2   // which volume and cascade lighting takes it from

// Must match Rendering.Glimmer.DebugSH in GlimmerCVars.cpp: the SH voxels on their own, in place of the final image
#define GLIMMER_DEBUG_SH_IRRADIANCE 1 // their irradiance
#define GLIMMER_DEBUG_SH_VISIBILITY 2 // the sky they see, interpolated
#define GLIMMER_DEBUG_SH_NEAREST 3    // the sky they see, of the one voxel P is in
#define GLIMMER_DEBUG_SH_BOUNCE 4     // what their blockers bounce: red = lit by the sky, green = lit by the sun

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_APPLY_SAMPLING_HLSLI)
#define GLIMMER_APPLY_SAMPLING_HLSLI

#include "SWRT/GlimmerSWRTApply.hlsli"
#include "SH/GlimmerSHApply.hlsli"

/*! Diffuse irradiance / pi from Glimmer at P, in the same units as the sky irradiance it stands in for. .a is its weight. */
float4 EvaluateGlimmer(GlimmerApply glimmer, float3 P, float3 N)
{
    if (glimmer.params.y == 0u)
    {
        return (float4)0.0;
    }

    // the probes cover the near field and fade out at its edge; the SH voxels take over from there, so they're only sampled
    // where the probes leave something uncovered
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

        return float3(skyLit, skyLit * max(bounce.a, 0.0), 0.0);
    }

    return EvaluateGlimmerSH(glimmer.sh, P, N).rgb * glimmer.settings.x;
}

/*! Which volume and cascade EvaluateGlimmer takes its irradiance from at P: the probe blocks (SWRT, the near field) from yellow
 *  for the finest level to red for the coarsest, then the SH voxels (the far field) from cyan to violet. Colours mix where cascades
 *  blend, weighted as the lighting weights them; what no cascade covers is magenta. */
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

        // as in EvaluateGlimmer, the voxels only get what the probes leave
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
