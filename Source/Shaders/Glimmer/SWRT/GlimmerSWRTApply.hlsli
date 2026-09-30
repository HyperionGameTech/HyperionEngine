#ifndef GLIMMER_SWRT_APPLY_HLSLI
#define GLIMMER_SWRT_APPLY_HLSLI

#include "GlimmerProbeTypes.hlsli"

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_SWRT_APPLY_SAMPLING_HLSLI)
#define GLIMMER_SWRT_APPLY_SAMPLING_HLSLI

// Bound by GlimmerTechnique::BindApplyResources()
DECLARE_SRV(DeferredPass, GlimmerProbeSHTexture) Texture3D<float4> glimmerProbeSH;
DECLARE_SRV(DeferredPass, GlimmerProbeStateTexture) Texture3D<uint2> glimmerProbeState;
DECLARE_SRV(DeferredPass, GlimmerProbeBaseTexture) Texture2DArray<float> glimmerProbeBase;

#include "GlimmerProbes.hlsli"

/*! The near field: irradiance / pi from the probe clipmap at P, with its coverage as .a. */
float4 EvaluateGlimmerProbes(GlimmerProbeVolume volume, float3 P, float3 N)
{
    return SampleGlimmerProbes(volume, P + N * 0.05, N);
}

#endif
