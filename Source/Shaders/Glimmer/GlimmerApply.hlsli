#ifndef GLIMMER_APPLY_HLSLI
#define GLIMMER_APPLY_HLSLI

#include "GlimmerProbeTypes.hlsli"

// Must match GlimmerApplyShaderData in GlimmerPass.hpp
struct GlimmerApply
{
    GlimmerProbeVolume volume;
    uint4 params; // x = debug vis
};

#define GLIMMER_DEBUG_VIS_IRRADIANCE 1

#endif

// The second include, after the probe textures are declared, adds the lookup
#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_APPLY_SAMPLING_HLSLI)
#define GLIMMER_APPLY_SAMPLING_HLSLI

#include "GlimmerProbes.hlsli"

/*! Diffuse irradiance / pi from Glimmer at P, in the same units as the sky irradiance it stands in for. .a is its weight. */
float4 EvaluateGlimmer(GlimmerApply glimmer, float3 P, float3 N)
{
    if (glimmer.volume.info.w == 0u)
    {
        return (float4)0.0;
    }

    float4 irradiance = SampleGlimmerProbes(glimmer.volume, P + N * 0.05, N);
    irradiance.rgb *= glimmer.volume.params.y;

    return irradiance;
}

#endif
