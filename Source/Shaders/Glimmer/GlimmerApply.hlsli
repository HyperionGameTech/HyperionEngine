#ifndef GLIMMER_APPLY_HLSLI
#define GLIMMER_APPLY_HLSLI

// How lighting samples Glimmer. Included twice: first for the constants, then with GLIMMER_APPLY_WITH_SAMPLING once they are declared.
// The active technique's apply header (picked by the properties AddGlimmerApplyShaderProperties() adds) provides GlimmerTechniqueApply,
// the textures it samples, and EvaluateGlimmerTechnique().
#ifdef GLIMMER_TECHNIQUE_SH
#include "SH/GlimmerSHApply.hlsli"
#else
#include "SWRT/GlimmerSWRTApply.hlsli"
#endif

// Must match GlimmerApplyShaderData in GlimmerPass.hpp, followed by the technique's block
struct GlimmerApply
{
    uint4 params;    // x = 1 to show Glimmer's irradiance on its own, y = 1 when the technique can be sampled
    float4 settings; // x = intensity
    GlimmerTechniqueApply techniqueApply; // not `technique`, which HLSL reserves
};

#define GLIMMER_DEBUG_VIS_IRRADIANCE 1

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_APPLY_SAMPLING_HLSLI)
#define GLIMMER_APPLY_SAMPLING_HLSLI

#ifdef GLIMMER_TECHNIQUE_SH
#include "SH/GlimmerSHApply.hlsli"
#else
#include "SWRT/GlimmerSWRTApply.hlsli"
#endif

/*! Diffuse irradiance / pi from Glimmer at P, in the same units as the sky irradiance it stands in for. .a is its weight. */
float4 EvaluateGlimmer(GlimmerApply glimmer, float3 P, float3 N)
{
    if (glimmer.params.y == 0u)
    {
        return (float4)0.0;
    }

    float4 irradiance = EvaluateGlimmerTechnique(glimmer.techniqueApply, P, N);
    irradiance.rgb *= glimmer.settings.x;

    return irradiance;
}

#endif
